#define PROJECT_NAME                "OpenTherm Gateway"
#define PROJECT_VERSION             "1.4.0"
#define PROJECT_REPO                "https://github.com/Laxilef/OTGateway"
#define AP_SSID                     "OpenTherm Gateway"
#define AP_PASSWORD                 "otgateway123456"

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

#ifndef DEBUG_BY_DEFAULT
  #define DEBUG_BY_DEFAULT false
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