# MAIN README

NodeMCU V3 electric meter pulse measuring
=========================================

This Arduino sketch is made for collecting KWh and kVAr LED pulses from a "smart" electric power meter. Specifically, this project has been tested on a Kaifa (MA304H3E 3-phase) meter. By measuring the time between pulses, active and reactive power is calculated. Also, it is possible to keep track of the total consumed electric energy, see below.

Note: The program is not designed for systems where power is generated inside the circuit measured by the meter.

Three breakout boards are needed to complete this rather simple project. Please see the hardware list.

Communication is based on the Websocket protocol (ws). To connect from the outside world, ports 80 and 81 need to be forwarded in your router.

Several clients can connect to the NodeMCU module (server) at the same time.

Hardware
--------
* LoLin NodeMCU V3 breakout board
* 2 pc LM393 light Sensor Module 3.3-5V 
* Some connecting wire
* A suitable USB cable
* Optional: A 5V power source to make the arrangement stand-alone

All items are cheap, and can be found on Ebay.

Required software
-----------------
The sketch "Power_Meter_LED_reader.ino", the COM/serial driver for the USB to serial chip, and the necessary Arduino libraries (see below).

Get started (Windows 10)
------------------------
Install the latest Arduino for Windows from here: https://www.arduino.cc/en/Main/Software

Configure the Arduino application
---------------------------------
Start the Arduino application.

In File->Preferences->Settings, enter this additional Boards Manager URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json .

In Tools->Board:->Boards Manager, find and install the library "esp8266 by ESP8266 Community".

In Tools, choose Board: "NodeMCU 1.0 (ESP8266-12E Module)", Flash Size: "4M (3M SPIFFS)", CPU Frequency: "80 MHz", Upload Speed: "115200".

Connect NodeMCU V3 to the computer
----------------------------------
Download COM/Serial port file CH341SER_WINDOWS.zip from https://github.com/nodemcu/nodemcu-devkit/tree/master/Drivers . Choose "Download ZIP", extract and install the driver. Note this is for the CH340 serial chip. Check the board you purchased. Your board may have the CP2102 chip, which needs another driver (not covered here). You should be able to find that driver online also, and then continue with this project.

Download arduinoWebSockets library from https://github.com/Links2004/arduinoWebSockets . Choose "Download ZIP", extract the .zip file and move it to the Arduino library folder.

Connect to the webpage
----------------------
You need to update the Arduino code with the ssid and WiFi password for your local network. You will find these variables on lines 68 and 69 in the .ino file. Also set the value of PULSES_PER_UNIT on line 57 to the number of pulses equal to 1 kWh and 1 kVArh on your meter.

Compile and upload the sketch. As soon as the upload reaches 100%, open the serial monitor and check that the board connects to your local WiFi network.

The ip address (automatically given to the unit by the DHCP server) is reported on the serial monitor. To connect, type the address into the address field of your browser on a PC or mobile phone as long as it is connected to the same local network. From the outside world you need to type your public ip address. This requires that you have performed the port forwarding mentioned above.

You should now have a simple webpage on your browser, allowing you to monitor the values calculated from the meter pulses. Also, if you enter the current meter kWh value after bootup (only once per system start), you can keep track of the total electric energy consumed in your house/flat on your mobile phone or PC. Note: I have found that there is a small drift in the energy value calculated from the LED blinks compared to the meter display itself. Any suggestions to explain this are very welcome.
