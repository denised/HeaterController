// This is the central point of configuation for all
// the hardware and preferences for this project.

// Port used by the temperature station
#define TEMPERATURE_PORT 3339

// Port used for the heater control interface
#define CNTRL_PORT 3337

// Port used by the heater for broadcast information
#define HEATER_PORT 3341

// Pin used to drive the lower wattage heater element
#define LWATT_PIN 7

// Pin used to drive the higher wattage heater element
#define HWATT_PIN 19

// The value used to denote if there is no known / valid temperature reading.
#define NO_TEMP_VALUE -100.0

// How long to trust the last-read temperature value for, before discarding it, 
// in microseconds
#define READ_LIFETIME (30 * 60 * 1000 * 1000)

// How long the heater controller should go without all the info it needs
// before declaring an error, in microseconds
// Note the error doesn't _do_ anything other than broadcast on HEATER_PORT
#define FLYING_BLIND_DURATION (30*60*1000*1000)

// Maximum heater temperature to tolerate, in Celsius
// The heater will be turned off if it reaches this temperature
#define MAX_HEATER_TEMPERATURE 55

// Frequency with which to check and update the heater control, in milliseconds
#define HEATER_UPDATE_FREQUENCY (30*1000)