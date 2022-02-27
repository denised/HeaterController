#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "protocol_examples_common.h"

// Our code starts here.
#include "libconfig.h"
#include "libdecls.h"

#define TAG "Main"

void init_time() {
    // Start sntp time synchronization
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    setenv("TZ", LOCAL_TIME_ZONE, 1);
    tzset();
}

void app_main(void)
{
    // Set up and connect to WIFI
    // We use the simple "example_connect" code used by the WIFI examples; 
    // it seems to be good enough for us.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    // Note: if Wifi initialization fails, the following independent tasks will never do anything
    // interesting, and the controller will operate in a "default" state.

    xTaskCreate(temperature_listener_task, "temp_listener", 4096, NULL, 5, NULL);
    heater_temp_reader_init();
    init_time();

    // Start the main controller
    power_controller_start();
}

