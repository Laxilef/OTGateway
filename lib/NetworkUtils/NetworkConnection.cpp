#include "NetworkConnection.h"
using namespace NetworkUtils;

void NetworkConnection::setup(bool useDhcp) {
  setUseDhcp(useDhcp);

  WiFi.onEvent(NetworkConnection::onEvent);
}

void NetworkConnection::reset() {
  status = Status::NONE;
  rawDisconnectReason = 0;
  disconnectReason = DisconnectReason::NONE;
}

void NetworkConnection::setUseDhcp(bool value) {
  useDhcp = value;
}

NetworkConnection::Status NetworkConnection::getStatus() {
  return status;
}

NetworkConnection::DisconnectReason NetworkConnection::getDisconnectReason() {
  return disconnectReason;
}

void NetworkConnection::onEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      status = useDhcp ? Status::CONNECTING : Status::CONNECTED;
      rawDisconnectReason = 0;
      disconnectReason = DisconnectReason::NONE;
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
      status = Status::CONNECTED;
      rawDisconnectReason = 0;
      disconnectReason = DisconnectReason::NONE;
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      status = Status::DISCONNECTED;
      rawDisconnectReason = 0;
      disconnectReason = DisconnectReason::DHCP_TIMEOUT;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      status = Status::DISCONNECTED;
      rawDisconnectReason = info.wifi_sta_disconnected.reason;
      disconnectReason = convertDisconnectReason(info.wifi_sta_disconnected.reason);
      break;

    default:
      break;
  }
}

NetworkConnection::DisconnectReason NetworkConnection::convertDisconnectReason(uint8_t reason) {
  switch (reason) {
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

    default:
      return DisconnectReason::OTHER;
  }
}

bool NetworkConnection::useDhcp = false;
NetworkConnection::Status NetworkConnection::status = Status::NONE;
NetworkConnection::DisconnectReason NetworkConnection::disconnectReason = DisconnectReason::NONE;
uint8_t NetworkConnection::rawDisconnectReason = 0;
