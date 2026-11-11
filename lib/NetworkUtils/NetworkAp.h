#pragma once
#include <NetworkUplink.h>
#include <NetworkWifi.h>

namespace NetworkUtils {
  /**
   * @brief Fallback AP: the setup network, raised when the station cannot carry the uplink
   */
  class ApUplink : public NetworkUplink {
  public:
    ApUplink(UplinkContext* context, WifiUplink* wifi) : NetworkUplink(context), wifi(wifi) {}

    void loop() override {
      // the association carries the traffic again: retire the AP once it is stable and idle
      if (this->wifi->isActive()) {
        if (
          this->isEnabled() && !this->hasClients()
          && millis() - this->wifi->getConnectedTime() >= this->retireDelay
        ) {
          Log.sinfoln(FPSTR(L_NETWORK), F("Stop AP because STA connected"));
          this->stop();
        }

        return;
      }

      if (this->isEnabled()) {
        this->attempt.disarm();

        return;
      }

      // a failed softAP() retries once the window runs out
      if (this->attempt.isPending()) {
        return;
      }

      this->start();
    }

    /**
     * @brief The AP interface is up, whether or not anyone is on it
     *
     * A parked radio means the AP is not carrying anything, whatever the mode still says.
     */
    bool isActive() override {
      return this->isEnabled() && !this->wifi->isSuspended();
    }

    bool isEnabled() {
      return (WiFi.getMode() & WIFI_AP) != 0;
    }

    bool hasClients() {
      if (!this->isEnabled()) {
        return false;
      }

      return WiFi.softAPgetStationNum() > 0;
    }

    void start() {
      Log.sinfoln(
        FPSTR(L_NETWORK),
        this->wifi->hasCredentials()
          ? F("Disconnected for a long time, start AP")
          : F("No STA credentials, start AP")
      );

      WiFi.mode(WIFI_AP_STA);
      this->context->delayCallback(250);

      if (!WiFi.softAP(this->context->apName, this->context->apPassword, this->context->apChannel)) {
        this->attempt.arm(this->startTimeout);
      }
    }

    void stop() {
      WiFi.mode(WIFI_STA);
      this->attempt.disarm();
    }

    IPAddress localIP() override {
      return WiFi.softAPIP();
    }

    IPAddress subnetMask() override {
      #ifdef ARDUINO_ARCH_ESP32
      return WiFi.softAPSubnetMask();
      #else
      return IPAddress(255, 255, 255, 0);
      #endif
    }

    IPAddress gatewayIP() override {
      return WiFi.softAPIP();
    }

    IPAddress dnsIP() override {
      return WiFi.softAPIP();
    }

    String macAddress() override {
      return WiFi.softAPmacAddress();
    }

  protected:
    const unsigned long retireDelay = 15000;
    const unsigned long startTimeout = 5000;

    WifiUplink* wifi;
  };
}
