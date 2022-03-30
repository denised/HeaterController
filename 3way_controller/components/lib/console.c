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
        broadcast_message("hello back");
    }
    else if ( strcmp(cmd, "version") == 0 ) {
        broadcast_message(version_string);
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
            LOGW(TAG,"Malformed bump command? |%s|", args);
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
            LOGW(TAG,"Malformed update command? |%s|", args);
        }
    }
    else if ( strcmp(cmd, "schedule") == 0 ) {
        set_temperature_schedule(args);
    }
    else if ( strcmp(cmd, "reboot") == 0 ) {
        broadcast_message("Rebooting now...");
        esp_restart();
    }
    else if ( strcmp(cmd, "report") == 0 ) {
        report_temperature_schedule();
        // TODO: report other things too.
    }
    else {
        LOGW(TAG, "Unrecognized command %s", cmd);
    }

    return 0;
}

void init_console() {
    listener_task("console", CNTRL_PORT, recieve_command);
}

