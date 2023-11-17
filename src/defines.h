#define OT_GATEWAY_VERSION          "1.3.4"
#define OT_GATEWAY_REPO             "https://github.com/Laxilef/OTGateway"
#define AP_SSID                     "OpenTherm Gateway"
#define AP_PASSWORD                 "otgateway123456"

#define EMERGENCY_TIME_TRESHOLD     120000
#define MQTT_RECONNECT_INTERVAL     5000
#define MQTT_KEEPALIVE              30

#define OPENTHERM_OFFLINE_TRESHOLD  10

#define EXT_SENSORS_INTERVAL        5000
#define EXT_SENSORS_FILTER_K        0.15

#define CONFIG_URL                  "http://%s/"
#define SETTINGS_VALID_VALUE        "stvalid" // only 8 chars!


#ifndef WM_DEBUG_MODE
  #define WM_DEBUG_MODE WM_DEBUG_NOTIFY
#endif

#ifndef USE_TELNET
  #define USE_TELNET true
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

#if USE_TELNET
  #define INFO_STREAM TelnetStream
  #define WARN_STREAM TelnetStream
  #define ERROR_STREAM TelnetStream
  #define DEBUG_STREAM  if (settings.debug) TelnetStream
  #define WM_DEBUG_PORT TelnetStream
#else
  #define INFO_STREAM Serial
  #define WARN_STREAM Serial
  #define ERROR_STREAM Serial
  #define DEBUG_STREAM  if (settings.debug) Serial
  #define WM_DEBUG_PORT Serial
#endif

#define INFO(...) INFO_STREAM.print("\r[INFO] "); INFO_STREAM.println(__VA_ARGS__);
#define INFO_F(...) INFO_STREAM.print("\r[INFO] "); INFO_STREAM.printf(__VA_ARGS__);
#define WARN(...) WARN_STREAM.print("\r[WARN] "); WARN_STREAM.println(__VA_ARGS__);
#define WARN_F(...) WARN_STREAM.print("\r[WARN] "); WARN_STREAM.printf(__VA_ARGS__);
#define ERROR(...) ERROR_STREAM.print("\r[ERROR] "); ERROR_STREAM.println(__VA_ARGS__);
#define DEBUG(...) DEBUG_STREAM.print("\r[DEBUG] "); DEBUG_STREAM.println(__VA_ARGS__);
#define DEBUG_F(...) DEBUG_STREAM.print("\r[DEBUG] "); DEBUG_STREAM.printf(__VA_ARGS__);

char buffer[255];