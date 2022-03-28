// This is the central point of configuation for 
// the 3way_controller project.  Some values are repeated in
// the temperature_station and/or console.py

// Port used by the temperature station
#define TEMPERATURE_PORT 3339

// Port used for the heater console interface
#define CNTRL_PORT 3337

// Port used to perform OTA update
#define OTA_PORT 3343

// Port used by the heater to broadcast information
#define BROADCAST_PORT 3341

// Address to brodcast to; usually this would be an "all" address
#define BROADCAST_IP_ADDR "10.0.0.255"


// Maximum number of messages to queue for broadcast
#define BROADCAST_QUEUE_SIZE 32

// Maximum length of messages to be broadcast
#define BROADCAST_MESSAGE_LEN 256

// Note: the message buffers are pre-allocated, requiring a block of
// 2 * BROADCAST_QUEUE_SIZE * BROADCAST_QUEUE_LEN  characters


// Pin used to drive the lower wattage heater element
#define LWATT_PIN 6

// Pin used to drive the higher wattage heater element
#define HWATT_PIN 10

// Pin used to perform diagnostics on update.  This pin isn't used for anything
// and isn't connected to anything.
#define DIAGNOSTIC_PIN 4

// The value used to denote if there is no known / valid temperature reading.
#define NO_TEMP_VALUE -100.0

// How long to trust the last-read temperature value for, before discarding it, 
// in milliseconds
#define READ_LIFETIME (30 * 60 * 1000)

// Maximum heater temperature to tolerate, in Celsius
// The heater will be turned off if it reaches this temperature
#define MAX_HEATER_TEMPERATURE 55

// Frequency with which to check and update the heater control, in milliseconds
#define HEATER_UPDATE_INTERVAL (30*1000)
