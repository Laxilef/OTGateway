#pragma once
#include <NetworkConnection.h>
#include <NetworkUplink.h>

namespace NetworkUtils {
  /**
   * @brief WiFi uplink: the station association, its radio, and the fallback AP
   */
  class WifiUplink : public NetworkUplink {
  public:
    explicit WifiUplink(UplinkContext* context) : NetworkUplink(context) {}

    void loop() override {
      if (this->reconnectFlag) {
        this->context->delayCallback(5000);

        Log.sinfoln(FPSTR(L_NETWORK), F("Reconnecting..."));
        this->reconnectFlag = false;
        this->disconnect();
        NetworkConnection::reset();
        this->context->delayCallback(1000);

        return;
      }

      if (this->isAssociated() && !this->hasCredentials()) {
        Log.sinfoln(FPSTR(L_NETWORK), F("Reset"));
        this->resetWifi();
        NetworkConnection::reset();
        this->context->delayCallback(1000);

        return;
      }

      if (this->isAssociated()) {
        this->attempt.disarm();

        if (!this->connected) {
          this->connectedTime = millis();
          this->connected = true;

          Log.sinfoln(
            FPSTR(L_NETWORK),
            F("Connected, downtime: %lu s., IP: %s, RSSI: %hhd"),
            (millis() - this->disconnectedTime) / 1000,
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI()
          );
        }

        #ifdef ARDUINO_ARCH_ESP8266
        if (millis() - this->prevArpGratuitous > 60000) {
          this->stationKeepAliveNow();
          this->prevArpGratuitous = millis();
        }
        #endif

        return;
      }

      // the association is down: track the downtime and keep asking for the network
      if (this->connected) {
        this->disconnectedTime = millis();
        this->connected = false;
        this->attempt.arm(this->failedConnectTimeout);

        Log.sinfoln(
          FPSTR(L_NETWORK),
          F("Disconnected, reason: %d, uptime: %lu s."),
          NetworkConnection::getDisconnectReason(),
          (millis() - this->connectedTime) / 1000
        );
      }

      if (!this->hasCredentials()) {
        this->attempt.disarm();

        return;
      }

      if (!this->attempt.isArmed()) {
        this->attempt.arm(this->failedConnectTimeout);
      }

      if (this->isConnecting() && millis() - this->prevReconnectingTime > this->resetConnectionTimeout) {
        Log.swarningln(FPSTR(L_NETWORK), F("Connection timeout, reset wifi..."));
        this->resetWifi();
        NetworkConnection::reset();
        this->context->delayCallback(250);

        return;
      }

      if (!this->isConnecting() && (!this->prevReconnectingTime || millis() - this->prevReconnectingTime > this->reconnectInterval)) {
        Log.sinfoln(FPSTR(L_NETWORK), F("Try connect..."));

        NetworkConnection::reset();
        if (!this->connect(true, this->connectionTimeout)) {
          Log.straceln(FPSTR(L_NETWORK), F("Connection failed. Status: %d, reason: %d, raw reason: %d"), NetworkConnection::getStatus(), NetworkConnection::getDisconnectReason(), NetworkConnection::rawDisconnectReason);
        }

        this->prevReconnectingTime = millis();
      }
    }

    /**
     * @brief WiFi carries the uplink: radio running and associated
     */
    bool isActive() override {
      return !this->suspended && this->isAssociated();
    }

    bool isAssociated() {
      return this->isStaEnabled() && NetworkConnection::getStatus() == NetworkConnection::Status::CONNECTED;
    }

    bool isConnecting() {
      return this->isStaEnabled() && NetworkConnection::getStatus() == NetworkConnection::Status::CONNECTING;
    }

    bool isStaEnabled() {
      return (WiFi.getMode() & WIFI_STA) != 0;
    }

    bool isApEnabled() {
      return (WiFi.getMode() & WIFI_AP) != 0;
    }

    /**
     * @brief When the association first came up, for the fallback AP's retirement delay
     */
    unsigned long getConnectedTime() {
      return this->connectedTime;
    }

