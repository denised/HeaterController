#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "libconfig.h"
#include "libdecls.h"

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

    // Note: if Wifi initialization fails, the following tasks will never do anything
    // interesting, and the controller will operate in a "default" state.

    broadcast_message("Booting...");

    init_time();
    init_temps();
    init_console();

    //power_controller_start();
}

