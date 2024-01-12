#include "Connection.h"

void Connection::setup(bool useDhcp) {
  setUseDhcp(useDhcp);

  #if defined(ARDUINO_ARCH_ESP8266)
  wifi_set_event_handler_cb(Connection::onEvent);
  #elif defined(ARDUINO_ARCH_ESP32)
  WiFi.onEvent(Connection::onEvent);
  #endif
}

void Connection::setUseDhcp(bool value) {
  useDhcp = value;
}

Connection::Status Connection::getStatus() {
  return status;
}

Connection::DisconnectReason Connection::getDisconnectReason() {
  return disconnectReason;
}

#if defined(ARDUINO_ARCH_ESP8266)
void Connection::onEvent(System_Event_t *evt) {
  switch (evt->event) {
    case EVENT_STAMODE_CONNECTED:
      status = useDhcp ? Status::CONNECTING : Status::CONNECTED;
      disconnectReason = DisconnectReason::NONE;
      
      break;

    case EVENT_STAMODE_GOT_IP:
      status = Status::CONNECTED;
      disconnectReason = DisconnectReason::NONE;
      break;

    case EVENT_STAMODE_DHCP_TIMEOUT:
      status = Status::DISCONNECTED;
      disconnectReason = DisconnectReason::DHCP_TIMEOUT;
      break;

    case EVENT_STAMODE_DISCONNECTED:
      status = Status::DISCONNECTED;
      disconnectReason = convertDisconnectReason(evt->event_info.disconnected.reason);

      break;
    
    default:
      break;
  }
}
#elif defined(ARDUINO_ARCH_ESP32)
void Connection::onEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      status = useDhcp ? Status::CONNECTING : Status::CONNECTED;
      disconnectReason = DisconnectReason::NONE;
      
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
      status = Status::CONNECTED;
      disconnectReason = DisconnectReason::NONE;
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      status = Status::DISCONNECTED;
      disconnectReason = DisconnectReason::DHCP_TIMEOUT;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      status = Status::DISCONNECTED;
      disconnectReason = convertDisconnectReason(info.wifi_sta_disconnected.reason);

      break;
    
    default:
      break;
  }

  //Serial.printf("SYS EVENT: %d, reason: %d\n\r", evt->event, disconnectReason);
}
#endif

Connection::DisconnectReason Connection::convertDisconnectReason(uint8_t reason) {
  switch (reason) {
  #if defined(ARDUINO_ARCH_ESP8266)
    case REASON_BEACON_TIMEOUT:
      return DisconnectReason::BEACON_TIMEOUT;

    case REASON_NO_AP_FOUND:
      return DisconnectReason::NO_AP_FOUND;

    case REASON_AUTH_FAIL:
      return DisconnectReason::AUTH_FAIL;

    case REASON_ASSOC_FAIL:
      return DisconnectReason::ASSOC_FAIL;

    case REASON_HANDSHAKE_TIMEOUT:
      return DisconnectReason::HANDSHAKE_TIMEOUT;
  #elif defined(ARDUINO_ARCH_ESP32)
    case WIFI_REASON_BEACON_TIMEOUT:
      return DisconnectReason::BEACON_TIMEOUT;

    case WIFI_REASON_NO_AP_FOUND:
      return DisconnectReason::NO_AP_FOUND;

    case WIFI_REASON_AUTH_FAIL:
      return DisconnectReason::AUTH_FAIL;

    case WIFI_REASON_ASSOC_FAIL:
      return DisconnectReason::ASSOC_FAIL;

    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return DisconnectReason::HANDSHAKE_TIMEOUT;
  #endif
    
    default:
      return DisconnectReason::OTHER;
  }
}

bool Connection::useDhcp = false;
Connection::Status Connection::status = Status::DISCONNECTED;
Connection::DisconnectReason Connection::disconnectReason = DisconnectReason::NONE;