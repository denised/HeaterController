#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "protocol_examples_common.h"
#include "libconfig.h"
#include "libdecls.h"

static const char *TAG = "main";
const char *version_string = "Bumpity bump";

void app_main(void)
{
    ota_check();

    // Set up required system services.  If any of these fail, the code aborts.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Connect to WIFI using the ESP demo code, which seems good enough for us.
    // If the connect fails, the rest of the code will continue to function
    // in a default mode.
    if (example_connect() != ESP_OK) {
        ESP_LOGE(TAG,"Wifi connection failed!");
    }

    LOGI(TAG, "%s", version_string);

    // Initialize our code
    init_time();
    init_temps();
    init_console();
    init_broadcast_loop();

    // ...and go!
    power_controller_start();

    LOGI(TAG, "%s", "...Booting complete");
    vTaskDelete(NULL);
}

