#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/temp_sensor.h"
#include "libconfig.h"
#include "libdecls.h"

// We sense three different temperatures, using different methods:
// * The ambient temperature is the household temperature somewhat distant from the heater, read from a 
//   remote temperature sensor that broadcasts on WIFI.
// * The heater temperature is the temperature of the heater itself, reflective of how hot the oil is, 
//   is read from an onboard temperature sensor.
// * The outdoor temperature is fetched from the internet.
//
// For the two non-local temperatures, we keep track of how old the data is, and discard it if it is
// too old.


static char *TAG = "temps";

static float ambient_store = NO_TEMP_VALUE;
static int64_t ambient_timestamp = 0;

//static float outside_store = NO_TEMP_VALUE;
//static int64_t outside_timestamp = 0;

// Ambient temperature

float current_ambient_temperature() {
    
    int64_t current_time = esp_timer_get_time();
    int64_t ts_delta = current_time - ambient_timestamp;

    if (ts_delta > READ_LIFETIME) {
        int mins = ts_delta / (1000*1000*60);
        ESP_LOGW(TAG,"Stored temp %d out of date by %d minutes", current_time, mins);
        return NO_TEMP_VALUE;
    }
    else {
        return ambient_store;
    }
}

int receive_ambient_temperature(void *buf, int len) {
    // Null terminate and treat as string; we can do this safely because we know the underlying buffer
    // is longer than any data we should be recieving. (#bad_code_smell)
    char *cbuf = (char *)buf;
    cbuf[len] = 0;
    ESP_LOGI(TAG, "Received ambient temp %s", cbuf);

    // Extract the temperature (in Celsius)
    float val = atof(cbuf);
    if (val < 2 || val > 40) {
        ESP_LOGW(TAG, "Ambient temperature out of range; ignoring");
    }
    else {
        // We might want to add an error check that the temperature change isn't greater than we expect?
        int change = val - ambient_store;
        if (change && ambient_store != NO_TEMP_VALUE) {
            ESP_LOGI(TAG, "Ambient temperature change of %d degrees", change);
        }

        // store it, and remember when we last read it
        ambient_timestamp = esp_timer_get_time();
        ambient_store = val;
    }
    return 0;
}

// Heater temperature
//
// Right now we are going to try using the onboard thermometer of the esp32c chip itself.
// This will give us a temperature reading "near" the heater.
// The main intent is to prevent the heater from being so hot that it will be a burn hazard.
// Whether we can use this sensor for this is TBD.

// See https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/temp_sensor.html
// And {$IDF_SRC}/examples/peripherals/temp_sensor

float current_heater_temperature() {
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
        return val;
    }
}

// Outside temperature

float current_outside_temperature() {
    // TODO
    return NO_TEMP_VALUE;
}


// Do all initialization required.

void init_temps() {
    // ambient listener/updater
    listener_task("ambient", TEMPERATURE_PORT, receive_ambient_temperature);
    
    // onboard sensor
     temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    ESP_ERROR_CHECK(temp_sensor_start());
}

