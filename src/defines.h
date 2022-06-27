#define OT_GATEWAY_VERSION          "1.0.5"
#define AP_SSID                     "OpenTherm Gateway"
#define USE_TELNET

#define EMERGENCY_TRESHOLD          10
#define MQTT_RECONNECT_INTERVAL     5000
#define MQTT_KEEPALIVE              30

#define OPENTHERM_IN_PIN            4
#define OPENTHERM_OUT_PIN           5
#define OPENTHERM_OFFLINE_TRESHOLD  10

#define DS18B20_PIN                 2
#define DS18B20_INTERVAL            1000
#define DS_CHECK_CRC                true
#define DS_CRC_USE_TABLE            true

#define CONFIG_URL                  "http://%s/"


#ifdef USE_TELNET
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

char buffer[120];