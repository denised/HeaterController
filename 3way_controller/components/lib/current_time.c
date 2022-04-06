#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "libconfig.h"
#include "libdecls.h"

/*
 * Originally I used SNTP to set the time, but ran into difficulty setting the time zone, since
 * there is no time-zone database on board the ESP32, and our daylight savings time has to be updated for
 * each new year.  Then I found http://worldtimeapi.org/, which was developed just for IOT.  So
 * now we use that instead.
 */

const char *TAG = "time";
static char databuf[2048];
static char fieldbuf[30];

/*
 * Return a string representation of the current time.
 * Provided buf is used unless NULL, in which case a new string is malloc'd
 */
char *time_string(char *ob) {
    time_t now;
    time(&now);
    if (ob == NULL) {
        ob = malloc(30);
    }
    return ctime_r(&now, ob);
}

/* Copy the value of the field from databuf to fieldbuf.  Return -1 if not found. 
 * Fieldname should include the preceding newline and succeeding ':'
 */
int read_field(const char *fieldname) {
    bzero(fieldbuf, sizeof(fieldbuf));
    char *line = strstr(databuf, fieldname);
    if (line) {
        char *val = line + strlen(fieldname);
        char *end = strchr(val, '\n');
        if (end && (end-val) < sizeof(fieldbuf)) {
            memcpy(fieldbuf, val, (end-val));
            LOGI(TAG, "read field value as %s", fieldbuf);
            return 0;
        }
        else {
            LOGE(TAG, "Unable to find end of field %s", fieldname);
        }
    }
    else {
        LOGE(TAG, "Unable to find field %s", fieldname);
    }
    return -1;
}


int update_time() {
    bzero(databuf,sizeof(databuf));
    int err = get_internet_data("worldtimeapi.org", "/api/ip.txt", databuf, sizeof(databuf)-1 );
    if (err != ESP_OK) {
        LOGI(TAG,"Time fetch returned error %d", err)
    }
    err = 0;

    // Even if we have an error, we proceed, because sometimes we got the data we need anyway.   
    if (read_field("\nutc_offset:") == 0) {
        int utcval = atoi(fieldbuf);
        bzero(fieldbuf, sizeof(fieldbuf));
        // believe it or not, the TZ env wants the offset in the opposite direction from 
        // standard, so we have to negate it.
        sprintf(fieldbuf, "UTC%+d", -utcval);
        LOGI(TAG,"Setting TZ to %s", fieldbuf);
        setenv("TZ",fieldbuf,1);
        tzset();
    }
    else {
        err++;
    }

    if (read_field("\nunixtime:") == 0) {
        time_t ut = atol(fieldbuf);
        if ( ut > 0 ) {
            struct timeval tv = { .tv_sec = ut };
            settimeofday(&tv,NULL);

            time_string(fieldbuf);
            LOGI(TAG,"Time set to %s", fieldbuf);
        }
        else {
            LOGE(TAG,"Unable to parse time %s", fieldbuf);
            err++;
        }
    }
    else {
        err++;
    }
    return (err ? -1 : 0);
}

/* We might not be able to get the time due to an internet outage.
 * Keep trying (but not too often) until we succeed.
 */
void update_until_good() {
    while(1) {
        if ( update_time() == 0 ) {
            break;
        }
        vTaskDelay( 60 * 60 * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void init_time() {
    xTaskCreate(update_until_good, "time_updater", 4096, NULL, 5, NULL);
}

int current_hour() { 
    // Note if time updater has not run yet, this will be wrong.  That's acceptable.  
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return timeinfo.tm_hour;
}
