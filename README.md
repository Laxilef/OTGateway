<div align="center">
   
   ![logo](/assets/logo.svg)
   <br>
   [![GitHub version](https://img.shields.io/github/release/Laxilef/OTGateway.svg?include_prereleases)](https://github.com/Laxilef/OTGateway/releases)
   [![GitHub download](https://img.shields.io/github/downloads/Laxilef/OTGateway/total.svg)](https://github.com/Laxilef/OTGateway/releases/latest)
   [![License](https://img.shields.io/github/license/Laxilef/OTGateway.svg)](LICENSE.txt)
   [![Telegram](https://img.shields.io/badge/Telegram-Channel-33A8E3)](https://t.me/otgateway)
   
</div>

## Features
- Hot water temperature control
- Heating temperature control
- Smart heating temperature control modes:
   - PID
   - Equithermic curves - adjusts the temperature based on indoor and outdoor temperatures
- Hysteresis setting (for accurate maintenance of room temperature)
- Ability to connect an external sensors to monitor outdoor and indoor temperature ([compatible sensors](https://github.com/Laxilef/OTGateway/wiki/Compatibility#temperature-sensors))
- Emergency mode. If the Wi-Fi connection is lost or the gateway cannot connect to the MQTT server, the mode will turn on. This mode will automatically maintain the set temperature and prevent your home from freezing. In this mode it is also possible to use equithermal curves (weather-compensated control).
- Automatic error reset (not with all boilers)
- Diagnostics:
  - The process of heating: works/does not work
  - The process of heating water for hot water: working/not working
  - Display of boiler errors
  - Burner status (flame): on/off
  - Burner modulation level in percent
  - Pressure in the heating system
  - Gateway status (depending on errors and connection status)
  - Boiler connection status via OpenTherm interface
  - The current temperature of the heat carrier (usually the return heat carrier)
  - Set heat carrier temperature (depending on the selected mode)
  - Current hot water temperature
- [Home Assistant](https://www.home-assistant.io/) integration via MQTT. The ability to create any automation for the boiler!

![logo](/assets/ha.png)

## Documentation
All available information and instructions can be found in the wiki:

* [Home](https://github.com/Laxilef/OTGateway/wiki)
   * [Quick Start](https://github.com/Laxilef/OTGateway/wiki#quick-start)
   * [Build firmware](https://github.com/Laxilef/OTGateway/wiki#build-firmware)
   * [Flash firmware via ESP Flash Download Tool](https://github.com/Laxilef/OTGateway/wiki#flash-firmware-via-esp-flash-download-tool)
   * [Settings](https://github.com/Laxilef/OTGateway/wiki#settings)
      * [External temperature sensors](https://github.com/Laxilef/OTGateway/wiki#external-temperature-sensors)
      * [Reporting indoor/outdoor temperature from any Home Assistant sensor](https://github.com/Laxilef/OTGateway/wiki#reporting-indooroutdoor-temperature-from-any-home-assistant-sensor)
      * [Reporting outdoor temperature from Home Assistant weather integration](https://github.com/Laxilef/OTGateway/wiki#reporting-outdoor-temperature-from-home-assistant-weather-integration)
      * [DHW meter](https://github.com/Laxilef/OTGateway/wiki#dhw-meter)
      * [Advanced Settings](https://github.com/Laxilef/OTGateway/wiki#advanced-settings)
   * [Equitherm mode](https://github.com/Laxilef/OTGateway/wiki#equitherm-mode)
      * [Ratios](https://github.com/Laxilef/OTGateway/wiki#ratios)
      * [Fit coefficients](https://github.com/Laxilef/OTGateway/wiki#fit-coefficients)
   * [PID mode](https://github.com/Laxilef/OTGateway/wiki#pid-mode)
* [Compatibility](https://github.com/Laxilef/OTGateway/wiki/Compatibility)
   * [Boilers](https://github.com/Laxilef/OTGateway/wiki/Compatibility#boilers)
   * [Boards](https://github.com/Laxilef/OTGateway/wiki/Compatibility#boards)
   * [Temperature sensors](https://github.com/Laxilef/OTGateway/wiki/Compatibility#temperature-sensors)
* [FAQ & Troubleshooting](https://github.com/Laxilef/OTGateway/wiki/FAQ-&-Troubleshooting)
* [OT adapters](https://github.com/Laxilef/OTGateway/wiki/OT-adapters)
   * [Adapters on sale](https://github.com/Laxilef/OTGateway/wiki/OT-adapters#adapters-on-sale)
   * [DIY](https://github.com/Laxilef/OTGateway/wiki/OT-adapters#diy)
      * [Files for production](https://github.com/Laxilef/OTGateway/wiki/OT-adapters#files-for-production)
      * [Connection](https://github.com/Laxilef/OTGateway/wiki/OT-adapters#connection)
      * [Leds on board](https://github.com/Laxilef/OTGateway/wiki/OT-adapters#leds-on-board)



## Dependencies
- [ESP8266Scheduler](https://github.com/nrwiersma/ESP8266Scheduler) (for ESP8266)
- [ESP32Scheduler](https://github.com/laxilef/ESP32Scheduler) (for ESP32)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [OpenTherm Library](https://github.com/ihormelnyk/opentherm_library)
- [ArduinoMqttClient](https://github.com/arduino-libraries/ArduinoMqttClient)
- [ESPTelnet](https://github.com/LennartHennigs/ESPTelnet)
- [FileData](https://github.com/GyverLibs/FileData)
- [GyverPID](https://github.com/GyverLibs/GyverPID)
- [GyverBlinker](https://github.com/GyverLibs/GyverBlinker)
- [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [TinyLogger](https://github.com/laxilef/TinyLogger)

## Debug
To display DEBUG messages you must enable debug in settings (switch is disabled by default).
You can connect via Telnet to read messages. IP: ESP8266 ip, port: 23
