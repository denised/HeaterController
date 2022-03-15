#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "libconfig.h"
#include "libdecls.h"


static char *TAG = "desired_temp";

static int temp_targets[24] = {19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,   // midnight -- 11am
                               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19 };  // noon -- 11pm

static int override_temp = NO_TEMP_VALUE;
static int64_t override_until = 0;


void set_temperature_schedule( int *new_temps, int temporary ) {
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

void bump_temperature(int increment, int hours) {
    // Note that multiple bumps are additive.
    float current_target = current_desired_temperature();
    override_temp = current_target + increment;
    // Note if hours <= 0, the effect will be to restore the regular schedule.
    // Non-intuitive, but useful, so I'm leaving it that way.
    // esp_timer tells time in *microseconds*.
    override_until = esp_timer_get_time() + (hours * 1000 * 1000 * 60 * 60);
    ESP_LOGI(TAG,"Bumped temperature from %f to %f until %lld", current_target, override_temp, override_until);
}

float current_desired_temperature() {
    if (override_until) {
        if (esp_timer_get_time() < override_until) {
            return override_temp;
        }
        else {
            // Mark override as expired.
            override_until = 0;
            // fall through
        }
    }
    return temp_targets[ current_hour() ];
}
