#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "libconfig.h"
#include "libdecls.h"

// Handle requests from the console

static const char *TAG = "console";

// forward decl
int split_string(char *input, char **substrings, int maxsplits);
int argcheck(int expect, int have);


int recieve_command(void *buf, int len) {
    char *args[30];
    char *cbuf = (char *)buf;
    cbuf[len] = 0;
    LOGI(TAG, "Received command %s", cbuf);

    int nargs = split_string(buf, args, 30);

    char *cmd = args[0];
    if ( strcmp(cmd, "hello") == 0 ) {
        broadcast_message("hello back");
    }
    else if ( strcmp(cmd, "version") == 0 ) {
        broadcast_message(version_string);
    }
    else if ( strcmp(cmd, "level") == 0 ) {
        if (argcheck(1, nargs)) {
            set_power_level( args[1] );
        }
    }
    else if ( strcmp(cmd, "bump") == 0 ) {
        if (argcheck(2, nargs) ) {
            int amount = atoi(args[1]);
            int duration = atoi(args[2]);
            ESP_LOGI(TAG,"Args were %s and %s", args[1], args[2]);
            broadcast_messagef("Temperature bumped by %d for %d", amount, duration);
            bump_temperature(amount, duration);
        }
    }
    else if ( strcmp(cmd, "schedule") == 0 ) {
        if (argcheck(1, nargs)) { // We can accept up to 24 values, but need at least one.
            int i, lastval=0, newval, values[24];
            for(i=1; i<nargs && i<24; i++) {
                newval = atoi(args[i]);
                if (newval == 0) {
                    LOGW(TAG,"Badly formed command: %s is not a valid temperature",args[i]);
                    goto cmd_done;
                }
                values[i] = lastval = newval;
            }
            for(; i<24; i++) {
                values[i] = lastval;
            }
            set_temperature_schedule(values);
        }
    }
    else if ( strcmp(cmd, "update") == 0 ) {
        if (argcheck(2, nargs)) {
            ota_upgrade(args[1], atoi(args[2]));
        }
    }
    else if ( strcmp(cmd, "reboot") == 0 ) {
        broadcast_message("Rebooting now...");
        esp_restart();
    }
    else if ( strcmp(cmd, "report") == 0 ) {
        // TODO
    }
    else {
        LOGE(TAG, "Unrecognized command %s", cmd);
    }

cmd_done:
    return 0;
}

void init_console() {
    listener_task("console", CNTRL_PORT, recieve_command);
}


int split_string(char *input, char **substrings, int maxsplits) {
    // in-place split on space character: replaces space with NULL
    // and fills the substrings array with the pointers to each piece.
    // returns the number of substrings found
    // note: input must be a writable buffer!
    int len = strlen(input);
    int i, si, in_string = 0;
    for( i=0,si=0; i<len && si<maxsplits; i++ ) {
        if(input[i] == ' ') {
            input[i] = 0;
            in_string = 0;
        }
        else if ( !in_string ) {
            in_string = 1;
            substrings[si++] = input+i;
        }
    }
    return si;
}

int argcheck(int expect, int have) {
    // Check that we got as many tokens as we need.
    if (expect > have-1) {
        LOGE(TAG, "Malformed command?  Expected %d tokens, got %d", expect+1, have);
        return 0;      
    }
    return 1;
}