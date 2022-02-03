
// This is the interface that the controller uses to interact with everything
struct power_controller_config {
    // There are two switches, the first controlling a 650W element, the second an 850W element
    // These are the connectors for them.
    int low_watt_switch;
    int high_watt_switch;

    // This function returns the current ambient temperature, in Celsius
    int (*get_ambient_temperature)(void);

    // This function returns the internal temperature of the heater, in Celsius
    // or -1, if no temperature sensor is being used.
    int (*get_heater_temperature)(void);
};

void set_temperature_schedule( int temps[] );
void power_controller_start(struct power_controller_config *config);

