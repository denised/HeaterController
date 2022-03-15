#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "libconfig.h"
#include "libdecls.h"

// Listen for "commands" on a UDP port, and handle them when they arrive.

const char *TAG = "console";

// forward decl
int split_string(char *input, char **substrings, int maxsplits);
int argcheck(int expect, int have);


int recieve_command(void *buf, int len) {
    char *args[30];
    char *cbuf = (char *)buf;
    cbuf[len] = 0;
    ESP_LOGI(TAG, "Received command %s", cbuf);

    int nargs = split_string(buf, args, 30);

    char *cmd = args[0];
    if ( strcmp(cmd, "hello") == 0 ) {
        broadcast_message("hello back");
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
            bump_temperature(amount, duration);
        }
    }
    // TODO: other commands go here.
    else {
        ESP_LOGE(TAG, "Unrecognized command %s", cmd);
    }

    return 0;
}

void init_console() {
    listener_task("console", false, CNTRL_PORT, recieve_command, NULL);
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
        ESP_LOGE(TAG, "Malformed command?  Expected %d tokens, got %d", expect+1, have);
        return 0;      
    }
    return 1;
}