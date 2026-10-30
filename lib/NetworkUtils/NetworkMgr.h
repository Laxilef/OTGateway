#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include "lwip/etharp.h"
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
  #if USE_ETHERNET
    #include <ETH.h>
  #endif
#endif

// W5500 support needs both the ESP32 core (for ETH.h) and the per-env USE_ETHERNET flag.
// USE_ETHERNET defaults to false in defines.h, so this must be a value test, not #ifdef.
#if USE_ETHERNET && defined(ARDUINO_ARCH_ESP32)
  #define NETWORK_HAS_ETHERNET
#endif
#include <NetworkConnection.h>

namespace NetworkUtils {
  class NetworkMgr {
  public:
    typedef std::function<void()> YieldCallback;
    typedef std::function<void(unsigned int)> DelayCallback;

    NetworkMgr() {
      NetworkConnection::setup(this->useDhcp);
      this->resetWifi();
    }

    NetworkMgr* setYieldCallback(YieldCallback callback = nullptr) {
      this->yieldCallback = callback;

      return this;
    }

    NetworkMgr* setDelayCallback(DelayCallback callback = nullptr) {
      this->delayCallback = callback;

      return this;
    }

    NetworkMgr* setHostname(const char* value) {
      this->hostname = value;
      return this;
    }

    NetworkMgr* setApCredentials(const char* ssid, const char* password = nullptr, uint8_t channel = 0) {
      this->apName = ssid;
      this->apPassword = password;
      this->apChannel = channel;

      return this;
    }

    NetworkMgr* setStaCredentials(const char* ssid = nullptr, const char* password = nullptr, uint8_t channel = 0) {
      this->staSsid = ssid;
      this->staPassword = password;
      this->staChannel = channel;

      return this;
    }

    NetworkMgr* setUseDhcp(bool value) {
      this->useDhcp = value;
      NetworkConnection::setup(this->useDhcp);

      return this;
    }

    NetworkMgr* setStaticConfig(const char* ip, const char* gateway, const char* subnet, const char* dns) {
      this->staticIp.fromString(ip);
      this->staticGateway.fromString(gateway);
      this->staticSubnet.fromString(subnet);
      this->staticDns.fromString(dns);

      return this;
    }

    NetworkMgr* setStaticConfig(IPAddress& ip, IPAddress& gateway, IPAddress& subnet, IPAddress& dns) {
      this->staticIp = ip;
      this->staticGateway = gateway;
      this->staticSubnet = subnet;
      this->staticDns = dns;

      return this;
    }

    /**
     * @brief Enable/disable the ethernet interface (W5500 over SPI)
     */
    NetworkMgr* setEthernet(bool enabled) {
      if (enabled != this->ethEnabled) {
        this->ethRestartFlag = true;
      }

      this->ethEnabled = enabled;
      return this;
    }

    /**
     * @brief Configure W5500 wiring. Use GPIO_IS_NOT_CONFIGURED to disable
     */
    NetworkMgr* setEthernetPins(uint8_t clk, uint8_t mosi, uint8_t miso, uint8_t cs, uint8_t irq, uint8_t rst) {
      if (
        clk != this->ethClk || mosi != this->ethMosi || miso != this->ethMiso
        || cs != this->ethCs || irq != this->ethIrq || rst != this->ethRst
      ) {
        this->ethRestartFlag = true;
      }

      this->ethClk = clk;
      this->ethMosi = mosi;
      this->ethMiso = miso;
      this->ethCs = cs;
      this->ethIrq = irq;
      this->ethRst = rst;

      return this;
    }

    bool hasStaCredentials() {
      return this->staSsid != nullptr;
    }

    bool isConnected() {
      if (this->isEthernetActive()) {
        return true;
      }

      return this->isStaEnabled() && NetworkConnection::getStatus() == NetworkConnection::Status::CONNECTED;
    }

    bool isEthernetEnabled() {
      return this->ethEnabled;
    }

    /**
     * @brief Ethernet is the active uplink: cable plugged in and DHCP lease obtained
     */
    bool isEthernetActive() {
      #ifdef NETWORK_HAS_ETHERNET
      if (!this->ethEnabled || !this->ethStarted) {
        return false;
      }

      return ETH.linkUp() && ETH.connected();
      #else
      return false;
      #endif
    }

