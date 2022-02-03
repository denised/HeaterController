
// The value returned by current_temperature if there is no known / valid value.
#define NO_TEMP_VALUE -100

void temperature_listener_task(void *);
int current_temperature();
