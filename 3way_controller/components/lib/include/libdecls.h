
// Temperature listener
void temperature_listener_task();
int current_ambient_temperature();

// Power controller
void set_temperature_schedule( int temps[] );
void power_controller_start();
