
// Temperature listener
void temperature_listener_task();
float current_ambient_temperature(); // in Celsius

// Power controller
void set_temperature_schedule( int temps[] );
void power_controller_start();

// Heater temperature
void heater_temp_reader_init();
float current_heater_temperature();