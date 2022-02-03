#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "tlistener.h"

static char *TAG = "temp listener";

// How long to trust the last-read value for, before discarding it, in microseconds
#define READ_LIFETIME (30 * 60 * 1000 * 1000)
static int temp_store = NO_TEMP_VALUE;
static int64_t temp_read = 0;

int current_temperature() {
    
    int64_t current_time = esp_timer_get_time();
    int64_t ts_delta = current_time - temp_read;

    if (ts_delta > READ_LIFETIME) {
        int mins = ts_delta / (1000*1000*60);
        ESP_LOGW(TAG,"Stored temp %d out of date by %d minutes", current_time, mins);
        return NO_TEMP_VALUE;
    }
    else {
        return temp_store;
    }
}

// Code taken pretty much directly from protocols/sockets/udp_server example

void temperature_listener_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int port = (int)pvParameters;
    struct sockaddr_in listen_to = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };

    while (1) {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        int err = bind(sock, (struct sockaddr *)&listen_to, sizeof(listen_to));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            break;
        }

        while (1) {
            struct sockaddr sender_addr;
            socklen_t salen = sizeof(sender_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, &sender_addr, &salen);

            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            else {
                // Get the sender's ip address as string for logging
                inet_ntoa_r(((struct sockaddr_in *)&sender_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %s", rx_buffer);

                // Extract the temperature (in Celsius)
                int val = atoi(rx_buffer);
                if (val < 2 || val > 40) {
                    ESP_LOGW(TAG, "Temperature out of range; ignoring");
                }
                else {
                    // We might want to add an error check that the temperature change isn't greater than we expect?
                    int change = val - temp_store;
                    if (change) {
                        ESP_LOGI(TAG, "Temperature change of %d degrees", change);
                    }

                    // store it, and remember when we last read it
                    temp_read = esp_timer_get_time();
                    temp_store = val;
                    ESP_LOGI(TAG, "Set time read to %lld", temp_read);
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}