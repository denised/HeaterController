extern const char *version_string;
extern int broadcast_interval;

// Time of day (we only need the hour)
void init_time();
int current_hour();

// temperature sensing
float current_ambient_temperature(); // in Celsius
float current_heater_temperature();
void init_temps();

// temperature setting
void set_temperature_schedule(int *temps);
void bump_temperature(int increment, int hours);
float current_desired_temperature();

// command listener
void init_console();

// Power controller
void set_power_level(char *level);
void power_controller_start();

// OTA (Over the Air) upgrade
void ota_upgrade(const char *ipaddr, int expected_len);
void ota_check();

// Network actions
void listener_task(const char *taskname, int port, int callback(void *, int));
void get_internet_data(const char *server, const char *path, char *fill_buffer, int fb_len);
void broadcast_message(const char *message);
void broadcast_messagef(const char *fmt, ...);
void init_broadcast_loop();

// duplicating esp logging so we can also broadcast it
#define LOGE(tag,...) \
    ESP_LOGE(tag,__VA_ARGS__); \
    broadcast_messagef(__VA_ARGS__);

#define LOGW(tag,...) \
    ESP_LOGW(tag,__VA_ARGS__); \
    broadcast_messagef(__VA_ARGS__);

#define LOGI(tag,...) \
    ESP_LOGI(tag,__VA_ARGS__); \
    broadcast_messagef(__VA_ARGS__);

