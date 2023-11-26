![logo](/assets/logo.svg)

## Features
- Hot water temperature control
- Heating temperature control
- Smart heating temperature control modes:
   - PID
   - Equithermic curves - adjusts the temperature based on indoor and outdoor temperatures
- Hysteresis setting (for accurate maintenance of room temperature)
- Ability to connect an external sensors to monitor outdoor and indoor temperature ([compatible sensors](#compatible-temperature-sensors))
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
- Auto tuning of PID and Equitherm parameters *(in development)*
- [Home Assistant](https://www.home-assistant.io/) integration via MQTT. The ability to create any automation for the boiler!

![logo](/assets/ha.png)

## Tested on
| Boiler | Master Member ID | Notes |
| --- | --- | --- |
| BAXI ECO Nova | default | Pressure sensor not supported, modulation level not stable |
| BAXI Ampera | 4 | Pressure sensor not supported, only heating (DHW not tested) |
| [Remeha Calenta Ace 40C](https://github.com/Laxilef/OTGateway/issues/1#issuecomment-1726081554) | default | - |
| [Baxi Nuvola DUO-TEC HT 16](https://github.com/Laxilef/OTGateway/issues/3#issuecomment-1751061488) | default | - |
| [AEG GBA124](https://github.com/Laxilef/OTGateway/issues/3#issuecomment-1765857609) | default | Pressure sensor not supported |
| [Ferroli DOMIcompact C 24](https://github.com/Laxilef/OTGateway/issues/3#issuecomment-1765310058)<br><sub>Board: MF08FA</sub> | 211 | Pressure sensor not supported |
| [Thermet Ecocondens Silver 35kW](https://github.com/Laxilef/OTGateway/issues/3#issuecomment-1767026384) | default | Pressure sensor not supported |
| [BAXI LUNA-3](https://github.com/Laxilef/OTGateway/issues/3#issuecomment-1794187178) | default | - |
| ITALTHERM TIME MAX 30F | default | Pressure sensor not supported<br>**Important:** on the "setup" page you need to set the checkbox "Opentherm summer/winter mode" |
| [Viessmann Vitodens 0-50w](https://github.com/Laxilef/OTGateway/issues/3#issuecomment-1819870142) | default | - |

## PCB
<img src="/assets/pcb.svg" width="27%" /> <img src="/assets/pcb_3d.png" width="30%" /> <img src="/assets/after_assembly.png" width="40%" />

Housing for installation on DIN rail - D2MG. Occupies only 2 DIN modules.<br>
The 220V > 5V power supply is already on the board, so additional power supplies are not needed.<br>
To save money, 2 levels are ordered as one board. After manufacturing, the boards need to be divided into 2 parts - upper and lower. The boards are inexpensively (5pcs for $2) manufactured at JLCPCB (Remove Order Number = Specify a location).<br><br>
Some components can be replaced with similar ones (for example use a fuse and led with legs). Some SMD components (for example optocouplers) can be replaced with similar SOT components.<br>Most of the components can be purchased inexpensively on Aliexpress, the rest in your local stores.

#### Connection
The outdoor temperature sensor must be connected to the **TEMP1** connector, the indoor temperature sensor must be connected to the **TEMP2** connector. The power supply for the sensors must be connected to the **3.3V** connector **(NOT 5V!)**, GND to **GND**.<br>
**The opentherm connection polarity does not matter.**
<!-- **Important!** On this board opentherm IN pin = 5, OUT pin = 4 -->

#### Leds
| LED | States |
| --- | --- |
| OT RX | Flashes when a response to the request is received from the boiler |
| Status | Controller status.<br>On, not blinking - no errors;<br>2 flashes - no connection to Wifi;<br>3 flashes - no connection to boiler;<br>4 flashes - boiler is fault;<br>5 flashes - emergency mode (no connection to Wifi or to the MQTT server)<br>10 fast flashes - end of the list of errors |
| Power | Always on when power is on |

#### Files for production
- [Schematic](/assets/Schematic.pdf)
- [BOM](/assets/BOM.xlsx)
- [Gerber](/assets/gerber.zip)

## Another compatible OpenTherm Adapters
- [Ihor Melnyk OpenTherm Adapter](http://ihormelnyk.com/opentherm_adapter)
- [DIYLESS Master OpenTherm Shield](https://diyless.com/product/master-opentherm-shield)
- [OpenTherm master shield for Wemos/Lolin](https://www.tindie.com/products/thehognl/opentherm-master-shield-for-wemoslolin/)
- And others. It's just that the adapter must implement [the schema](http://ihormelnyk.com/Content/Pages/opentherm_adapter/opentherm_adapter_schematic_o.png)

## Compatible Temperature Sensors
* DS18B20
* DS1822
* DS1820
* MAX31820
* MAX31850

[See more](https://github.com/milesburton/Arduino-Temperature-Control-Library#usage)

# Quick Start
1. Download the latest firmware from the [releases page](https://github.com/Laxilef/OTGateway/releases) (or compile yourself) and flash your ESP8266 board using the [ESP Flash Download Tool](https://www.espressif.com/en/support/download/other-tools) or other software.
2. Connect to *OpenTherm Gateway* hotspot, password: otgateway123456
3. Open configuration page in browser: 192.168.4.1
4. Set up a connection to your wifi network
5. Set up a connection to your MQTT server: ip, port, user, password
6. Set up a **Opentherm pin IN** & **Opentherm pin OUT**. No change for my board. Typically used **IN** = 4, **OUT** = 5
7. Set up a **Outdoor sensor pin** & **Indoor sensor pin**. No change for my board.
8. if necessary, set up a the Master Member ID ([see more](#tested-on))
9. Restart module (required after changing OT pins and/or sensors pins!)

After connecting to your wifi network, you can go to the setup page at the address that ESP8266 received.
The OTGateway device will be automatically added to homeassistant if MQTT server ip, login and password are correct.

## HomeAsssistant settings
By default, the "Equitherm" and "PID" modes are disabled. In this case, the boiler will simply maintain the temperature you set.
To use "Equitherm" or "PID" modes, the controller needs to know the temperature inside and outside the house.<br><br>
The temperature inside the house can be set using simple automation:
<details>

  **sensor.livingroom_temperature** - temperature sensor inside the house.<br>
  **number.opentherm_indoor_temp** - an entity that stores the temperature value inside the house. The default does not need to be changed.

  ```yaml
    alias: Set boiler indoor temp
    description: ""
    trigger:
      - platform: state
        entity_id:
          - sensor.livingroom_temperature
      - platform: time_pattern
        seconds: /30
    condition: []
    action:
      - if:
          - condition: template
            value_template: "{{ has_value('number.opentherm_indoor_temp') and (states('sensor.livingroom_temperature')|float(0) - states('number.opentherm_indoor_temp')|float(0)) | abs | round(2) >= 0.01 }}"
        then:
          - service: number.set_value
            data:
              value: "{{ states('sensor.livingroom_temperature')|float(0)|round(2) }}"
            target:
              entity_id: number.opentherm_indoor_temp
    mode: single
  ```
</details>

If your boiler does not support the installation of an outdoor temperature sensor or does not provide this value via the opentherm protocol, then you can use an external sensor or use automation.
<details>
  <summary>Simple automation</summary>

  **weather.home** - [weather entity](https://www.home-assistant.io/integrations/weather/). It is important that the address of your home is entered correctly in the Home Assistant settings.<br>
  **number.opentherm_outdoor_temp** - an entity that stores the temperature value outside the house. The default does not need to be changed.

  ```yaml
    alias: Set boiler outdoor temp
    description: ""
    trigger:
      - platform: state
        entity_id:
          - weather.home
        attribute: temperature
        for:
          hours: 0
          minutes: 1
          seconds: 0
      - platform: time_pattern
        seconds: /30
    condition: []
    action:
      - if:
          - condition: template
            value_template: "{{ has_value('weather.home') and (state_attr('weather.home', 'temperature')|float(0) - states('number.opentherm_outdoor_temp')|float(0)) | abs | round(2) >= 0.1 }}"
        then:
          - service: number.set_value
            data:
              value: "{{ state_attr('weather.home', 'temperature')|float(0)|round(2) }}"
            target:
              entity_id: number.opentherm_outdoor_temp
    mode: single
  ```
</details>
After these settings, you can enable the "Equitherm" and/or "PID" modes and configure them as described below.

## Troubleshooting
**Q:** Controller in monitor/readonly mode (cannot set temperature, etc.)<br>
**A:** Try changing the "Master Member ID", this is most likely the problem. If you don't know which identifier is valid for your boiler, you need to use brute force.

**Q:** The boiler does not respond to setting the heating temperature.<br>
**A:** Try on "Opentherm summer/winter mode", "Opentherm CH2 enabled", "Opentherm heating CH1 to CH2" checkboxes one by one and all together on the "setup" page. If this doesn't work, see above.

**Q:** My boiler does not have a hot water heating function. How to turn it off?<br>
**A:** Uncheck "Opentherm DHW present" on the "setup" page.

**Q:** My boiler does not stop heating the heat carrier even if I turn off the heating.<br>
**A:** Try on "Modulation sync with heating" checkbox on the "setup" page.

**Q:** My DHW does not work when the heating is turned off.<br>
**A:** Try off "Modulation sync with heating" checkbox on the "setup" page.


## About modes
### Equitherm
Weather-compensated temperature control maintains a comfortable set temperature in the house. The algorithm requires temperature sensors in the house and outside.<br> Instead of an outdoor sensor, you can use the weather forecast and automation for HA.

#### Ratios:
***N*** - heating curve coefficient. The coefficient is selected individually, depending on the insulation of the room, the heated area, etc.<br>
Range: 0.001...10, default: 0.7, step 0.001


***K*** - сorrection for desired room temperature.<br>
Range: 0...10, default: 3, step 0.01


***T*** - thermostat correction.<br>
Range: 0...10, default: 2, step 0.01

#### Instructions for fit coefficients:
**Tip.** I created a [table in Excel](/assets/equitherm_calc.xlsx) in which you can enter temperature parameters inside and outside the house and select coefficients. On the graph you can see the temperature that the boiler will set.

1. Set the ***K*** and ***T*** coefficients to 0.
2. The first thing you need to do is to fit the curve (***N*** coefficient). If your home has low heat loss, then start with 0.5. Otherwise start at 0.7. When the temperature inside the house stops changing, increase or decrease the coefficient value in increments of 0.1 to select the optimal curve.<br>
Please note that passive heating (sun) will affect the house temperature during curve fitting. This process is not fast and will take you 1-2 days.
Important. During curve fitting, the temperature must be kept stable as the outside temperature changes.<br>
At this stage, it is important for you to stabilize the indoor temperature at exactly 20 (+- 0.5) degrees.<br>
For example. You fit curve 0.67; set temperature 20; the temperature in the house is 20.1 degrees while the outside temperature is -10 degrees and -5 degrees. This is good.
3. After fitting the curve, you must select the ***K*** coefficient. It influences the boiler temperature correction to maintain the set temperature.
For example. Set temperature: 23 degrees; temperature in the house: 20 degrees. Try setting it to 2 and see how the temperature in the house changes after stabilization. Select the value so that the temperature in the house is close to the set.
4. Now you can choose the ***T*** coefficient. Simply put, it affects the sharpness of the temperature change. If you want fast heating, then set a high value (6-10), but then the room may overheat. If you want smooth heating, set 1-5. Choose the optimal value for yourself.
5. Check to see if it works correctly at different set temperatures over several days.

Read more about the algorithm [here](https://wdn.su/blog/1154).

### PID
See [Wikipedia](https://en.wikipedia.org/wiki/PID_controller).
![PID example](https://upload.wikimedia.org/wikipedia/commons/3/33/PID_Compensation_Animated.gif)

In Google you can find instructions for tuning the PID controller.

<!--### Use Equitherm mode + PID mode
@todo-->

## Dependencies
- [ESP8266Scheduler](https://github.com/nrwiersma/ESP8266Scheduler) (for ESP8266)
- [ESP32Scheduler](https://github.com/laxilef/ESP32Scheduler) (for ESP32)
<!-- - [NTPClient](https://github.com/arduino-libraries/NTPClient)-->
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [OpenTherm Library](https://github.com/ihormelnyk/opentherm_library)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [ESPTelnet](https://github.com/LennartHennigs/ESPTelnet)
- [EEManager](https://github.com/GyverLibs/EEManager)
- [GyverPID](https://github.com/GyverLibs/GyverPID)
- [GyverBlinker](https://github.com/GyverLibs/GyverBlinker)
- [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [WiFiManager](https://github.com/tzapu/WiFiManager)

## Debug
To display DEBUG messages you must enable debug in settings (switch is disabled by default).
You can connect via Telnet to read messages. IP: ESP8266 ip, port: 23
