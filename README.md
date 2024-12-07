<div align="center">

   [![GitHub version](https://img.shields.io/github/release/Laxilef/OTGateway.svg?include_prereleases)](https://github.com/Laxilef/OTGateway/releases)
   [![GitHub download](https://img.shields.io/github/downloads/Laxilef/OTGateway/total.svg)](https://github.com/Laxilef/OTGateway/releases/latest)
   [![License](https://img.shields.io/github/license/Laxilef/OTGateway.svg)](LICENSE.txt)
   [![Telegram](https://img.shields.io/badge/Telegram-Channel-33A8E3)](https://t.me/otgateway)

</div>
<hr />

![Dashboard](/assets/poster-1.png) 
![Configuration](/assets/poster-2.png) 
![Integration with HomeAssistant](/assets/poster-3.png)

## Features
- DHW temperature control
- Heating temperature control
- Smart heating temperature control modes:
   - PID
   - Equithermic curves - adjusts the temperature based on indoor and outdoor temperatures
- Hysteresis setting _(for accurate maintenance of room temperature)_
- Ability to connect [additional (external) sensors](https://github.com/Laxilef/OTGateway/wiki/Compatibility#temperature-sensors): Dallas (1-wire), NTC 10k, Bluetooth (BLE). Makes it possible to monitor indoor and outdoor temperatures, temperatures on pipes/heat exchangers/etc.
- Emergency mode. In any dangerous situation _(loss of connection with Wifi, MQTT, sensors, etc)_ it will not let you and your home freeze.
- Ability of remote fault reset _(not with all boilers)_
- Diagnostics:
  - Displaying gateway status
  - Displaying the connection status to the boiler via OpenTherm
  - Displaying the fault status and fault code
  - Displaying the diagnostic status & diagnostic code
  - Display of the process of heating: works/does not work
  - Display of burner (flame) status: on/off
  - Display of burner modulation level in percent
  - Display of pressure in the heating system
  - Display of current temperature of the heat carrier
  - Display of return temperature of the heat carrier
  - Display of setpoint heat carrier temperature (useful when using PID or Equitherm)
  - Display of the process of DHW: working/not working
  - Display of current DHW temperature
  - _And other information..._
- [Home Assistant](https://www.home-assistant.io/) integration via MQTT. The ability to create any automation for the boiler!

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
   * [Logs and debug](https://github.com/Laxilef/OTGateway/wiki#logs-and-debug)
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

___
This project is tested with BrowserStack.
