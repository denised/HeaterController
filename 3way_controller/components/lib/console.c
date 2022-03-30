#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "libconfig.h"
#include "libdecls.h"

// Handle requests from the console

static const char *TAG = "console";

int recieve_command(void *buf, int len) {
    char *cbuf = (char *)buf, *cmd, *args;
    cbuf[len] = 0;
    LOGI(TAG, "Received command |%s|", cbuf);

    if (strlen(cbuf) == 0) {
        LOGI(TAG,"Empty command ignored");
        return 0;
    }

    // look for first space character to find if we have arguments.
    char *sp = strchr(cbuf, ' ');
    if (sp) { // split string in two.
        *sp = 0;
        cmd = cbuf;
        args = sp+1;
    }
    else {
        cmd = cbuf;
        args = "";
    }

    if ( strcmp(cmd, "hello") == 0 ) {
        send_message(0,"hello back");
    }
    else if ( strcmp(cmd, "version") == 0 ) {
        send_message(0,version_string);
    }
    else if ( strcmp(cmd, "level") == 0 ) {
        set_power_level( args );
    }
    else if ( strcmp(cmd, "bump") == 0 ) {
        int amount, duration, found;
        found = sscanf(args, " %d %d", &amount, &duration);
        if ( found == 2 ) {
            bump_temperature(amount, duration);
        }
        else {
            LOGI(TAG,"Malformed bump command? |%s|", args);
        }
    }
    else if ( strcmp(cmd, "update") == 0 ) {
        char addr[16];  // 123.456.777.999
        int len, found;
        found = sscanf(args, " %15s %d", addr, &len);
        if (found == 2) {
            ota_upgrade(addr, len);
        }
        else {
            LOGI(TAG,"Malformed update command? |%s|", args);
        }
    }
    else if ( strcmp(cmd, "schedule") == 0 ) {
        set_temperature_schedule(args);
    }
    else if ( strcmp(cmd, "reboot") == 0 ) {
        send_message(0,"Rebooting now...");
        esp_restart();
    }
    else if ( strcmp(cmd, "report") == 0 ) {
        send_messagef(0, "Errors since boot: %d", current_error_count());
        report_errors();
        report_temperature_schedule();
    }
    else if ( strcmp(cmd, "errtest") == 0 ) {
        // Generate a bunch of errors so we can see the behavior of the error handler
        for(int i=0; i<100; i++) {
            LOGE(TAG,"Test error %d", i);
        }
    }
    else {
        LOGI(TAG, "Unrecognized command %s", cmd);
    }

    return 0;
}

void init_console() {
    listener_task("console", CNTRL_PORT, recieve_command);
}