    /**
     * @brief WiFi carries the uplink: radio running, associated, no ethernet takeover
     *
     * False while ethernet is the uplink (the radio is parked and its RSSI/channel are stale),
     * which is what the status API and MQTT publish on.
     */
    bool isWifiUplink() {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->ethActive || this->wifiRadioOff) {
        return false;
      }
      #endif

      return this->isStaEnabled() && NetworkConnection::getStatus() == NetworkConnection::Status::CONNECTED;
    }

    /**
     * @brief WiFi signal quality in percent, 0 when WiFi is not carrying the uplink
     */
    uint8_t getSignalQuality() {
      return this->isWifiUplink() ? rssiToSignalQuality(WiFi.RSSI()) : 0;
    }

    bool isEthernetLinkUp() {
      #ifdef NETWORK_HAS_ETHERNET
      return this->ethStarted && ETH.linkUp();
      #else
      return false;
      #endif
    }

    bool isEthernetEnabledState() {
      return this->ethEnabled;
    }

    bool isEthernetDriverStarted() {
      return this->ethStarted;
    }

    bool isEthernetInitFailed() {
      return this->ethStartUnavailable;
    }

    /**
     * @return "ethernet", "wifi" or "none"
     */
    const char* getConnectionType() {
      if (this->isEthernetActive()) {
        return "ethernet";
      }

      if (this->isStaEnabled() && NetworkConnection::getStatus() == NetworkConnection::Status::CONNECTED) {
        return "wifi";
      }

      return "none";
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

    bool hasApClients() {
      if (!this->isApEnabled()) {
        return false;
      }

      return WiFi.softAPgetStationNum() > 0;
    }

    short int getRssi() {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->isEthernetActive()) {
        return 0;
      }
      #endif

      return WiFi.RSSI();
    }

    IPAddress getApIp() {
      return WiFi.softAPIP();
    }

    IPAddress getStaIp() {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->isEthernetActive()) {
        return ETH.localIP();
      }
      #endif

