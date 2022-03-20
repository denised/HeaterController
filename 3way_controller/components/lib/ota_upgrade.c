#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "libconfig.h"
#include "libdecls.h"

#define BUFFSIZE 1024

static const char *TAG = "ota";
static unsigned char ota_buffer[BUFFSIZE] = { 0 };

// forward decl
static int connect_to(const char *ipaddr);

/* 
 * Contact the named IP address on the OTA port to download and install a new version
 * of the code.
 * 
 * Derived from the ESP-IDF "native_ota_example"
 */

void ota_upgrade(const char *ipaddr, int expected_len)
{
    esp_err_t err;

    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    int sock;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        LOGE(TAG, "Unable to obtain partition to write to");
        return;
    }

    sock = connect_to(ipaddr);
    if ( sock < 0 ) {
        LOGE(TAG, "Unable to create connection to %s", ipaddr);
        return;
    }

    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        goto cleanup;
    }

    LOGI(TAG, "Starting OTA Update");

    int binary_file_length = 0;
    int data_read = 0;
    do {
        data_read = recv(sock, ota_buffer, sizeof(ota_buffer), 0);
        if (data_read < 0) {
            LOGE(TAG, "network read failed (%d)", data_read);
            goto cleanup;
        }
        else if (data_read > 0) {
            err = esp_ota_write( update_handle, (const void *)ota_buffer, data_read);
            if (err != ESP_OK) {
                LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                goto cleanup;
            }
            binary_file_length += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_length);
        }
    } while(data_read > 0);

    LOGI(TAG, "Total data length received: %d", binary_file_length);
    if (binary_file_length != expected_len) {
        LOGE(TAG, "Length does not match expected length %d, aborting.", expected_len);
        goto cleanup;
    }

    shutdown(sock, 0);
    close(sock);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return;
    }

    // Success!
    LOGI(TAG, "Preparing to restart system!");
    esp_restart();

cleanup:
    esp_ota_abort(update_handle);
    shutdown(sock, 0);
    close(sock);
}

/*
 * A small test to determine whether or not the boot worked.
 */
static bool diagnostic(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << DIAGNOSTIC_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    LOGI(TAG, "Diagnostics (5 sec)...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bool diagnostic_is_ok = gpio_get_level(DIAGNOSTIC_PIN);

    gpio_reset_pin(DIAGNOSTIC_PIN);
    return diagnostic_is_ok;
}

/* 
 * Perform startup check to see if we have successfully completed an OTA upgrade.
 */
void ota_check(void) {

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // run diagnostic function ...
            bool diagnostic_is_ok = diagnostic();
            if (diagnostic_is_ok) {
                LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
    else {
        LOGW(TAG, "Unable to get OTA information!.  Continuing with current boot");
    }
}


static int connect_to(const char *ipaddr) {

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        return sock;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(ipaddr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(OTA_PORT);

    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6)) != 0) {
        return -1;
    }

    return sock;
}
