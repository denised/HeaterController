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
// There are two levels of override:
// If the heater is too hot, it is turned off, period.
// Otherwise, if the power level has been explicitly set, that is used.
// Otherwise, the level is set based on the desired and existing temperatures.

enum power_level { power_off, power_low, power_medium, power_high, power_na };
static enum power_level power_level = power_low;
static enum power_level power_override = power_na;
static const char *TAG = "power controller";

// float safe comparison
#define NO_T_VALUE(temp) (temp < NO_TEMP_VALUE + 0.1)

/*
 * Set (or unset) override behavior for the heater.
 */
void set_power_level(char *level) {
    if ( strcmp(level,"auto") == 0 ) {
        power_override = power_na;
    }
    else {
        if ( strcmp(level, "off") == 0 ) {
            power_override = power_level = power_off;
        }
        else if ( strcmp(level, "low") == 0 ) {
            power_override = power_level = power_low;
        }
        else if ( strncmp(level, "med", 3) == 0 ) {
            power_override = power_level = power_medium;
        }
        else if ( strncmp(level, "hi", 2) == 0 ) {
            power_override = power_level = power_high;
        }
        else {
            LOGI(TAG, "Ignoring unrecognized power level %s", level);
            power_override = power_na;       
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
        LOGI(TAG,"Desired temp %f, actual %f, heater %f", desired_temp, actual_temp, heater_temp);
        
        if ( heater_temp > MAX_HEATER_TEMPERATURE ) {
            LOGI(TAG, "Discontinuing heat, heater temperature is %f", heater_temp);
            if ( heater_temp > MAX_HEATER_TEMPERATURE + 2 ) {
                LOGE(TAG, "Help! Heater is overheating!");
            }
            power_level = power_off;
        }
        else if ( power_override != power_na ) {
            LOGI(TAG, "Using assigned power level %d", power_override );
            power_level = power_override;
        }
        else if ( NO_T_VALUE(desired_temp) || NO_T_VALUE(actual_temp) ) {      
            LOGI(TAG, "Flying blind; maintain behavior: %d", power_level);
        }

        // I originally thought I'd have to do something more complicated than the following, but
        // this rather simple approach seems to be working for me so far.  It probably depends
        // a lot on things like insulation and air flow.

        else if ( actual_temp > desired_temp ) {
            LOGI(TAG, "Too warm; turn off");
            power_level = power_off;
        }
        else if ( desired_temp - actual_temp <= 0.2 ) {
            LOGI(TAG, "Just a little please");
            power_level = power_low;
        }
        else if ( desired_temp - actual_temp <= 2 ) {
            LOGI(TAG, "Medium");
            power_level = power_medium;
        }
        else {
            // At full power we usually exceed the max heater temperature fairly easily.
            // Rather than going through cycling between full power and no power, let's try 
            // to ease up before we hit that top temp.
            if ( heater_temp > MAX_HEATER_TEMPERATURE-1 ) {
                LOGI(TAG,"Hold up a little");
                power_level = power_medium;
            }
            else {
                LOGI(TAG,"Full blast!");
                power_level = power_high;
            }
        }

        switch(power_level) {
            case power_off:
                gpio_set_level(LWATT_PIN, 0);
                gpio_set_level(HWATT_PIN, 0);
                break;
            case power_low:
                gpio_set_level(LWATT_PIN, 1);
                gpio_set_level(HWATT_PIN, 0);
                break;
            case power_medium:
                gpio_set_level(LWATT_PIN, 0);
                gpio_set_level(HWATT_PIN, 1);
                break;  
            case power_high:
                gpio_set_level(LWATT_PIN, 1);
                gpio_set_level(HWATT_PIN, 1);
                break; 
            case power_na:
                // can't happen; only power_override can be set to power_na
                break;              
        }
        
        // Delay, in milliseconds.
        vTaskDelay(HEATER_UPDATE_INTERVAL / portTICK_PERIOD_MS);
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

    // Go!
    xTaskCreate(power_controller_loop, "power_controller", 4096, NULL, 5, NULL);
}
