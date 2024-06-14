#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
  #include "lwip/etharp.h"
#elif defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#endif

namespace NetworkUtils {
  struct NetworkConnection {
    enum class Status {
      CONNECTED,
      CONNECTING,
      DISCONNECTED,
      NONE
    };

    enum class DisconnectReason {
      BEACON_TIMEOUT,
      NO_AP_FOUND,
      AUTH_FAIL,
      ASSOC_FAIL,
      HANDSHAKE_TIMEOUT,
      DHCP_TIMEOUT,
      OTHER,
      NONE
    };

    static Status status;
    static DisconnectReason disconnectReason;
    static uint8_t rawDisconnectReason;
    
    static void setup(bool useDhcp);
    static void setUseDhcp(bool value);
    static void reset();
    static Status getStatus();
    static DisconnectReason getDisconnectReason();
    #if defined(ARDUINO_ARCH_ESP8266)
    static void onEvent(System_Event_t *evt);
    #elif defined(ARDUINO_ARCH_ESP32)
    static void onEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    #endif

  protected:
    static DisconnectReason convertDisconnectReason(uint8_t reason);
    static bool useDhcp;
  };
}