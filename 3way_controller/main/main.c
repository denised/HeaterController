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
const char *version_string = "Here comes the LED";
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

/*
 * Show status via the on-board LED
 * We switch the LED between different colors to show different status:
 * 
 * Normal operation: blue
 * Booted within the last 10 minutes: green
 * There have been errors since last report: yellow
 * There have been new errors in the last 30 minutes: red
 * 
 * The timing of the color change is driven by the caller --- at this point I'm
 * just piggy-backing it on top of the messages task, so it gets the message
 * interval.
 * 
 * This code uses a hard-to-discover extra IDF component: IDF_PATH/examples/common_components/led_strip.
 * Examples of using it can be found in the IDF examples  get-started/blink  and   peripherals/rmt/led_strip
 * This post was useful:
 * https://www.electronics-lab.com/deep-dive-on-controlling-led-with-esp32-c3-devkitm-1-development-board-using-esp-idf/
 */

#define LED_RMT_CHANNEL 0   // you can chose 0-3; doesn't seem to matter which
#define LED_PIN 8           // for this board, this is hard-wired
#define LED_REFRESH_TIME 50 // milliseconds

static led_strip_t *status_led;

static const int64_t boot_warning_duration = (10LL * 60 * 60 * 1000 * 1000);
static const int64_t new_error_limit = (30LL * 60 * 60 * 1000 * 1000);
static int last_error_count = 0;
static int64_t last_error_stamp = 0;
static int odd_even = 0;

void init_status_led() {

    status_led = led_strip_init(LED_RMT_CHANNEL, LED_PIN, 1);
    status_led->clear(status_led, LED_REFRESH_TIME);

    last_error_count = new_error_count();
    last_error_stamp = esp_timer_get_time();
}


void led_color_pulse(int red, int green, int blue) {
    if (odd_even) {
        status_led->set_pixel(status_led, 0, red, green, blue);
    }
    else {
        // instead of blinking the led entirely, we alter intensity
        status_led->set_pixel(status_led, 0, red/3, green/3, blue/3);        
    }
    status_led->refresh(status_led, LED_REFRESH_TIME);
    odd_even = 1 - odd_even;
}

void update_status_led() {

    int updated_error_count = new_error_count();
    int64_t now_stamp = esp_timer_get_time();

    if ( updated_error_count > 0 ) {
        if ( updated_error_count > last_error_count ) {
            last_error_count = updated_error_count;
            last_error_stamp = now_stamp;
        }
        
        if ( now_stamp-last_error_stamp < new_error_limit ) {
            led_color_pulse(255, 10, 0);
        }
        else {
            led_color_pulse(180, 180, 0);
        }
    }
    else {
        // if updated error count == 0 then either there's never been an
        // error, or we've done a reset.  either way, clear the previous error counters.
        last_error_count = 0;
        last_error_stamp = 0;

        if ( now_stamp < boot_warning_duration ) {
            led_color_pulse(0, 230, 20);
        }
        else {
            led_color_pulse(0, 50, 230);
        }
    }
}