#pragma once
#include <NetworkUplink.h>

#ifdef NETWORK_HAS_ETHERNET
  #include <ETH.h>
#endif

namespace NetworkUtils {
  /**
   * @brief Ethernet uplink: the W5500 driver, its cable and its DHCP lease
   */
  class EthernetUplink : public NetworkUplink {
  public:
    explicit EthernetUplink(UplinkContext* context) : NetworkUplink(context) {}

    void loop() override {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->restartFlag) {
        this->restartFlag = false;

        // The W5500 driver cannot be re-initialized in place: Arduino's ETH.end() leaves
        // the SPI bus allocated, so a wiring change only takes effect after a restart.
        if (this->started) {
          Log.swarningln(FPSTR(L_NETWORK), F("Ethernet: settings changed, restart required to apply them"));
        }
      }

      if (!this->enabled || !this->hasPins()) {
        if (this->active || this->linkUp) {
          this->active = false;
          this->linkUp = false;

          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: disabled, handing the uplink back to WiFi"));
        }

        this->attempt.disarm();

        return;
      }

      if (!this->started) {
        if (this->startUnavailable) {
          return;
        }

        Log.sinfoln(
          FPSTR(L_NETWORK),
          F("Ethernet: init W5500 (CS: %u, IRQ: %u, RST: %u, CLK: %u, MISO: %u, MOSI: %u)"),
          this->cs,
          this->irq,
          this->rst,
          this->clk,
          this->miso,
          this->mosi
        );

        if (!ETH.begin(ETH_PHY_W5500, 1, this->cs, this->irq, this->rst, SPI2_HOST, this->clk, this->miso, this->mosi, 12)) {
          // A failed init leaves the SPI bus allocated, so retrying cannot succeed;
          // wait for a restart instead of flapping.
          this->startUnavailable = true;
          this->attempt.disarm();

          Log.serrorln(FPSTR(L_NETWORK), F("Ethernet: W5500 init failed, check the wiring and restart"));

          return;
        }

        this->started = true;
        this->running = true;
        this->prevLinkCheck = 0;
        this->attempt.arm(this->linkGrace);

        if (!this->context->useDhcp) {
          ETH.config(this->context->staticIp, this->context->staticGateway, this->context->staticSubnet, this->context->staticDns);
        }

        if (strlen(this->context->hostname) && UplinkContext::isHostnameValid(this->context->hostname)) {
          ETH.setHostname(this->context->hostname);
        }

        return;
      }

      if (millis() - this->prevLinkCheck < this->linkCheckInterval) {
        return;
      }

      this->prevLinkCheck = millis();

      if (!ETH.linkUp()) {
        if (this->linkUp) {
          this->linkUp = false;

          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: cable disconnected"));
        }

        this->attempt.disarm();

        if (this->active) {
          this->active = false;

          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: link lost, falling back to WiFi"));
          this->context->delayCallback(250);
        }

        return;
      }

      if (!this->linkUp) {
        this->linkUp = true;
        this->attempt.arm(this->leaseGrace);

        Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: cable connected"));
      }

      if (!ETH.connected() || this->active) {
        return;
      }

      this->active = true;
      this->attempt.disarm();

      Log.sinfoln(
        FPSTR(L_NETWORK),
        F("Ethernet: using ethernet instead of WiFi, IP: %s, link: %u Mbps, duplex: %u"),
        ETH.localIP().toString().c_str(),
        ETH.linkSpeed(),
        ETH.fullDuplex() ? 1 : 0
      );
      #endif
    }

    /**
     * @brief Ethernet carries the uplink: cable plugged in and DHCP lease obtained
     */
    bool isActive() override {
      #ifdef NETWORK_HAS_ETHERNET
      if (!this->enabled || !this->started) {
        return false;
      }

      return ETH.linkUp() && ETH.connected();
      #else
      return false;
      #endif
    }

    /**
     * @brief Enable/disable the interface, taking the driver in and out of the FSM
     */
    void setEnabled(bool value) {
      this->enabled = value;

      #ifdef NETWORK_HAS_ETHERNET
      // The driver stays installed: stop() only takes the link and the netif down, while the
      // SPI bus, the MAC and the netif itself stay allocated, so start() resumes without a restart
      if (this->started && ETH.handle() != nullptr && value != this->running) {
        esp_err_t ret = value ? esp_eth_start(ETH.handle()) : esp_eth_stop(ETH.handle());

        if (ret == ESP_OK) {
          this->running = value;

        } else {
          Log.serrorln(FPSTR(L_NETWORK), F("Ethernet: driver %s failed"), value ? "start" : "stop");
        }
      }
      #endif
    }

    /**
     * @brief Configure the W5500 wiring. Use GPIO_IS_NOT_CONFIGURED to disable
     */
    void setPins(uint8_t clk, uint8_t mosi, uint8_t miso, uint8_t cs, uint8_t irq, uint8_t rst) {
      if (
        clk != this->clk || mosi != this->mosi || miso != this->miso
        || cs != this->cs || irq != this->irq || rst != this->rst
      ) {
        this->markSettingsChanged();
      }

      this->clk = clk;
      this->mosi = mosi;
      this->miso = miso;
      this->cs = cs;
      this->irq = irq;
      this->rst = rst;
    }

    /**
     * @brief A setting the driver reads changed: applying it needs a restart
     */
    void markSettingsChanged() {
      if (this->started) {
        this->restartFlag = true;
      }
    }

    bool hasPins() {
      return this->clk != GPIO_IS_NOT_CONFIGURED
        && this->mosi != GPIO_IS_NOT_CONFIGURED
        && this->miso != GPIO_IS_NOT_CONFIGURED
        && this->cs != GPIO_IS_NOT_CONFIGURED
        && this->irq != GPIO_IS_NOT_CONFIGURED
        && this->rst != GPIO_IS_NOT_CONFIGURED;
    }

    bool isLinkUp() {
      #ifdef NETWORK_HAS_ETHERNET
      return this->started && ETH.linkUp();
      #else
      return false;
      #endif
    }

    IPAddress localIP() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.localIP();
      #else
      return IPAddress();
      #endif
    }

    IPAddress subnetMask() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.subnetMask();
      #else
      return IPAddress();
      #endif
    }

    IPAddress gatewayIP() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.gatewayIP();
      #else
      return IPAddress();
      #endif
    }

    IPAddress dnsIP() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.dnsIP();
      #else
      return IPAddress();
      #endif
    }

    String macAddress() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.macAddress();
      #else
      return String();
      #endif
    }

  protected:
    const unsigned long linkCheckInterval = 1000;
    const unsigned long linkGrace = 5000;
    const unsigned long leaseGrace = 20000;

    bool enabled = false;
    bool started = false;
    bool running = false;
    bool active = false;
    bool linkUp = false;
    bool restartFlag = false;
    bool startUnavailable = false;

    uint8_t clk = GPIO_IS_NOT_CONFIGURED;
    uint8_t mosi = GPIO_IS_NOT_CONFIGURED;
    uint8_t miso = GPIO_IS_NOT_CONFIGURED;
    uint8_t cs = GPIO_IS_NOT_CONFIGURED;
    uint8_t irq = GPIO_IS_NOT_CONFIGURED;
    uint8_t rst = GPIO_IS_NOT_CONFIGURED;

    unsigned long prevLinkCheck = 0;
  };
}
