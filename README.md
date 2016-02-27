## ESP8266 based PSTN Caller ID interface

The CDN message detailed
[here](http://www.epanorama.net/documents/telecom/cid_bellcore.html)
is parsed by an AVR micro controller and passed on to the ESP8266
module over UART. The ESP8266 then hands it over to an MQTT broker by
publishing it as the topic ```/clipdev```. The message payload would
be string like this:

    <error status>,<month>,<day>,<hour>,<minute>,<calling line number>
	
This message originates from the firmware running on the AVR micro controller.

The source code for the ESP8266 SoC is in the ```esp8266``` directory and that 
for the AVR (ATTiny84) microcontroller that interfaces with the FSK chip is in ```avr_ht9032```.

## Hardware

This was made for the device described in https://github.com/aniline/wifi_pstn_cid .

## Building

### ESP Firmware

Setup the variables as required by https://github.com/esp8266/source-code-examples . 
This includes stuff like:

    XTENSA_TOOLS_ROOT=/opt/Espressif/crosstool-NG/builds/xtensa-lx106-elf/bin
    SDK_BASE=/opt/Espressif/ESP8266_SDK
    FW_TOOL=${XTENSA_TOOLS_ROOT}/esptool.py
    ESPTOOL=${XTENSA_TOOLS_ROOT}/esptool.py
    ESPPORT=/dev/ttyUSB0

Build checkdirs because the Makefile symlinks some .c files from SDK in the first step.

    $ make checkdirs
    $ make

Flash using. Pass ```ESPPORT=<ESP serial port>``` if it changed from the time you set the environment.

    $ make flash

This version has not been tested with the SDK 1.5.

### AVR firmware

If you have AVR GCC suite just a ```make``` should build the ```.hex``` file. The provided ```makefile``` uses [avrdude](http://www.nongnu.org/avrdude/) to flash and programmer is set to [usbasp](http://www.fischl.de/usbasp/).

## Configuring

### Wifi Credentials in ESP 8266 

It uses the [tuanpmt's](https://github.com/tuanpmt) library and configuration mechanism. It loads
the settings in ```user_config.h``` to flash if the magic number CFG_HOLDER is different from whats
in the flash. So when you update configuration change CFG_HOLDER to re-save the values to flash.

## Thanks

* [tuanpmt's MQTT Library: esp_mqtt](https://github.com/tuanpmt/esp_mqtt) 
* [pfalcon's ESP Open SDK](https://github.com/pfalcon/esp-open-sdk)

