
<img src=".images/topdrawing.svg" style="width:90%" alt="heater + microchip = happiness">


# Controller for a common 3-way electric heater

I love these heaters for their quietness, portability, and safety, but they don't do a very good job of determining when to turn on and off.  And they don't decide _at all_ what power level to use; you have to set that.  The result is sometimes it is too hot, sometimes too cold---even when the heater could do the job perfectly.  I have several of these heaters, and one of them stopped working one day, and so this project was born.

To make the heater perform better, I had two key objectives:
* Use a remote temperature sensor, for better accuracy.
* Have the heater automatically switch between different power levels.

I used two [ESP32-C3 boards](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/index.html), one for the heater and one for the temperature sensor, which communicate using the built-in WIFI capabilities of the board.  The code for both is in this project, and in the xxx section below I will walk through the hardware and software configuration for this project.

If you want to develop a controller for a heater, this code could serve as a starting point or inspiration.  I've also tried to separate different functions out so they can be understood and used independently.  But you would use them by copying the code into your own project, not as a "component" (in the ESP32 or Arduino sense).

## System Features

### Independent temperature station
This extremely simple project just attaches a temperature sensor to an ESP32-C3 and broadcasts the current temperature at intervals.

### Power Controller
The main function of the power controller is to determine which heater elements should be on.  Most of these heaters have two elements of two different wattages (mine are 650Watt and 850Watt), which can be turned on independently or together, resulting in three heating levels.  The logic decides what level to set based on how far the current temperature is from the desired temperature.  It also measures the temperature of the heater itself (using the onboard temp sensor on the ESP32 board itself) to keep the heater from getting too hot.

### Console 
Interaction with the heater controller is through the console, which is a very simple python program on the client side, and the matching module on the driver side.  Almost all communication is done via broadcast UDP, which greatly simplifies setup and management.  This works fine in a home WIFI environment like mine, but would obviously not be sufficient in larger or public networks.

Typing '?' in the python client will show a list of available commands.

### Desired Temperature Management
Among the things that are available in the console are three commands related to setting the desired temperature:
* Temperature schedule:  A 24-hour schedule of integer temperature values to model varying temperature desired throughout a day.  This is stored persistently on the board and is used as soon as the system boots.
* "Bump" operation: A temporary override of the schedule for the next n hours.  Just feeling a little chilly right now?  Bump the temperature for the next two hours!
* Manual set heater level: Just like the original heater controls, you can also just set the level you want directly.  (This is still subject to the overheating logic, however.)

### OTA Update
The ESP32 chips have a built-in capability to update "Over the Air" via WIFI.  This means you can modify the code, and then just load it directly to the heater, no cables required.  There are good sample demos of this capability with the Espressif docs; the only thing I did differently is use plain TCP to make the connection, rather than HTTPS.  This simplifies the code somewhat on both sides, but again is only appropriate on a private home WIFI network.

### Messaging
The heater controller responds to console requests and emits log messages over UDP.  The messages are queued from multiple tasks and sent by a single dedicated task.  The queuing code is designed for this multi-producer, single-consumer case, and also discards older messages rather than blocking, if the queue would overflow.

### Initial Time Setup
