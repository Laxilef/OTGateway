#define PROJECT_NAME                    "OpenTherm Gateway"
#define PROJECT_VERSION                 "1.4.0-rc.24"
#define PROJECT_REPO                    "https://github.com/Laxilef/OTGateway"

#define MQTT_RECONNECT_INTERVAL         15000

#define EXT_SENSORS_INTERVAL            5000
#define EXT_SENSORS_FILTER_K            0.15

#define CONFIG_URL                      "http://%s/"
#define SETTINGS_VALID_VALUE            "stvalid" // only 8 chars!
#define GPIO_IS_NOT_CONFIGURED          0xff

#define DEFAULT_HEATING_TARGET_TEMP     40
#define DEFAULT_HEATING_MIN_TEMP        20
#define DEFAULT_HEATING_MAX_TEMP        90

#define DEFAULT_DHW_TARGET_TEMP         40
#define DEFAULT_DHW_MIN_TEMP            30
#define DEFAULT_DHW_MAX_TEMP            60

#define THERMOSTAT_INDOOR_DEFAULT_TEMP  20
#define THERMOSTAT_INDOOR_MIN_TEMP      5
#define THERMOSTAT_INDOOR_MAX_TEMP      30

#ifndef USE_SERIAL
  #define USE_SERIAL true
#endif

#ifndef USE_TELNET
  #define USE_TELNET true
#endif

#ifndef USE_BLE
  #define USE_BLE false
#endif

#ifndef DEFAULT_HOSTNAME
  #define DEFAULT_HOSTNAME "opentherm"
#endif

#ifndef DEFAULT_AP_SSID
  #define DEFAULT_AP_SSID "OpenTherm Gateway"
#endif

#ifndef DEFAULT_AP_PASSWORD
  #define DEFAULT_AP_PASSWORD "otgateway123456"
#endif

#ifndef DEFAULT_STA_SSID
  #define DEFAULT_STA_SSID ""
#endif

#ifndef DEFAULT_STA_PASSWORD
  #define DEFAULT_STA_PASSWORD ""
#endif

#ifndef DEBUG_BY_DEFAULT
  #define DEBUG_BY_DEFAULT false
#endif

#ifndef DEFAULT_STATUS_LED_GPIO
  #define DEFAULT_STATUS_LED_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_PORTAL_LOGIN
  #define DEFAULT_PORTAL_LOGIN ""
#endif

#ifndef DEFAULT_PORTAL_PASSWORD
  #define DEFAULT_PORTAL_PASSWORD ""
#endif

#ifndef DEFAULT_MQTT_SERVER
  #define DEFAULT_MQTT_SERVER ""
#endif

#ifndef DEFAULT_MQTT_PORT
  #define DEFAULT_MQTT_PORT 1883
#endif

#ifndef DEFAULT_MQTT_USER
  #define DEFAULT_MQTT_USER ""
#endif

#ifndef DEFAULT_MQTT_PASSWORD
  #define DEFAULT_MQTT_PASSWORD ""
#endif

#ifndef DEFAULT_MQTT_PREFIX
  #define DEFAULT_MQTT_PREFIX "opentherm"
#endif

#ifndef DEFAULT_OT_IN_GPIO
  #define DEFAULT_OT_IN_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_OT_OUT_GPIO
  #define DEFAULT_OT_OUT_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_OT_RX_LED_GPIO
  #define DEFAULT_OT_RX_LED_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_SENSOR_OUTDOOR_GPIO
  #define DEFAULT_SENSOR_OUTDOOR_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_SENSOR_INDOOR_GPIO
  #define DEFAULT_SENSOR_INDOOR_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_RELAY1_GPIO
  #define DEFAULT_RELAY1_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_RELAY2_GPIO
  #define DEFAULT_RELAY2_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_RELAY3_GPIO
  #define DEFAULT_RELAY3_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_RELAY4_GPIO
  #define DEFAULT_RELAY4_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef DEFAULT_EXT_PUMP_GPIO
  #define DEFAULT_EXT_PUMP_GPIO GPIO_IS_NOT_CONFIGURED
#endif

#ifndef PROGMEM
  #define PROGMEM 
#endif

#ifndef GPIO_IS_VALID_GPIO
  #define GPIO_IS_VALID_GPIO(gpioNum) (gpioNum >= 0 && gpioNum <= 16)
#endif

#define GPIO_IS_VALID(gpioNum) (gpioNum != GPIO_IS_NOT_CONFIGURED && GPIO_IS_VALID_GPIO(gpioNum))

enum class SensorType : byte {
  BOILER,
  MANUAL,
  DS18B20,
  BLUETOOTH
};

enum class UnitSystem : byte {
  METRIC,
  IMPERIAL
};

char buffer[255];