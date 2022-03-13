
// temperature sensing
float current_ambient_temperature(); // in Celsius
float current_heater_temperature();
float current_outside_temperature();
void init_temps();


// Power controller
void set_temperature_schedule( int temps[] );
void power_controller_start();


// Network listener

void listener_task(const char *taskname, int is_tcp, int port, int callback(void *, int), int streamback(void *, int));