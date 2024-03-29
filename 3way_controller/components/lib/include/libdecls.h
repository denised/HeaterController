extern const char *version_string;      // main.c

// read/write persistent storage values
char *get_psv(const char *key);
void set_psv(const char *key, const char *newval);

// Time of day funtions
void init_time();
int update_time();
int current_hour();
char *time_string(char *buf);

// temperature sensing
float current_ambient_temperature(); // in Celsius
float current_heater_temperature();
void init_temps();
void report_ambient_history_values();

// temperature setting
int max_temperature();
void set_max_temperature(int val);
void init_temperature_schedule();
void set_temperature_schedule(const char *sched);
void bump_temperature(int increment, int hours);
float current_desired_temperature();
void report_temperature_schedule();

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
int get_internet_data(const char *server, const char *path, char *fill_buffer, int fb_len);
void init_broadcast_loop();

// led status
void init_status_led();
void update_status_led();


// message and error management
void send_message(int severity,const char *message);
void send_messagef(int severity,const char *fmt, ...);
int process_message_queue(int sock, void *sa);
int error_count();
int new_error_count();
void report_errors();

// duplicating ESP logging so we can also send and log it
#define LOGE(tag,...) \
    ESP_LOGE(tag,__VA_ARGS__); \
    send_messagef(2,__VA_ARGS__);

#define LOGW(tag,...) \
    ESP_LOGW(tag,__VA_ARGS__); \
    send_messagef(1,__VA_ARGS__);

#define LOGI(tag,...) \
    ESP_LOGI(tag,__VA_ARGS__); \
    send_messagef(0,__VA_ARGS__);

