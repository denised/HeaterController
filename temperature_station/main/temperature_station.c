#include <string.h>
#include <sys/param.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "protocol_examples_common.h"
#include "lwip/sockets.h"

/* ======== PARAMETERS ======== */
#define BROADCAST_IP_ADDR "10.0.0.255"
#define PORT 3339

#define ADC_CONNECT ADC1_CHANNEL_4  // connects on GPIO4

/* n minute delay between broadcasts */
#define DELAY (1 * 60 * 1000 / portTICK_PERIOD_MS)
/* ===== =============== ====== */

static const char *TAG = "temp_monitor";
static esp_adc_cal_characteristics_t adc_characteristics;
static bool calibration_enabled = false;


static float read_temperature() 
{
    // TODO: I'm not checking the "calibration_enabled" flag, since it works on my device and
    // I haven't been able to figure out what I would do if it _weren't_ enabled.  
    int raw_data, milliv;
    float temperature;

    // Get raw data
    raw_data = adc1_get_raw(ADC_CONNECT);
    ESP_LOGI(TAG, "Raw ADC data %d", raw_data);
    
    // convert first to voltage
    milliv = esp_adc_cal_raw_to_voltage(raw_data, &adc_characteristics);
    ESP_LOGI(TAG, "voltage: %d mV", milliv);

    // then to Celsius
    temperature = (milliv - 500) / 10.0;
    ESP_LOGI(TAG, "Temperature is %f", temperature);
    return temperature;
}

static void temperature_station(void *pvParameters)
{
    char payload[10];
    int optval = 1;
    int err; 
    float temperature;

    // Create a socket to broadcast on
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

        // While the socket is still working, keep using it
        while (1) {
            temperature = read_temperature();
            sprintf(payload, "%.2f", temperature);
            
            // broadcast
            err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest, sizeof(dest));
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Sent '%s'", payload);

            vTaskDelay( DELAY );
        }

        // if the socket breaks, shut down and restart.
        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void adc_init()
{
    // Initialize the Analog-to-Digital converter
    int ret;
    
    // The TMP36 temperature sensor (https://learn.adafruit.com/tmp36-temperature-sensor)
    // has an output voltage up to 2V.
    // The esp ADC can only read to about 1.1V.  So we must supply an attenuation to convert the
    // voltage to a usable range.  This one allows for a range up to 2.5V
    adc_atten_t attenuation = ADC_ATTEN_DB_11;

    // Set up calibration, if possible
    ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret == ESP_OK) {
        calibration_enabled = true;
        esp_adc_cal_characterize(ADC_UNIT_1, attenuation, ADC_WIDTH_BIT_DEFAULT, 0, &adc_characteristics);
    }
    else {
        ESP_LOGE(TAG, "Unable to initialize calibration, return code %d", ret);
    }

    // set up the ADC itself
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC_CONNECT, attenuation));
}

void app_main(void)
{
    // boiler-plate initialization required of all apps
    int ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Connect to WIFI.  This uses parameters in the sdkconfig file for SSID and password
    ESP_ERROR_CHECK(example_connect());

    // initialize the analog-to-digital converter
    adc_init();

    // and go
    xTaskCreate(temperature_station, "temperature_station", 4096, NULL, 5, NULL);
}
