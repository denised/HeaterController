#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "libconfig.h"
#include "libdecls.h"

/*
 * Simple implementation of the low-level networking stuff so it doesn't 
 * clutter the higher-level code.  There are lots of assumptions, like single only a single client,
 * no two-way communications needed, etc., built in here.
 * Three methods:
 *     listener_loop: passive connection listener
 *     get_internet_data: outbound fetch of URL.
 *     broadcast_message: broadcast UDP message
 */

/*
 * Listener: Perpetually listen for incoming UDP connections on a specified port.  
 * This function is parameterized by a callback that actually handles the data received,
 * and it runs as a task loop (so it should be the last thing called in an independent
 * vTask)
 */

struct argsholder {
    char *taskname;
    int port;
    int (*callback)(void *, int);
};

static char *TAG = "network";
#define BUFLEN 1048


void listener_loop(struct argsholder *args) {
    unsigned char rx_buffer[BUFLEN];
    struct sockaddr_in listen_to = {
            .sin_family = AF_INET,
            .sin_port = htons(args->port),
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };

    while (1) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            LOGE(TAG, "Unable to create socket: errno %d, task %s", errno, args->taskname);
            break;
        }

        int err = bind(sock, (struct sockaddr *)&listen_to, sizeof(listen_to));
        if (err < 0) {
            LOGE(TAG, "Socket unable to bind: errno %d, task %s", errno, args->taskname);
            break;
        }

        while (1) {
            int received_len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);

            if (received_len < 0) {
                LOGE(TAG, "receive failed: errno %d, task %s", errno, args->taskname);
                break;
            }
            else {
                args->callback(rx_buffer, received_len);
            }
        }

        LOGW(TAG, "Shutting down socket and restarting (task %s)", args->taskname);
        close(sock);
    }

    LOGE(TAG, "Major error in task %s; aborting", args->taskname);
    vTaskDelete(NULL);
}


void listener_task(const char *taskname, int port, int callback(void *, int)) {
    struct argsholder *args = malloc(sizeof *args);
    args->taskname = strcpy(malloc(strlen(taskname)+1), taskname);
    args->port = port;
    args->callback = callback;
    xTaskCreate((TaskFunction_t)listener_loop, taskname, 4096, args, 5, NULL);
}


/* 
 * Fetch data from the internet (http).  This operates inline (i.e. in the
 * same task) and will block until complete.  Fills the buffer supplied by the caller.
 * If the actual data was longer than that, the rest is discarded.
 */

