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


static int max_heater_temperature = MAX_HEATER_TEMPERATURE;

int max_temperature() {
    return max_heater_temperature;
}
void set_max_temperature(int newval) {
    max_heater_temperature = newval;
}

/*
 * Parse a set of temperature values from a string.  We are expecting 24 comma-separated
 * integers (in reasonable ranges).  If there are too few values we repeat the last one to make
 * 24.  If any of the values are not integers in reasonable range we give up.
 * On success, 0 is returned and the new values are coped into vals.
 * On error, -1 is returned and vals is unchanged.  
 */
int parse_temperature_values( const char *sched, int *vals ) {
    int tvals[24];
    int i = 0;
    int v = 19; // if sched is empty, we'll fill with this value
    const char *cp = sched;

    // initially store values into tvals, so we don't mess up vals in case of error.
    while( i < 24 && cp && strlen(cp) ) {
        // atoi will take an integer off the front of the string.
        // it will return 0 if there is no integer value, which is conveniently out of range.
        v = atoi(cp);
        if ( v < 10 || v > 30 ) {  // celsius range; wider than really necessary.
            return -(i+1);
        }
        tvals[i++] = v;
        // jump to next comma, plus one
        cp = strchr(cp+1, ',');
        if (cp) cp++;
    }
    // pad out remaining values
    while( i < 24 ) {
        tvals[i++] = v;
    }
    // copy all of tvals to vals
    for( i=0; i<24; i++ ) {
        vals[i] = tvals[i];
    }
    return 0;
}

void set_temperature_schedule( const char *sched ) {

    LOGI(TAG,"parsing |%s|", sched);
    int ret = parse_temperature_values( sched, temp_targets );
    if ( ret < 0 ) {
        LOGI(TAG, "Malformed temperature schedule |%s| (token %d)", sched, -(ret-1));
    }
    else {
        LOGI(TAG, "Temperature schedule updated");
        // store permanently
        set_psv("ts", sched);
    }
}

void init_temperature_schedule() {
    // Fetch the temperature schedule we've stored before, if there is one.
    char *stored_schedule = get_psv("ts");
    if (stored_schedule) {
        LOGI(TAG, "Restoring schedule %s", stored_schedule);
        parse_temperature_values( stored_schedule, temp_targets );
    }
}

void report_temperature_schedule() {
    // debugging routine so we can see what the actual schedule is.
    for(int i = 0; i < 4; i++) {
        int j = i*6;
        send_messagef(0,"schedule %d, %d, %d, %d, %d, %d", 
            temp_targets[j], temp_targets[j+1], temp_targets[j+2],
            temp_targets[j+3], temp_targets[j+4], temp_targets[j+5]);
    }
}

/*
 * Temporarily modify the schedule.  For the next n hours, set the desired temperature
 * to the current temp + increment (which may be positive or negative).
 * Multiple bumps are additive.
 * An hour value of 0 or less will remove all current bumps.
 */
void bump_temperature(int increment, int hours) {
    float current_target = current_desired_temperature();
    override_temp = (int)(current_target + increment);
    // esp_timer tells time in *microseconds*.
    override_until = esp_timer_get_time() + ((int64_t)hours * 1000 * 1000 * 60 * 60);
    LOGI(TAG,"Bumped temperature from %f to %d until %lld", current_target, override_temp, override_until);
}


/*
 * Combine the schedule and any current override to determine what temperature we want right now.
 */
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
