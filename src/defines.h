#define PROJECT_NAME                "OpenTherm Gateway"
#define PROJECT_VERSION             "1.4.0-rc.12"
#define PROJECT_REPO                "https://github.com/Laxilef/OTGateway"

#define EMERGENCY_TIME_TRESHOLD     120000
#define MQTT_RECONNECT_INTERVAL     15000
#define MQTT_KEEPALIVE              30

#define OPENTHERM_OFFLINE_TRESHOLD  10

#define EXT_SENSORS_INTERVAL        5000
#define EXT_SENSORS_FILTER_K        0.15

#define CONFIG_URL                  "http://%s/"
#define SETTINGS_VALID_VALUE        "stvalid" // only 8 chars!

#define DEFAULT_HEATING_MIN_TEMP    20
#define DEFAULT_HEATING_MAX_TEMP    90
#define DEFAULT_DHW_MIN_TEMP        30
#define DEFAULT_DHW_MAX_TEMP        60


#ifndef WM_DEBUG_MODE
  #define WM_DEBUG_MODE WM_DEBUG_NOTIFY
#endif

#ifndef USE_SERIAL
  #define USE_SERIAL true
#endif

#ifndef USE_TELNET
  #define USE_TELNET true
#endif

#ifndef USE_BLE
  #define USE_BLE false
#endif

#ifndef HOSTNAME_DEFAULT
  #define HOSTNAME_DEFAULT "opentherm"
#endif

#ifndef AP_SSID_DEFAULT
  #define AP_SSID_DEFAULT "OpenTherm Gateway"
#endif

#ifndef AP_PASSWORD_DEFAULT
  #define AP_PASSWORD_DEFAULT "otgateway123456"
#endif

#ifndef STA_SSID_DEFAULT
  #define STA_SSID_DEFAULT ""
#endif

#ifndef STA_PASSWORD_DEFAULT
  #define STA_PASSWORD_DEFAULT ""
#endif

#ifndef DEBUG_BY_DEFAULT
  #define DEBUG_BY_DEFAULT false
#endif

#ifndef PORTAL_LOGIN_DEFAULT
  #define PORTAL_LOGIN_DEFAULT ""
#endif

#ifndef PORTAL_PASSWORD_DEFAULT
  #define PORTAL_PASSWORD_DEFAULT ""
#endif

#ifndef MQTT_SERVER_DEFAULT
  #define MQTT_SERVER_DEFAULT ""
#endif

#ifndef MQTT_PORT_DEFAULT
  #define MQTT_PORT_DEFAULT 1883
#endif

#ifndef MQTT_USER_DEFAULT
  #define MQTT_USER_DEFAULT ""
#endif

#ifndef MQTT_PASSWORD_DEFAULT
  #define MQTT_PASSWORD_DEFAULT ""
#endif

#ifndef MQTT_PREFIX_DEFAULT
  #define MQTT_PREFIX_DEFAULT "opentherm"
#endif

#ifndef OT_IN_PIN_DEFAULT
  #define OT_IN_PIN_DEFAULT 0
#endif

#ifndef OT_OUT_PIN_DEFAULT
  #define OT_OUT_PIN_DEFAULT 0
#endif

#ifndef SENSOR_OUTDOOR_PIN_DEFAULT
  #define SENSOR_OUTDOOR_PIN_DEFAULT 0
#endif

#ifndef SENSOR_INDOOR_PIN_DEFAULT
  #define SENSOR_INDOOR_PIN_DEFAULT 0
#endif

#ifndef EXT_PUMP_PIN_DEFAULT
  #define EXT_PUMP_PIN_DEFAULT 0
#endif

#ifndef PROGMEM
  #define PROGMEM 
#endif

char buffer[255];