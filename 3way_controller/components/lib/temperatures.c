#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "driver/temp_sensor.h"
#include "libconfig.h"
#include "libdecls.h"

// We sense two different temperatures, using different methods:
// * The ambient temperature is the household temperature somewhat distant from the heater, read from a 
//   remote temperature sensor that broadcasts on WIFI.
// * The heater temperature is the temperature of the heater itself, reflective of how hot the oil is, 
//   is read from an onboard temperature sensor.
//
// For the ambient temperature, we dampen the sensor variability by keeping a running average.
// We also through away the data entirely if it is too old.


static char *TAG = "temps";

// We get temperatures at 1-minute intervals, so this smooths over 15 minute periods
#define HISTORY_LEN 15
static float ambient_history[HISTORY_LEN];
static int ahi = 0;
static int64_t ambient_timestamp = 0;

// Ambient temperature

void reset_ambient_history() {
    for(int i=0; i<HISTORY_LEN; i++) {
        ambient_history[i] = NO_TEMP_VALUE;
    }
    ahi = 0;
}

float current_ambient_temperature() {
    
    int64_t current_time = esp_timer_get_time();
    int64_t ts_delta = current_time - ambient_timestamp;

    if (ts_delta > READ_LIFETIME*1000LL) {
        int mins = ts_delta / (1000*1000*60);
        ESP_LOGW(TAG,"Stored temp %d out of date by %d minutes", current_time, mins);
        reset_ambient_history();
        return NO_TEMP_VALUE;
    }
    else {
        float val = 0;
        int count = 0;
        for(int i = 0; i<HISTORY_LEN; i++) {
            if (ambient_history[i] != NO_TEMP_VALUE) {
                val += ambient_history[i];
                count++;
            }
        }
        if (count == 0) {
            return NO_TEMP_VALUE;
        }
        else {
            return val/count;
        }
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
        LOGW(TAG, "Ambient temperature %f out of range; ignoring", val);
    }
    else {
        // store it, and remember when we last read it
        ambient_timestamp = esp_timer_get_time();
        ambient_history[ahi++] = val;
        if (ahi == HISTORY_LEN) {
            ahi = 0;
        }
    }
    return 0;
}


void report_ambient_history_values() {
    static char ahstring[(HISTORY_LEN * 8) + 10];

    memset(ahstring, ' ', (HISTORY_LEN*8)+9);
    // For the paranoid: if any of the values run over, they will be written over by the next in line, leading
    // to a garbled string, but not corrupted memory.  And the extra chars added to the buffer should prevent the
    // last one from overflowing, as well.
    for(int i=0; i<HISTORY_LEN; i++) {
        sprintf(ahstring+(i*8), "%6.2f, ", ambient_history[i]);
    }
    send_messagef(0, "ambient temperature history %s", ahstring);
}

// Heater temperature
//
// Use the onboard thermometer of the esp32c chip itself.
// This will give us a temperature reading "near" the heater.
// The main intent is to prevent the heater from being so hot that it will be a burn hazard.
//
// See https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/temp_sensor.html
// And {$IDF_SRC}/examples/peripherals/temp_sensor

float current_heater_temperature() {
    float val;
    int err;
    err = temp_sensor_read_celsius(&val);
    if (err) {
        LOGE(TAG, "Unable to read heater temperature");
        return NO_TEMP_VALUE;
    }
    return val;
}



// Do all initialization required.

void init_temps() {
    
    // initialize ambient history
    reset_ambient_history();

    // start ambient listener/updater
    listener_task("ambient", TEMPERATURE_PORT, receive_ambient_temperature);

    // initialize onboard sensor
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    esp_err_t err = temp_sensor_set_config(temp_sensor);
    if (err != ESP_OK) {
        LOGE(TAG, "temp sensor config failed (%s)", esp_err_to_name(err));
    }
    err = temp_sensor_start();
    if (err != ESP_OK) {
        LOGE(TAG, "temp sensor start failed (%s)", esp_err_to_name(err));
    }
}

