
// Time of day (we only need the hour)
void init_time();
int current_hour();

// temperature sensing
float current_ambient_temperature(); // in Celsius
float current_heater_temperature();
float current_outside_temperature();
void init_temps();

// temperature setting
void set_temperature_schedule(int *temps, int temporary);
void bump_temperature(int increment, int hours);
float current_desired_temperature();

// command listener
void init_console();

// Power controller
void set_power_level(char *level);
void power_controller_start();

// Network actions
void listener_task(const char *taskname, int is_tcp, int port, int callback(void *, int), int streamback(void *, int));
void get_internet_data(const char *server, const char *path,  char *fill_buffer, int fb_len);
void broadcast_message(char *message);