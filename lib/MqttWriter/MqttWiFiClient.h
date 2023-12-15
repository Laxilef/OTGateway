#include <WiFiClient.h>

class MqttWiFiClient : public WiFiClient {
public:
#ifdef ARDUINO_ARCH_ESP8266
  void flush() override {
    if (this->connected()) {
      WiFiClient::flush(0);
    }
  }

  void stop() override {
    this->abort();
  }
#endif

#ifdef ARDUINO_ARCH_ESP32
  void setSync(bool) {}

  bool getSync() {
    return false;
  }
#endif
};