// Get the temperature of the heater.
// Right now we are going to try using the onboard thermometer of the esp32c chip itself.
// This will give us a temperature reading "near" the heater.
// This is definitely what we want to compute the delta or flux of temperature between the heater
// and the remote temperature station.
// It may or may not be sufficient for safety purposes --- TBD.

// See https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/temp_sensor.html
// And {$IDF_SRC}/examples/peripherals/temp_sensor

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "driver/temp_sensor.h"

#include "libconfig.h"
#include "libdecls.h"

static char *TAG = "heater temp";

void heater_temp_reader_init() {
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    ESP_ERROR_CHECK(temp_sensor_start());
}

int current_heater_temperature() {
    float val;
    int err;
    err = temp_sensor_read_celsius(&val);
    ESP_ERROR_CHECK(err);
    if (err) {
        ESP_LOGW(TAG, "Unable to read heater temperature");
        return NO_TEMP_VALUE;
    }
    else {
        ESP_LOGI(TAG,"Heater temperature is %f", val);
        return (int)(val);
    }
}