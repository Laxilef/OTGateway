#pragma once
#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
  #include "lwip/etharp.h"
#elif defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <esp_wifi.h>
#endif

// W5500 support needs both the ESP32 core (for ETH.h) and the per-env USE_ETHERNET flag.
// USE_ETHERNET defaults to false in defines.h, so this must be a value test, not #ifdef.
#if USE_ETHERNET && defined(ARDUINO_ARCH_ESP32)
  #define NETWORK_HAS_ETHERNET
#endif

namespace NetworkUtils {
  typedef std::function<void()> YieldCallback;
  typedef std::function<void(unsigned int)> DelayCallback;

  /**
   * @brief Settings and callbacks every uplink reads
   */
  struct UplinkContext {
    const char* hostname = "esp";

    const char* staSsid = nullptr;
    const char* staPassword = nullptr;
    uint8_t staChannel = 0;

    const char* apName = "ESP";
    const char* apPassword = nullptr;
    uint8_t apChannel = 1;

    bool useDhcp = true;
    IPAddress staticIp;
    IPAddress staticGateway;
    IPAddress staticSubnet;
    IPAddress staticDns;

    YieldCallback yieldCallback = []() {
      ::yield();
    };
    DelayCallback delayCallback = [](unsigned int time) {
      ::delay(time);
    };

    /**
     * @brief check RFC compliance
     *
     * @param value
     * @return true
     * @return false
     */
    static bool isHostnameValid(const char* value) {
      size_t len = strlen(value);
      if (len > 24) {
        return false;
      } else if (value[len - 1] == '-') {
        return false;
      }

      for (size_t i = 0; i < len; i++) {
        if (!isalnum(value[i]) && value[i] != '-') {
          return false;
        }
      }

      return true;
    }
  };

  /**
   * @brief How long an uplink may still take the pass
   *
   * Armed when the uplink starts trying, disarmed when it resolves either way. A window that has
   * run out means the uplink gave up, which lets the class below it take over.
   */
  class UplinkAttempt {
  public:
    void arm(unsigned long window) {
      this->start = millis() ? millis() : 1;
      this->window = window;
    }

    void disarm() {
      this->start = 0;
      this->window = 0;
    }

    bool isArmed() const {
      return this->window > 0;
    }

    bool isPending() const {
      return this->window > 0 && (millis() - this->start) < this->window;
    }

  protected:
    unsigned long start = 0;
    unsigned long window = 0;
  };

  /**
   * @brief One interface that is able to carry the uplink
   */
  class NetworkUplink {
  public:
    explicit NetworkUplink(UplinkContext* context) : context(context) {}

    virtual ~NetworkUplink() {}

    /**
     * @brief Run one pass of this interface's state machine
     */
    virtual void loop() = 0;

    /**
     * @brief The interface is able to carry traffic right now
     */
    virtual bool isActive() = 0;

    /**
     * @brief The interface may still become active, so nothing below it should start
     */
    virtual bool isPending() {
      return !this->isActive() && this->attempt.isPending();
    }

    virtual IPAddress localIP() = 0;
    virtual IPAddress subnetMask() = 0;
    virtual IPAddress gatewayIP() = 0;
    virtual IPAddress dnsIP() = 0;
    virtual String macAddress() = 0;

    virtual short int rssi() {
      return 0;
    }

  protected:
    UplinkContext* context;
    UplinkAttempt attempt;
  };
}
