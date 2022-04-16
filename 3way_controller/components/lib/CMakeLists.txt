idf_component_register(SRC_DIR "."
    SRCS "network.c" "power_controller.c" "temperatures.c" "desired_temp.c" "current_time.c" "console.c" "ota_upgrade.c" "messages.c" "status_led.c"
    INCLUDE_DIRS "include"
    REQUIRES "app_update" "led_strip")
