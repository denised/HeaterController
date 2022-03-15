#include <stdlib.h>
#include <string.h>
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
 * Listener: Perpetually listen for incoming UDP or TCP connections on a specified port.  
 * This should be the last call made on an independent task.
 *
 * For UDP connections, the caller must provide a single callback to handle the received
 * message.
 *
 * For TCP connections, the caller must provide two callbacks.  The first callback is used for
 * the first buffer of a new message, and the second callback is called repeatedly until the
 * data is all consumed.  The second callback will be called a final time, with length zero, to
 * mark the end of the data, or with a negative length, to indicate an error occurred.  If
 * either callback returns a negative value this streaming process is interrupted (and the rest
 * of that message will be discarded).
 */

struct argsholder {
    char *taskname;
    int is_tcp; // false ==> udp connection
    int port;
    int (*callback)(void *, int);
    int (*streamback)(void *, int);
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
        int sock = socket(AF_INET, (args->is_tcp ? SOCK_STREAM : SOCK_DGRAM), 0);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d, task %s", errno, args->taskname);
            break;
        }

        int err = bind(sock, (struct sockaddr *)&listen_to, sizeof(listen_to));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d, task %s", errno, args->taskname);
            break;
        }

        if (args->is_tcp) {
            ESP_LOGE(TAG, "NOT IMPLEMENTED!  streaming isn't implemented yet");
        }
        while (1) {
            int received_len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);

            if (received_len < 0) {
                ESP_LOGE(TAG, "receive failed: errno %d, task %s", errno, args->taskname);
                break;
            }
            else {
                ESP_LOGI(TAG, "recieved %d bytes, task %s", received_len, args->taskname);
                args->callback(rx_buffer, received_len);
            }
        }

        ESP_LOGE(TAG, "Shutting down socket and restarting (task %s)", args->taskname);
        shutdown(sock, 0);
        close(sock);
    }

    ESP_LOGE(TAG, "Major error in task %s; aborting", args->taskname);
    vTaskDelete(NULL);
}


void listener_task(const char *taskname, int is_tcp, int port, int callback(void *, int), int streamback(void *, int)) {
    struct argsholder *args = malloc(sizeof *args);
    args->taskname = strcpy(malloc(strlen(taskname)+1), taskname);
    args->is_tcp = is_tcp;
    args->port = port;
    args->callback = callback;
    args->streamback = streamback;
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
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        sock = socket(res->ai_family, res->ai_socktype, 0);
        if(sock < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(sock);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        sprintf(buf, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, server);
        ESP_LOGI(TAG, "request:\n%s", buf);
        if (write(sock, buf, strlen(buf)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(sock);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout = { .tv_sec = 5 };
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(sock);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read network response */
        int i = 0, copy_len, r;
        int recv_len = sizeof(buf)-1;

        do {
            bzero(buf, sizeof(buf));
            r = read(sock, buf, recv_len);
            ESP_LOGI(TAG, "... read %d", r);
            if (r > 0) {
                copy_len = ( i+r<fb_len ? r : fb_len-i-1 );
                strncpy(fill_buffer+i, buf, copy_len);
                i += copy_len;
            }
        } while(r > 0 && i < fb_len);
        fill_buffer[i] = 0;

        ESP_LOGI(TAG, "... done reading from socket. read %d, errno %d.", i, errno);
        close(sock);
        break;
    }
}

/* 
 * Send a message on the broadcast port.  This activity operates inline (i.e. will block).
 * There's a potential race condition if multiple callers tried to broadcast before the socket
 * is initialized, but the result is probably(?) harmless.
 */

int broadcast_socket = -1;
struct sockaddr_in broadcast_dest;

void broadcast_message(char *message) {
    // initialize socket, if needed
    if (broadcast_socket < 0) {
        int optval = 1;
        broadcast_dest.sin_addr.s_addr = inet_addr(BROADCAST_IP_ADDR);
        broadcast_dest.sin_family = AF_INET;
        broadcast_dest.sin_port = htons(BROADCAST_PORT);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create broadcast socket: errno %d", errno);
            return;
        }
        if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof optval) < 0) {
            ESP_LOGE(TAG, "Error setting port to broadcast");
            shutdown(sock, 0);
            close(sock);
            return;
        }
        broadcast_socket = sock;
    }

    // do the actual broadcast
    if (sendto(broadcast_socket, message, strlen(message), 0, (struct sockaddr *)&broadcast_dest, sizeof(broadcast_dest)) < 0) {
        ESP_LOGE(TAG, "Error occurred during broadcast: errno %d", errno);
        shutdown(broadcast_socket, 0);
        close(broadcast_socket);
        broadcast_socket = -1;
    }
}