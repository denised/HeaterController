#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "libconfig.h"
#include "libdecls.h"

// This is the code that actually does the controlling.
// 
// The controller pays attention to several sorts of inputs:
//   * the desired temperature
//   * the local (heater) temperature
//   * the ambient (remote) temperature
// and combines those to  determine whether the heater should be off, low, medium or high.

static char *TAG = "power controller";
static int64_t last_update;


enum power_level { power_off, power_low, power_medium, power_high };
enum power_level power_level = power_off;
int power_override = 0;


// float safe comparison
#define NO_T_VALUE(temp) (temp < NO_TEMP_VALUE + 0.1)

void set_power_level(char *level) {
    if ( strcmp(level,"auto") == 0 ) {
        power_override = 0;
    }
    else {
        power_override = 1;
        if ( strcmp(level, "low") == 0 ) {
            power_level = power_low;
        }
        else if ( strncmp(level, "med", 3) == 0 ) {
            power_level = power_medium;
        }
        else if ( strncmp(level, "hi", 2) == 0 ) {
            power_level = power_high;
        }
        else {
            ESP_LOGW(TAG, "Ignoring unrecognized power level %s", level);
            power_override = 0;       
        }
    }
}

void power_controller_loop() {
    float desired_temp, actual_temp, heater_temp;

    // Delay a bit to let various initializations have a go.
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    while(1) {
        desired_temp = current_desired_temperature();
        actual_temp = current_ambient_temperature();
        heater_temp = current_heater_temperature();
        ESP_LOGI(TAG,"Desired temp %f, actual %f, heater %f", desired_temp, actual_temp, heater_temp);
        
        if ( heater_temp > MAX_HEATER_TEMPERATURE ) {
            ESP_LOGW(TAG, "Discontinuing heat, heater temperature is %f", heater_temp);
            gpio_set_level(LWATT_PIN, 0);
            gpio_set_level(HWATT_PIN, 0);
        }
        else if ( NO_T_VALUE(desired_temp) || NO_T_VALUE(actual_temp) ) {      
            // We check how long it has been since we have gotten real data, and if it has been
            // too long, issue a error.  Note this doesn't alter the behavior of the heater though:
            // whenever we have no information, we go with the default setting.
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
