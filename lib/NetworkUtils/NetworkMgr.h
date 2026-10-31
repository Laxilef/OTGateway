#pragma once
#include <NetworkAp.h>
#include <NetworkConnection.h>
#include <NetworkEthernet.h>
#include <NetworkUplink.h>
#include <NetworkWifi.h>

namespace NetworkUtils {
  /**
   * @brief Picks the uplink that carries the traffic and drives it
   *
   * The hierarchy is ethernet > WiFi > fallback AP: the first interface able to do it owns the
   * pass, and every status getter reads that decision instead of re-testing the interfaces.
   */
  class NetworkMgr {
  public:
    NetworkMgr() : wifi(&this->context), ap(&this->context, &this->wifi), ethernet(&this->context) {
      NetworkConnection::setup(this->context.useDhcp);
      this->wifi.resetWifi();
    }

    NetworkMgr* setYieldCallback(YieldCallback callback = nullptr) {
      this->context.yieldCallback = callback;

      return this;
    }

    NetworkMgr* setDelayCallback(DelayCallback callback = nullptr) {
      this->context.delayCallback = callback;

      return this;
    }

    NetworkMgr* setHostname(const char* value) {
      this->context.hostname = value;

      return this;
    }

    NetworkMgr* setApCredentials(const char* ssid, const char* password = nullptr, uint8_t channel = 0) {
      this->context.apName = ssid;
      this->context.apPassword = password;
      this->context.apChannel = channel;

      return this;
    }

    NetworkMgr* setStaCredentials(const char* ssid = nullptr, const char* password = nullptr, uint8_t channel = 0) {
      this->context.staSsid = ssid;
      this->context.staPassword = password;
      this->context.staChannel = channel;

      return this;
    }

    NetworkMgr* setUseDhcp(bool value) {
      if (value != this->context.useDhcp) {
        this->ethernet.markSettingsChanged();
      }

      this->context.useDhcp = value;
      NetworkConnection::setup(this->context.useDhcp);

      return this;
    }

    NetworkMgr* setStaticConfig(const char* ip, const char* gateway, const char* subnet, const char* dns) {
      IPAddress valueIp, valueGateway, valueSubnet, valueDns;

      valueIp.fromString(ip);
      valueGateway.fromString(gateway);
      valueSubnet.fromString(subnet);
      valueDns.fromString(dns);

      if (
        valueIp != this->context.staticIp || valueGateway != this->context.staticGateway
        || valueSubnet != this->context.staticSubnet || valueDns != this->context.staticDns
      ) {
        this->ethernet.markSettingsChanged();
      }

      this->context.staticIp = valueIp;
      this->context.staticGateway = valueGateway;
      this->context.staticSubnet = valueSubnet;
      this->context.staticDns = valueDns;

      return this;
    }

    NetworkMgr* setStaticConfig(IPAddress& ip, IPAddress& gateway, IPAddress& subnet, IPAddress& dns) {
      if (
        ip != this->context.staticIp || gateway != this->context.staticGateway
        || subnet != this->context.staticSubnet || dns != this->context.staticDns
      ) {
        this->ethernet.markSettingsChanged();
      }

      this->context.staticIp = ip;
      this->context.staticGateway = gateway;
      this->context.staticSubnet = subnet;
      this->context.staticDns = dns;

      return this;
    }

    /**
     * @brief Enable/disable the ethernet interface (W5500 over SPI)
     */
    NetworkMgr* setEthernet(bool enabled) {
      this->ethernet.setEnabled(enabled);

      return this;
    }

    /**
     * @brief Configure W5500 wiring. Use GPIO_IS_NOT_CONFIGURED to disable
     */
    NetworkMgr* setEthernetPins(uint8_t clk, uint8_t mosi, uint8_t miso, uint8_t cs, uint8_t irq, uint8_t rst) {
      this->ethernet.setPins(clk, mosi, miso, cs, irq, rst);

      return this;
    }

    /**
     * @brief Uplinks in the order they are tried: ethernet, WiFi, fallback AP
     */
    enum class Uplink : uint8_t { None = 0, Ap, Wifi, Ethernet };

