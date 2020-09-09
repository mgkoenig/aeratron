# Aeratron Web Remote

__IMPORTANT NOTE:__ This is an unofficial DIY devolpment of a web based remote control for [Aeratron](https://aeratron.com) ceiling fans. There is neither a whatsoever guarantee that this software is working properly nor of its completeness. 

## Description
This project implements a simple web interface to remote control your Aeratron AE2+ ceiling fan. The AE3+ model might also work but isn't tested.

![Screenshot](https://github.com/mgkoenig/Aeratron/blob/master/doc/screenshot.jpg)

## Getting Started
1. Download and install all listed software packages to your Arduino IDE.
2. Copy all provided resource files (favicon, syslog, styles, etc.) from the `data` folder using the _ESP32 Sketch Data Upload_ function to your ESP32 flash memory.
3. Download the source code and open it with your Arduino IDE.
4. Compile and run the code on your device.
5. Check for the IP address of your ESP32 device which is printed on your Serial Monitor.
6. Connect to your device with a web brwoser of your choice. 

## Used hardware
In order to use this application you need an [ESP32 module](https://www.amazon.com/MELIFE-Development-Dual-Mode-Microcontroller-Integrated/dp/B07Q576VWZ/ref=sr_1_2?dchild=1&keywords=esp32&qid=1599678294&sr=8-2) with a [433MHz transmitter](https://www.amazon.com/WayinTop-Transmitter-Frequency-Regenerative-Transceiver/dp/B086ZL8W1W/ref=sr_1_4?crid=2NCN2WQSXOQ4B&dchild=1&keywords=433mhz+transmitter+and+receiver&qid=1599678364&sprefix=433mhz%2Caps%2C270&sr=8-4) attached. In my case the device is USB bus powered only and capable to control the fan over a distance of approximately 4 meters. 

![Screenshot](https://github.com/mgkoenig/Aeratron/blob/master/doc/buildup.jpg)

## Used software packages
In order to enable all implemented features the following packages are used: 

* [Arduino core for the ESP32](https://github.com/espressif/arduino-esp32) for ESP32 core features
* [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) for web services
* [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) for network routines
* [ESP32FS Plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/) to make use of SPIFFS
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) to store a syslog as json file on the SPIFFS
* Useful: [ESP32 Erase Flash Plugin](https://github.com/tanakamasayuki/arduino-esp32erase-plugin/releases/tag/1.0) to erase the whole ESP32 flash memory if necessary

## Aeratron RF protocol
Aeratrons RF remote controls (at least model AE+) are communicating using the [LPD433](https://en.wikipedia.org/wiki/LPD433) UHF band.
### Bit representation
Worth mentioning: the data transfer method is similar to those of the famous WS2812 RGB LEDs. Each bit is represented in a timely manner of a 0 and 1. 

Representation of a logic 0:
```
              +--------+
              |
+-------------+
<-----T0L-----><--T0H-->
```
Representation of a logic 1:
```
        +--------------+
        |
+-------+
<--T1L--><-----T1H----->
```
According to my measurements, the timings are as follows:

Period | Duration
------ | -------------
T0L    | 1000µs
T0H    | 500µs
T1L    | 500µs
T1H    | 1000µs

### Data Sequence
A single command of the Aeratron fan always consists of a start bit (logic 1) followed by three bytes of the fan address, fan state and light state. 
```
+---+------------------------+------------------------+------------------------+
| S |      Fan Address       |       Fan State        |      Light State       |
+---+------------------------+------------------------+------------------------+
  ^
Start bit
```

#### Fan Address
The first byte represents the fan address. The address is related to the position of the dip switches inside your original Aeratron (AE+) remote control. You find the dip switches right next to the battery case. By factory setting all four dip switches are set. Therefore the address byte equals `0xF0`. 

#### Fan State
The second byte contains information about the fan speed as well the rotation direction. The byte pattern is as follows:

```
+---+---+---+---+---+---+---+---+
|   |   | D |   |   | S P E E D |
+---+---+---+---+---+---+---+---+
          ^               ^
    direction bit    speed level
```
The speed level (0 to 6) is stored binary coded within the last three bits, where 0 means the fan is turned off. 
If the direction bit is clear (0) the fan will rotate left (summer season). By setting the direction bit (1) it will rotate right (winter season). 

#### Light state
Unfortunately my fan is not equipped with the light kit. Therefore I am not able to test this feature. However, at least it's possible to re-enact the sequence for turning on and off the light switch. In order to implement the whole dimming feature further equipment and testing would be necessary. 

## Outlook
A next step could be to implement the full light dimming feature. If you would like to contribute to this project and are owning an Aeratron fan with the light kit, maybe you could provide me some light dimming sequences which I could implement. Let's get in touch! 
