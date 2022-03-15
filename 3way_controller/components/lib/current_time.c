#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "libconfig.h"
#include "libdecls.h"

/*
 * Originally I used SNTP to set the time, but ran into difficulty setting the time zone, since
 * there is no time-zone database on board, and our daylight savings time has to be updated for
 * each new year.  Then I found http://worldtimeapi.org/, which was developed just for IOT.  So
 * now we use that instead.
 */


void time_updater() {
    // Update the time once a day.
    // Arguably we don't even need to do that, since all we're concerned about is DST changes,
    // but 
    
    char databuf[512];
    while(1) {

        //get_internet_data("worldtimeapi.org", "api/ip.txt", databuf, sizeof(databuf)-1 );
        get_internet_data("example.com", "hello", databuf, sizeof(databuf)-1 );
        ESP_LOGI("time", "Received %d chars", strlen(databuf));
        ESP_LOGI("time","%s",databuf);

        // setenv("TZ", tz, 1);
        // tzset();

        // ask every 30 minutes; temp.
        vTaskDelay(1000 * 60 * 30 / portTICK_PERIOD_MS);
    }
}

void init_time() {
    // NOT WORKING.  TODO
    //xTaskCreate(time_updater, "time_updater", 4096, NULL, 5, NULL);
}

int current_hour() { 
    // Note if time updater has not run yet, this will be wrong.  That's acceptable.  
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return timeinfo.tm_hour;
}