      return WiFi.localIP();
    }

    IPAddress getStaSubnet() {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->isEthernetActive()) {
        return ETH.subnetMask();
      }
      #endif

      return WiFi.subnetMask();
    }

    IPAddress getStaGateway() {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->isEthernetActive()) {
        return ETH.gatewayIP();
      }
      #endif

      return WiFi.gatewayIP();
    }

    IPAddress getStaDns() {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->isEthernetActive()) {
        return ETH.dnsIP();
      }
      #endif

      return WiFi.dnsIP();
    }

    String getStaMac() {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->isEthernetActive()) {
        return ETH.macAddress();
      }
      #endif

      return WiFi.macAddress();
    }

    const char* getStaSsid() {
      return this->staSsid;
    }

    const char* getStaPassword() {
      return this->staPassword;
    }

    uint8_t getStaChannel() {
      return this->staChannel;
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

    void reconnect() {
      this->reconnectFlag = true;
    }

    bool connect(bool force = false, unsigned int timeout = 1000u) {
      if (this->isConnected() && !force) {
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

      if (!this->hasStaCredentials()) {
        return false;
      }

      this->delayCallback(250);

      #ifdef ARDUINO_ARCH_ESP32
      if (this->setWifiHostname(this->hostname)) {
        Log.straceln(FPSTR(L_NETWORK), F("Set hostname '%s': success"), this->hostname);

      } else {
        Log.serrorln(FPSTR(L_NETWORK), F("Set hostname '%s': fail"), this->hostname);
      }
      #endif

      if (!WiFi.mode((WiFiMode_t)(WiFi.getMode() | WIFI_STA))) {
        return false;
      }

      this->delayCallback(250);

      #ifdef ARDUINO_ARCH_ESP8266
      if (this->setWifiHostname(this->hostname)) {
        Log.straceln(FPSTR(L_NETWORK), F("Set hostname '%s': success"), this->hostname);

      } else {
        Log.serrorln(FPSTR(L_NETWORK), F("Set hostname '%s': fail"), this->hostname);
      }

      this->delayCallback(250);
      #endif

      #ifdef ARDUINO_ARCH_ESP32
      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
      WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
      #endif

      if (!this->useDhcp) {
        WiFi.begin(this->staSsid, this->staPassword, this->staChannel, nullptr, false);
        WiFi.config(this->staticIp, this->staticGateway, this->staticSubnet, this->staticDns);
        WiFi.reconnect();

      } else {
        WiFi.begin(this->staSsid, this->staPassword, this->staChannel, nullptr, true);
      }

      unsigned long beginConnectionTime = millis();
      while (millis() - beginConnectionTime < timeout) {
        this->delayCallback(100);

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
        this->delayCallback(100);
      }
      #else
      WiFi.disconnect(false, true);
      #endif
    }

    void loop() {
      #ifdef NETWORK_HAS_ETHERNET
      this->loopEthernet();

      // Ethernet owns the uplink while a cable is connected and DHCP has completed
      if (this->isEthernetActive()) {
        return;
      }
      #endif

      if (this->reconnectFlag) {
        this->delayCallback(5000);

        Log.sinfoln(FPSTR(L_NETWORK), F("Reconnecting..."));
        this->reconnectFlag = false;
        this->disconnect();
        NetworkConnection::reset();
        this->delayCallback(1000);

      } else if (this->isConnected() && !this->hasStaCredentials()) {
        Log.sinfoln(FPSTR(L_NETWORK), F("Reset"));
        this->resetWifi();
        NetworkConnection::reset();
        this->delayCallback(1000);

      } else if (this->isConnected()) {
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

        if (this->isApEnabled() && millis() - this->connectedTime > this->reconnectInterval && !this->hasApClients()) {
          Log.sinfoln(FPSTR(L_NETWORK), F("Stop AP because STA connected"));

          WiFi.mode(WIFI_STA);
          return;
        }

        #ifdef ARDUINO_ARCH_ESP8266
        if (millis() - this->prevArpGratuitous > 60000) {
          this->stationKeepAliveNow();
          this->prevArpGratuitous = millis();
        }
        #endif

      } else {
        if (this->connected) {
          this->disconnectedTime = millis();
          this->connected = false;

          Log.sinfoln(
            FPSTR(L_NETWORK),
            F("Disconnected, reason: %d, uptime: %lu s."),
            NetworkConnection::getDisconnectReason(),
            (millis() - this->connectedTime) / 1000
          );
        }

        if (!this->hasStaCredentials() && !this->isApEnabled()) {
          Log.sinfoln(FPSTR(L_NETWORK), F("No STA credentials, start AP"));

          WiFi.mode(WIFI_AP_STA);
          this->delayCallback(250);
          WiFi.softAP(this->apName, this->apPassword, this->apChannel);

        } else if (!this->isApEnabled() && millis() - this->disconnectedTime > this->failedConnectTimeout) {
          Log.sinfoln(FPSTR(L_NETWORK), F("Disconnected for a long time, start AP"));

          WiFi.mode(WIFI_AP_STA);
          this->delayCallback(250);
          WiFi.softAP(this->apName, this->apPassword, this->apChannel);

        } else if (this->isConnecting() && millis() - this->prevReconnectingTime > this->resetConnectionTimeout) {
          Log.swarningln(FPSTR(L_NETWORK), F("Connection timeout, reset wifi..."));
          this->resetWifi();
          NetworkConnection::reset();
          this->delayCallback(250);

        } else if (!this->isConnecting() && this->hasStaCredentials() && (!this->prevReconnectingTime || millis() - this->prevReconnectingTime > this->reconnectInterval)) {
          Log.sinfoln(FPSTR(L_NETWORK), F("Try connect..."));

          NetworkConnection::reset();
          if (!this->connect(true, this->connectionTimeout)) {
            Log.straceln(FPSTR(L_NETWORK), F("Connection failed. Status: %d, reason: %d, raw reason: %d"), NetworkConnection::getStatus(), NetworkConnection::getDisconnectReason(), NetworkConnection::rawDisconnectReason);
          }

          this->prevReconnectingTime = millis();
        }
      }
    }

    static uint8_t rssiToSignalQuality(short int rssi) {
      return constrain(map(rssi, -100, -50, 0, 100), 0, 100);
    }

  protected:
    const unsigned int reconnectInterval = 15000;
    const unsigned int failedConnectTimeout = 185000;
    const unsigned int connectionTimeout = 10000;
    const unsigned int resetConnectionTimeout = 90000;

    YieldCallback yieldCallback = []() {
      ::yield();
    };
    DelayCallback delayCallback = [](unsigned int time) {
      ::delay(time);
    };

    const char* hostname = "esp";
    const char* apName = "ESP";
    const char* apPassword = nullptr;
    uint8_t apChannel = 1;

    const char* staSsid = nullptr;
    const char* staPassword = nullptr;
    uint8_t staChannel = 0;

    bool useDhcp = true;
    IPAddress staticIp;
    IPAddress staticGateway;
    IPAddress staticSubnet;
    IPAddress staticDns;

    bool connected = false;
    bool reconnectFlag = false;
    unsigned long prevArpGratuitous = 0;
    unsigned long prevReconnectingTime = 0;
    unsigned long connectedTime = 0;
    unsigned long disconnectedTime = 0;

    bool ethEnabled = false;
    bool ethStarted = false;
    bool ethActive = false;
    bool ethLinkUp = false;
    bool ethRestartFlag = false;
    bool ethStartUnavailable = false;
    uint8_t ethClk = GPIO_IS_NOT_CONFIGURED;
    uint8_t ethMosi = GPIO_IS_NOT_CONFIGURED;
    uint8_t ethMiso = GPIO_IS_NOT_CONFIGURED;
    uint8_t ethCs = GPIO_IS_NOT_CONFIGURED;
    uint8_t ethIrq = GPIO_IS_NOT_CONFIGURED;
    uint8_t ethRst = GPIO_IS_NOT_CONFIGURED;
    unsigned long ethPrevLinkCheck = 0;
    const unsigned long ethLinkCheckInterval = 1000;

    // WiFi and ethernet never share the uplink: the radio is parked while ethernet is active
    bool wifiRadioOff = false;
    unsigned long ethActiveSince = 0;
    const unsigned long wifiRadioOffDelay = 5000;


    #ifdef NETWORK_HAS_ETHERNET
    /**
     * @brief Park the WiFi radio while ethernet carries the uplink
     *
     * esp_wifi_stop() parks the radio without tearing the driver and its netifs down.
     * WiFi.mode(WIFI_OFF) does the same job but also deinitialises, which panicked when it
     * was called from this task with lwIP live — keep the driver allocated.
     */
    void suspendWifiRadio() {
      if (this->wifiRadioOff || !this->hasStaCredentials()) {
        return;
      }

      // nobody configuring over the fallback AP, and a driver that is actually running
      if (this->isApEnabled() && this->hasApClients()) {
        return;
      }

      if (WiFi.getMode() == WIFI_MODE_NULL) {
        return;
      }

      Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: uplink active, switching the WiFi radio off"));

      esp_wifi_stop();
      this->wifiRadioOff = true;
    }

    /**
     * @brief Bring the radio back when ethernet no longer carries the uplink
     */
    void resumeWifiRadio() {
      if (!this->wifiRadioOff) {
        return;
      }

      this->wifiRadioOff = false;

      Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: uplink lost, switching the WiFi radio back on"));

      esp_wifi_start();

      this->connected = false;
      this->disconnectedTime = millis();
      this->prevReconnectingTime = 0;

      NetworkConnection::reset();

      if (this->hasStaCredentials()) {
        WiFi.begin(this->staSsid, this->staPassword);

        Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: reconnecting to WiFi \"%s\""), this->staSsid);
      }
    }

    /**
     * @brief Keep the radio state in step with the uplink
     */
    void updateWifiRadio() {
      if (this->ethActive && this->ethLinkUp) {
        if (!this->ethActiveSince) {
          this->ethActiveSince = millis();
          return;
        }

        if (millis() - this->ethActiveSince >= this->wifiRadioOffDelay) {
          this->suspendWifiRadio();
        }

        return;
      }

      this->ethActiveSince = 0;
      this->resumeWifiRadio();
    }

    /**
     * @brief Start the W5500, watch the cable and hand the uplink to ethernet when it comes up
     */
    void loopEthernet() {
      if (this->ethRestartFlag) {
        this->ethRestartFlag = false;

        // The W5500 driver cannot be re-initialized in place: Arduino's ETH.end() leaves
        // the SPI bus allocated, so a wiring change only takes effect after a restart.
        if (this->ethStarted) {
          Log.swarningln(FPSTR(L_NETWORK), F("Ethernet: settings changed, restart required to apply them"));
        }
      }

      if (!this->ethEnabled || !this->hasEthernetPins()) {
        if (this->ethActive || this->ethLinkUp) {
          this->ethActive = false;
          this->ethLinkUp = false;

          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: disabled, handing the uplink back to WiFi"));

          NetworkConnection::reset();
          this->prevReconnectingTime = 0;
          this->disconnectedTime = millis();
          this->connected = false;
        }

        this->resumeWifiRadio();

        return;
      }

      if (!this->ethStarted) {
        if (this->ethStartUnavailable) {
          this->resumeWifiRadio();

          return;
        }

        Log.sinfoln(
          FPSTR(L_NETWORK),
          F("Ethernet: init W5500 (CS: %u, IRQ: %u, RST: %u, CLK: %u, MISO: %u, MOSI: %u)"),
          this->ethCs,
          this->ethIrq,
          this->ethRst,
          this->ethClk,
          this->ethMiso,
          this->ethMosi
        );

        if (!ETH.begin(ETH_PHY_W5500, 1, this->ethCs, this->ethIrq, this->ethRst, SPI2_HOST, this->ethClk, this->ethMiso, this->ethMosi, 12)) {
          // A failed init leaves the SPI bus allocated, so retrying cannot succeed;
          // wait for a restart instead of flapping.
          this->ethStartUnavailable = true;

          Log.serrorln(FPSTR(L_NETWORK), F("Ethernet: W5500 init failed, check the wiring and restart"));

          return;
        }

        this->ethStarted = true;
        this->ethPrevLinkCheck = 0;

        if (!this->useDhcp) {
          ETH.config(this->staticIp, this->staticGateway, this->staticSubnet, this->staticDns);
        }

        if (strlen(this->hostname) && this->isHostnameValid(this->hostname)) {
          ETH.setHostname(this->hostname);
        }

        return;
      }

      if (millis() - this->ethPrevLinkCheck < this->ethLinkCheckInterval) {
        return;
      }

      this->ethPrevLinkCheck = millis();

      // one place that keeps the radio in step with the uplink, whatever the branch below decides
      this->updateWifiRadio();

      if (!ETH.linkUp()) {
        if (this->ethLinkUp) {
          this->ethLinkUp = false;

          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: cable disconnected"));
        }

        if (this->ethActive) {
          this->ethActive = false;

          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: link lost, falling back to WiFi"));

          NetworkConnection::reset();
          this->prevReconnectingTime = 0;
          this->disconnectedTime = millis();
          this->connected = false;
          this->delayCallback(250);
        }

        return;
      }

      if (!this->ethLinkUp) {
        this->ethLinkUp = true;

        Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: cable connected"));
      }

      if (!ETH.connected() || this->ethActive) {
        return;
      }

      this->ethActive = true;
      this->connected = false;
      this->disconnectedTime = millis();

      Log.sinfoln(
        FPSTR(L_NETWORK),
        F("Ethernet: using ethernet instead of WiFi, IP: %s, link: %u Mbps, duplex: %u"),
        ETH.localIP().toString().c_str(),
        ETH.linkSpeed(),
        ETH.fullDuplex() ? 1 : 0
      );

      // Ethernet is the primary uplink. updateWifiRadio() parks the WiFi radio once the link
      // has been stable for a few seconds, and brings it back when the link drops.
      NetworkConnection::reset();
    }

    bool hasEthernetPins() {
      return this->ethClk != GPIO_IS_NOT_CONFIGURED
        && this->ethMosi != GPIO_IS_NOT_CONFIGURED
        && this->ethMiso != GPIO_IS_NOT_CONFIGURED
        && this->ethCs != GPIO_IS_NOT_CONFIGURED
        && this->ethIrq != GPIO_IS_NOT_CONFIGURED
        && this->ethRst != GPIO_IS_NOT_CONFIGURED;
    }
    #endif


    bool setWifiHostname(const char* hostname) {
      if (!this->isHostnameValid(hostname)) {
        return false;
      }

      if (strcmp(WiFi.getHostname(), hostname) == 0) {
        return true;
      }

      return WiFi.setHostname(hostname);
    }

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
}