#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include "lwip/etharp.h"
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
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

    NetworkMgr* setApCredentials(const char* ssid, const char* password = nullptr, byte channel = 0) {
      this->apName = ssid;
      this->apPassword = password;
      this->apChannel = channel;

      return this;
    }

    NetworkMgr* setStaCredentials(const char* ssid = nullptr, const char* password = nullptr, byte channel = 0) {
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

    bool hasStaCredentials() {
      return this->staSsid != nullptr;
    }

    bool isConnected() {
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

    bool hasApClients() {
      if (!this->isApEnabled()) {
        return false;
      }

      return WiFi.softAPgetStationNum() > 0;
    }

    short int getRssi() {
      return WiFi.RSSI();
    }

    IPAddress getApIp() {
      return WiFi.softAPIP();
    }

    IPAddress getStaIp() {
      return WiFi.localIP();
    }

    IPAddress getStaSubnet() {
      return WiFi.subnetMask();
    }

    IPAddress getStaGateway() {
      return WiFi.gatewayIP();
    }

    IPAddress getStaDns() {
      return WiFi.dnsIP();
    }

    String getStaMac() {
      return WiFi.macAddress();
    }

    const char* getStaSsid() {
      return this->staSsid;
    }

    const char* getStaPassword() {
      return this->staPassword;
    }

    byte getStaChannel() {
      return this->staChannel;
    }

    bool resetWifi() {
      // set policy manual for work 13 ch
      {
        #ifdef ARDUINO_ARCH_ESP8266
        wifi_country_t country = {"CN", 1, 13, WIFI_COUNTRY_POLICY_AUTO};
        wifi_set_country(&country);
        #elif defined(ARDUINO_ARCH_ESP32)
        const wifi_country_t country = {"CN", 1, 13, CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER, WIFI_COUNTRY_POLICY_AUTO};
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

    static byte rssiToSignalQuality(short int rssi) {
      return constrain(map(rssi, -100, -50, 0, 100), 0, 100);
    }

  protected:
    const unsigned int reconnectInterval = 15000;
    const unsigned int failedConnectTimeout = 185000;
    const unsigned int connectionTimeout = 5000;
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
    byte apChannel = 1;

    const char* staSsid = nullptr;
    const char* staPassword = nullptr;
    byte staChannel = 0;

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