    /**
     * @brief The uplink that currently carries traffic
     *
     * Always the first interface in the hierarchy that can do it, so every consumer reads the
     * same decision instead of re-testing ethernet and overriding the WiFi answer.
     */
    Uplink getUplink() {
      if (this->ethernet.isActive()) {
        return Uplink::Ethernet;
      }

      if (this->wifi.isActive()) {
        return Uplink::Wifi;
      }

      if (this->ap.isActive()) {
        return Uplink::Ap;
      }

      return Uplink::None;
    }

    bool isConnected() {
      return this->getUplink() >= Uplink::Wifi;
    }

    /**
     * @brief WiFi signal quality in percent, 0 when WiFi is not carrying the uplink
     */
    uint8_t getSignalQuality() {
      return this->getUplink() == Uplink::Wifi ? rssiToSignalQuality(this->wifi.rssi()) : 0;
    }

    /**
     * @return "ethernet", "wifi", "wifi-ap" or "none"
     */
    const char* getConnectionType() {
      return uplinkName(this->getUplink());
    }

    bool isApEnabled() {
      return this->ap.isEnabled();
    }

    short int getRssi() {
      return this->station()->rssi();
    }

    IPAddress getApIp() {
      return this->ap.localIP();
    }

    IPAddress getStaIp() {
      return this->station()->localIP();
    }

    IPAddress getStaSubnet() {
      return this->station()->subnetMask();
    }

    IPAddress getStaGateway() {
      return this->station()->gatewayIP();
    }

    IPAddress getStaDns() {
      return this->station()->dnsIP();
    }

    String getStaMac() {
      return this->station()->macAddress();
    }

    const char* getStaSsid() {
      return this->context.staSsid;
    }

    uint8_t getStaChannel() {
      return this->context.staChannel;
    }

    void reconnect() {
      this->wifi.reconnect();
    }

    void loop() {
      // The top of the hierarchy polls every pass so it can take the uplink whenever it can
      this->ethernet.loop();

      this->trackUplink();
      this->updateWifiRadio();

      if (this->getUplink() == Uplink::Ethernet) {
        return;
      }

      // a pending ethernet attempt holds the hierarchy: nothing below it starts until it resolves
      if (this->ethernet.isPending() && !this->wifi.isActive() && !this->ap.isActive()) {
        return;
      }

      this->wifi.loop();

      // the ladder only reaches the AP once the station is out of the running
      if (!this->wifi.isActive() && this->wifi.isPending()) {
        return;
      }

      this->ap.loop();
    }

    static uint8_t rssiToSignalQuality(short int rssi) {
      return constrain(map(rssi, -100, -50, 0, 100), 0, 100);
    }

  protected:
    UplinkContext context;
    WifiUplink wifi;
    ApUplink ap;
    EthernetUplink ethernet;

    Uplink uplink = Uplink::None;

    /**
     * @brief The interface holding the station address: the wired one while it carries the
     * uplink, the station otherwise
     */
    NetworkUplink* station() {
      if (this->getUplink() == Uplink::Ethernet) {
        return &this->ethernet;
      }

      return &this->wifi;
    }

    /**
     * @brief Notice an uplink change so the hierarchy is walked from the top again
     */
    void trackUplink() {
      const Uplink current = this->getUplink();

      if (current == this->uplink) {
        return;
      }

      Log.sinfoln(FPSTR(L_NETWORK), F("Uplink: %s -> %s"), uplinkName(this->uplink), uplinkName(current));

      this->uplink = current;
    }

    /**
     * @brief Keep the WiFi radio in step with the uplink
     *
     * WiFi is only in service while it carries the traffic. The AP is for first configuration:
     * once a cable carries the uplink the radio goes off, and if that uplink never comes up the
     * ladder falls back to the AP again on its own.
     */
    void updateWifiRadio() {
      if (this->ethernet.isActive()) {
        this->wifi.suspend();

        return;
      }

      this->wifi.resume();
    }

    /**
     * @brief Uplink token, already in locale-key shape
     */
    static const char* uplinkName(Uplink uplink) {
      switch (uplink) {
        case Uplink::Ethernet:
          return "ethernet";

        case Uplink::Wifi:
          return "wifi";

        case Uplink::Ap:
          return "wifi-ap";

        default:
          return "none";
      }
    }
  };
}
