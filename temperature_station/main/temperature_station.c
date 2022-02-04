#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "addr_from_stdin.h"

/* Driver for temp reader */
#include "dht11.h"

/* ======== PARAMETERS ======== */
#define BROADCAST_IP_ADDR "10.0.0.255"
#define PORT 3339

#define GPIO_CONNECT GPIO_NUM_9

/* n minute delay between pings */
#define DELAY (1 * 60 * 1000 / portTICK_PERIOD_MS)
/* ===== =============== ====== */

static const char *TAG = "station";

static void weather_station(void *pvParameters)
{
    char payload[20];
    int optval = 1;
    int err; 

    while (1) {

        struct sockaddr_in dest;
        dest.sin_addr.s_addr = inet_addr(BROADCAST_IP_ADDR);
        dest.sin_family = AF_INET;
        dest.sin_port = htons(PORT);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof optval);
        if (err < 0) {
            ESP_LOGE(TAG, "Error setting port to broadcast");
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        while (1) {

            struct dht11_reading reading = DHT11_read();
            if (reading.status != DHT11_OK ) {
                ESP_LOGE(TAG, "Error occurred reading sensor: errno %d", errno);
            }
            else {
                ESP_LOGI(TAG, "Read %d, %d, %d", reading.status, reading.temperature, reading.humidity);
                // I suppose we should check for bizarre large numbers before overwriting the buffer?
                sprintf(payload, "%d %d", reading.temperature, reading.humidity);
                err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest, sizeof(dest));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
                ESP_LOGI(TAG, "Sent '%s'", payload);
            }
            vTaskDelay( DELAY );
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    DHT11_init(GPIO_CONNECT);

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(weather_station, "weather_station", 4096, NULL, 5, NULL);
}
