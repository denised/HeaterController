#include <stdlib.h>
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "libconfig.h"
#include "libdecls.h"

// A simple common implementation of the low-level networking stuff so it doesn't 
// clutter the higher-level code.  There are lots of assumptions, like single only a single client,
// no two-way communications needed, etc., built in here.
//
// For UDP connections, the caller must provide a single callback to handle the received message.
//
// For TCP connections, the caller must provide two callbacks.  The first callback is used for
// the first buffer of a new message, and the second callback is called repeatedly until the data 
// is all consumed.  The second callback will be called a final time, with length zero, 
// to mark the end of the data, or with a negative length, to indicate an error occurred.
// If either callback returns a negative value this streaming process is interrupted
// (and the rest of that message will be discarded).

struct argsholder {
    char *taskname;
    int is_tcp; // false ==> udp connection
    int port;
    int (*callback)(void *, int);
    int (*streamback)(void *, int);
};

#define TAG "listener"
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