#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "led_strip.h"
#include "protocol_examples_common.h"
#include "libconfig.h"
#include "libdecls.h"

static const char *TAG = "main";
const char *version_string = "Smooooth operator";
nvs_handle_t storage_handle;

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

    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &storage_handle));

    // Connect to WIFI using the ESP demo code, which seems good enough for us.
    // If the connect fails, the rest of the code will continue to function
    // in a default mode.
    if (example_connect() != ESP_OK) {
        LOGE(TAG,"Wifi connection failed!");
    }

    LOGI(TAG, "%s", version_string);

    // Initialize our code
    init_broadcast_loop();
    init_time();
    init_temps();
    init_console();
    init_temperature_schedule();

    // ...and go!
    power_controller_start();

    LOGI(TAG, "%s", "...Booting complete");
    vTaskDelete(NULL);
}


/*
 * Little bits of code that don't need their own files...
 */

/*
 *  Get and set persistent data
 */


char *get_psv(const char *key) {
    size_t len;
    int ret;
    // Getting key without memptr will fill in the length
    ret = nvs_get_str(storage_handle, key, NULL, &len);
    if ( ret == ESP_OK ) {
        char *val = malloc(len);
        nvs_get_str(storage_handle, key, val, &len);
        return val;
    }
    else if ( ret != ESP_ERR_NVS_NOT_FOUND ) {
        LOGE(TAG,"Fetch psv key (%s) error %d", key, ret);
    }
    return NULL;
}

void set_psv(const char *key, const char *newval) {
    int ret;
    ret = nvs_set_str(storage_handle, key, newval);
    if ( ret == ESP_OK ) {
        ret = nvs_commit(storage_handle);
        if ( ret != ESP_OK ) {
            LOGE(TAG, "NVS commit error %d", ret );
        }
    }
    else {
        LOGE(TAG, "Set psv key (%s)=(%s) error %d", key, newval, ret);
    }
}
