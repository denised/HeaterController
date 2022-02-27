#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "libconfig.h"
#include "libdecls.h"

// This is the code that actually does the controlling.
// 
// The controller pays attention to several sorts of inputs:
//   * the desired temperature (specified as a 24-hour schedule)
//   * the local (heater) temperature
//   * the ambient (remote) temperature
// and combines those to  determine whether the heater should be off, low, medium or high.

static char *TAG = "power controller";
static int temp_targets[24] = {19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,   // midnight -- 11am
                               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19 };  // noon -- 11pm
static int64_t last_update;

// float safe comparison
#define NO_T_VALUE(temp) (temp < NO_TEMP_VALUE + 0.1)


void set_temperature_schedule( int *new_temps ) {
    for(int i=0; i<24; i++) {
        int nt = new_temps[i];
        if ( nt < 10 || nt > 30 ) {
            ESP_LOGE(TAG,"Temperature target %d out of bounds; ignoring", nt);
        }
        else {
            temp_targets[i] = nt;
        }
    }
    ESP_LOGI(TAG,"Temperature targets updated");
}


void power_controller_loop() {
    float desired_temp, actual_temp, heater_temp;
    while(1) {
        // For now, just ignore time of day and use the first value
        desired_temp = temp_targets[0];
        actual_temp = current_ambient_temperature();
        heater_temp = current_heater_temperature();
        ESP_LOGI(TAG,"Desired temp %f, actual %f, heater %f", desired_temp, actual_temp, heater_temp);
        
        if ( heater_temp > MAX_HEATER_TEMPERATURE ) {
            ESP_LOGW(TAG, "Discontinuing heat, heater temperature is %f", heater_temp);
            gpio_set_level(LWATT_PIN, 0);
            gpio_set_level(HWATT_PIN, 0);
        }
        else if ( NO_T_VALUE(desired_temp) || NO_T_VALUE(actual_temp) ) {
            int64_t ts_delta = esp_timer_get_time() - last_update;
            ESP_LOGI(TAG, "Missing information: desired temp %f, actual_temp %f, heater_temp %f, last_updated %lld", desired_temp, actual_temp, heater_temp, ts_delta);
            if (ts_delta > FLYING_BLIND_DURATION) {
                ESP_LOGE(TAG, "No information to control with!");
            }
            ESP_LOGI(TAG, "Flying blind; default behavior: low heat");
            gpio_set_level(LWATT_PIN, 1);
            gpio_set_level(HWATT_PIN, 0);
        }
        else if ( actual_temp > desired_temp ) {
            ESP_LOGI(TAG, "Too warm; turn off");
            gpio_set_level(LWATT_PIN, 0);
            gpio_set_level(HWATT_PIN, 0);
        }
        else if ( desired_temp - actual_temp <= 2 ) {
            ESP_LOGI(TAG, "Just a little please");
            gpio_set_level(LWATT_PIN, 1);
            gpio_set_level(HWATT_PIN, 0);
        }
        else if ( desired_temp - actual_temp <= 5 ) {
            ESP_LOGI(TAG, "Medium");
            gpio_set_level(LWATT_PIN, 0);
            gpio_set_level(HWATT_PIN, 1);
        }
        else {
            ESP_LOGI(TAG,"Full blast!");
            gpio_set_level(LWATT_PIN, 1);
            gpio_set_level(HWATT_PIN, 1);
        }
        
        // Delay, in milliseconds.
        vTaskDelay(HEATER_UPDATE_FREQUENCY / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}



void power_controller_start()
{
    // Initialize stuff
    // Output pins
    gpio_config_t pin_conf = {
        .pin_bit_mask = ((1ULL << LWATT_PIN) | (1ULL << HWATT_PIN)),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pin_conf);

    last_update = esp_timer_get_time();

    // Go!
    xTaskCreate(power_controller_loop, "power_controller", 4096, NULL, 5, NULL);
}