void get_internet_data(const char *server, const char *path, char *fill_buffer, int fb_len) {
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    int sock;
    char buf[256];

    while(1) {
        int err = getaddrinfo(server, "80", &hints, &res);

        if(err != 0 || res == NULL) {
            LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        sock = socket(res->ai_family, res->ai_socktype, 0);
        if(sock < 0) {
            LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if(connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
            LOGE(TAG, "... socket connect failed errno %d", errno);
            close(sock);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        freeaddrinfo(res);

        sprintf(buf, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, server);
        LOGI(TAG, "request message:\n%s", buf);
        if (write(sock, buf, strlen(buf)) < 0) {
            LOGE(TAG, "... socket send failed");
            close(sock);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        struct timeval receiving_timeout = { .tv_sec = 5 };
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
            LOGE(TAG, "... failed to set socket receiving timeout");
            close(sock);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Read network response */
        int i = 0, copy_len, r;
        int recv_len = sizeof(buf)-1;

        do {
            bzero(buf, sizeof(buf));
            r = read(sock, buf, recv_len);
            ESP_LOGD(TAG, "... read %d", r);
            if (r > 0) {
                copy_len = ( i+r<fb_len ? r : fb_len-i-1 );
                strncpy(fill_buffer+i, buf, copy_len);
                i += copy_len;
            }
        } while(r > 0 && i < fb_len);
        fill_buffer[i] = 0;

        LOGI(TAG, "... done reading from socket. read %d, errno %d.", i, errno);
        close(sock);
        break;
    }
}

/* 
 * Send a message on the broadcast port.  Initially the message is just queued to
 * send.  A separate task handles the actual sending.  Messages are limited
 * to BROADCAST_MESSAGE_LEN in size.
 * 
 * Implementation Notes:
 * 
 * The reason for the queue/send design is so that only a single task has access
 * to the actual socket (since sockets/ports cannot be shared across tasks).
 * 
 * There are two queues so that one is always available to other tasks,
 * while the other might be in process with the broadcast loop below.
 * 
 * We don't worry about mutex issues regarding access to the queues because this
 * is an RT system which won't switch tasks until they block, and all the queue
 * manipulation is non-blocking.
 * 
 * If a queue would overflow before the broadcast loop gets to it, we start 
 * filling it round-robin (newer messages overwrite older ones).  Messages can
 * get lost that way.   If that happens in practice, increase the queue size or
 * decrease the broadcast loop interval.
 * 
 * The round-robin handling is also why we copy messages into a fixed block
 * of messages rather than allocating messages individually --- this way we don't
 * have to handle de-allocation and/or potential leaks.
 *
 * I chose to do my own pointer arithmetic because I find it easier to read than
 * trying to do the correct casts on 2-D arrays.  Hence the single block
 * of characters for each queue.  YMMV.
 * 
 */

static char mq1[BROADCAST_QUEUE_SIZE * BROADCAST_MESSAGE_LEN];
static char mq2[BROADCAST_QUEUE_SIZE * BROADCAST_MESSAGE_LEN];
static char *filling_queue = mq1;     // current queue to fill
static int mqi = 0;        // current open slot in filling queue
static int mq_wrapped = 0; // have we started round-robining yet?


/*
 * Broadcast message on broadcast port.
 * This function actually only enqueues the message to be sent later.
 */
void broadcast_message(const char *message) {

    // Round robin if necessary
    if (mqi == BROADCAST_QUEUE_SIZE) {
        mqi = 0;
        mq_wrapped = 1;
    }
    int len = strlen(message);
    if (len >= BROADCAST_MESSAGE_LEN) {
        len = BROADCAST_MESSAGE_LEN - 1;
    }
    int offset = mqi*BROADCAST_MESSAGE_LEN;

    memcpy( filling_queue+offset, message, len );
    *(filling_queue + offset + len) = 0; // null terminate string
    
    mqi++;
}

/*
 * Formatting version
 */
void broadcast_messagef(const char *fmt, ...) {

    if (mqi == BROADCAST_QUEUE_SIZE) {
        mqi = 0;
        mq_wrapped = 1;
    }
    int offset = mqi*BROADCAST_MESSAGE_LEN;

    va_list args;
    va_start(args, fmt);
    vsnprintf( filling_queue+offset, BROADCAST_MESSAGE_LEN, fmt, args );
    va_end(args);
    
    mqi++;
}


/*
 * This is the function that actually takes the queued messages and sends
 * them;  Called from within broadcaster_loop.
 * Returns a negative value if something bad happens.
 */
int send_queue(int sock, struct sockaddr_in *sa) {

    // Check if we have anything to do at all
    if ( mqi > 0 || mq_wrapped ) {

        // get our own copy of the queue and counters
        int i, slen, mqtop = (mq_wrapped ? BROADCAST_QUEUE_SIZE : mqi);
        int didoverflow = mq_wrapped;
        char *processing_queue = filling_queue, *msg;

        // Swap the queues so any new messages come in on the other queue.
        // We aren't worried about mutex here because FreeRTOS won't interrupt
        // us until we block.
        filling_queue = (processing_queue == mq1 ? mq2 : mq1);
        mqi = 0;
        mq_wrapped = 0;

        if (didoverflow) {
            // "recursive" broadcast is okay b/c/ we are just queuing it...
            LOGE(TAG, "Broadcast queue overflowed!  Some messages will be missing.");
        }

        ESP_LOGI(TAG, "Sending %d messages", mqtop);
        for(i=0; i<mqtop; i++) {
            msg = (processing_queue + i*BROADCAST_MESSAGE_LEN);
            slen = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)sa, sizeof(struct sockaddr_in));
            if (slen < 0) {
                ESP_LOGE(TAG, "Error occurred during broadcast: errno %d", errno);
                return slen;
            }
        }
    }
    return 0;
}



// This can be dynamically modified
int broadcast_interval = (5 * 1000);


/* 
 * This behaves just like the listener loop, except that it is a sender,
 * and the action is hard-wired, not parameterizable.
 */
void broadcast_loop() {
    int sock;
    int ret, optval = 1;
    struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(BROADCAST_PORT),
            .sin_addr.s_addr = inet_addr(BROADCAST_IP_ADDR)
        };

    while (1) {
        // initialize socket
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create broadcast socket: errno %d", errno);
            break;
        }
        if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof optval) < 0) {
            ESP_LOGE(TAG, "Error setting broadcast mode: errno %d", errno);
            break;
        }

        // use socket until it breaks
        // note if it breaks in the middle of a queue, we'll loose some messages;
        // ok for now
        do {
            vTaskDelay(broadcast_interval  / portTICK_PERIOD_MS);
            ret = send_queue(sock, &addr);
        } while (ret >= 0);

        ESP_LOGI(TAG, "Shutting down broadcast socket and restarting");
        close(sock);
    }

    ESP_LOGE(TAG, "Major error in broadcast_loop; aborting");
    vTaskDelete(NULL);
}

void init_broadcast_loop() {
    xTaskCreate(broadcast_loop, "broadcast_loop", 4096, NULL, 5, NULL);    
}