    /**
     * @brief Stop the radio while another uplink carries the traffic
     *
     * esp_wifi_stop() parks the radio without tearing the driver and its netifs down.
     * WiFi.mode(WIFI_OFF) does the same job but also deinitialises, which panicked when it
     * was called from this task with lwIP live — keep the driver allocated.
     */
    void suspend() {
      #ifdef ARDUINO_ARCH_ESP32
      if (this->suspended) {
        return;
      }

      if (WiFi.getMode() == WIFI_MODE_NULL) {
        return;
      }

      // a scan holds the radio: parking it mid-scan leaves the driver half torn down
      if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        return;
      }

      Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: uplink active, switching the WiFi radio off"));

      esp_wifi_stop();
      this->suspended = true;
      #endif
    }

    /**
     * @brief Bring the radio back when no other uplink carries the traffic
     */
    void resume() {
      #ifdef ARDUINO_ARCH_ESP32
      if (!this->suspended) {
        return;
      }

      this->suspended = false;

      Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: uplink lost, switching the WiFi radio back on"));

      esp_wifi_start();

      this->connected = false;
      this->disconnectedTime = millis();
      this->prevReconnectingTime = 0;
      this->attempt.arm(this->failedConnectTimeout);

      NetworkConnection::reset();

      if (this->hasCredentials()) {
        WiFi.begin(this->context->staSsid, this->context->staPassword);

        Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: reconnecting to WiFi \"%s\""), this->context->staSsid);
      }
      #endif
    }

    bool isSuspended() {
      return this->suspended;
    }

    bool resetWifi() {
      // set policy manual for work 13 ch
      {
        #ifdef ARDUINO_ARCH_ESP8266
        wifi_country_t country = {"JP", 1, 14, WIFI_COUNTRY_POLICY_MANUAL};
        wifi_set_country(&country);
        #elif defined(ARDUINO_ARCH_ESP32)
        const wifi_country_t country = {"JP", 1, 14, CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER, WIFI_COUNTRY_POLICY_MANUAL};
        esp_wifi_set_country(&country);
        #endif
      }

      WiFi.persistent(false);
      #if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
      WiFi.setAutoConnect(false);
      #endif
      WiFi.setAutoReconnect(false);

      #ifdef ARDUINO_ARCH_ESP8266
      WiFi.setSleepMode(WIFI_NONE_SLEEP);
      #elif defined(ARDUINO_ARCH_ESP32)
      WiFi.setSleep(USE_BLE ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
      #endif

      WiFi.softAPdisconnect();
      #ifdef ARDUINO_ARCH_ESP8266
      /*if (wifi_softap_dhcps_status() == DHCP_STARTED) {
        wifi_softap_dhcps_stop();
      }*/
      #endif

      WiFi.disconnect(false, true);
      #ifdef ARDUINO_ARCH_ESP8266
      /*if (wifi_station_dhcpc_status() == DHCP_STARTED) {
        wifi_station_dhcpc_stop();
      }*/

      wifi_station_dhcpc_set_maxtry(5);
      #endif

      #if defined(ARDUINO_ARCH_ESP32) && ESP_ARDUINO_VERSION_MAJOR < 3
      // Nothing. Because memory leaks when turn off WiFi on ESP32 SDK < 3.0.0
      return true;
      #else
      return WiFi.mode(WIFI_OFF);
      #endif
    }

    /**
     * @brief Ask for a fresh connect
     *
     * The attempt window is armed here so the ladder keeps the fallback AP out of the way while
     * the station works on this reconnect; it expires like any other failed attempt.
     */
    void reconnect() {
      this->attempt.arm(this->failedConnectTimeout);

      this->reconnectFlag = true;
    }

    bool connect(bool force = false, unsigned int timeout = 1000u) {
      if (this->isAssociated() && !force) {
        return true;
      }

      if (force && !this->isApEnabled()) {
        this->resetWifi();
        NetworkConnection::reset();

      } else {
        /*#ifdef ARDUINO_ARCH_ESP8266
        if (wifi_station_dhcpc_status() == DHCP_STARTED) {
          wifi_station_dhcpc_stop();
        }
        #endif*/

        this->disconnect();
      }

      if (!this->hasCredentials()) {
        return false;
      }

      this->context->delayCallback(250);

      #ifdef ARDUINO_ARCH_ESP32
      if (this->setHostname(this->context->hostname)) {
        Log.straceln(FPSTR(L_NETWORK), F("Set hostname '%s': success"), this->context->hostname);

      } else {
        Log.serrorln(FPSTR(L_NETWORK), F("Set hostname '%s': fail"), this->context->hostname);
      }
      #endif

      if (!WiFi.mode((WiFiMode_t)(WiFi.getMode() | WIFI_STA))) {
        return false;
      }

      this->context->delayCallback(250);

      #ifdef ARDUINO_ARCH_ESP8266
      if (this->setHostname(this->context->hostname)) {
        Log.straceln(FPSTR(L_NETWORK), F("Set hostname '%s': success"), this->context->hostname);

      } else {
        Log.serrorln(FPSTR(L_NETWORK), F("Set hostname '%s': fail"), this->context->hostname);
      }

      this->context->delayCallback(250);
      #endif

      #ifdef ARDUINO_ARCH_ESP32
      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
      WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
      #endif

      if (!this->context->useDhcp) {
        WiFi.begin(this->context->staSsid, this->context->staPassword, this->context->staChannel, nullptr, false);
        WiFi.config(this->context->staticIp, this->context->staticGateway, this->context->staticSubnet, this->context->staticDns);
        WiFi.reconnect();

      } else {
        #ifdef ARDUINO_ARCH_ESP32
        // the interface keeps a static address from an earlier association, so the addressing has
        // to be reset before the station reconnects or the switch back to DHCP never takes effect
        const IPAddress noIp(0, 0, 0, 0);
        WiFi.config(noIp, noIp, noIp);
        #endif

        WiFi.begin(this->context->staSsid, this->context->staPassword, this->context->staChannel, nullptr, true);
      }

      unsigned long beginConnectionTime = millis();
      while (millis() - beginConnectionTime < timeout) {
        this->context->delayCallback(100);

        NetworkConnection::Status status = NetworkConnection::getStatus();
        if (status != NetworkConnection::Status::CONNECTING && status != NetworkConnection::Status::NONE) {
          return status == NetworkConnection::Status::CONNECTED;
        }
      }

      return false;
    }

    void disconnect() {
      #ifdef ARDUINO_ARCH_ESP32
      WiFi.disconnectAsync(false, true);

      const unsigned long start = millis();
      while (WiFi.isConnected() && (millis() - start) < 5000) {
        this->context->delayCallback(100);
      }
      #else
      WiFi.disconnect(false, true);
      #endif
    }

    bool setHostname(const char* hostname) {
      if (!UplinkContext::isHostnameValid(hostname)) {
        return false;
      }

      if (strcmp(WiFi.getHostname(), hostname) == 0) {
        return true;
      }

      return WiFi.setHostname(hostname);
    }

    bool hasCredentials() {
      return this->context->staSsid != nullptr;
    }

    IPAddress localIP() override {
      return WiFi.localIP();
    }

    IPAddress subnetMask() override {
      return WiFi.subnetMask();
    }

    IPAddress gatewayIP() override {
      return WiFi.gatewayIP();
    }

    IPAddress dnsIP() override {
      return WiFi.dnsIP();
    }

    String macAddress() override {
      return WiFi.macAddress();
    }

    short int rssi() override {
      return WiFi.RSSI();
    }

  protected:
    const unsigned int reconnectInterval = 15000;
    const unsigned int failedConnectTimeout = 185000;
    const unsigned int connectionTimeout = 10000;
    const unsigned int resetConnectionTimeout = 90000;

    bool connected = false;
    bool reconnectFlag = false;
    bool suspended = false;
    unsigned long prevArpGratuitous = 0;
    unsigned long prevReconnectingTime = 0;
    unsigned long connectedTime = 0;
    unsigned long disconnectedTime = 0;

    #ifdef ARDUINO_ARCH_ESP8266
    /**
     * @brief
     * https://github.com/arendst/Tasmota/blob/e6515883f0ee5451931b6280ff847b117de5a231/tasmota/tasmota_support/support_wifi.ino#L1196
     */
    static void stationKeepAliveNow(void) {
      for (netif* interface = netif_list; interface != nullptr; interface = interface->next) {
        if (
          (interface->flags & NETIF_FLAG_LINK_UP)
          && (interface->flags & NETIF_FLAG_UP)
          && interface->num == STATION_IF
          && (!ip4_addr_isany_val(*netif_ip4_addr(interface)))
          ) {
          etharp_gratuitous(interface);
          break;
        }
      }
    }
    #endif
  };
}
