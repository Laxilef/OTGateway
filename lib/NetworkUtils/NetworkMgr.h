#include <WiFi.h>
#include <esp_wifi.h>
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

    uint8_t getStaChannel() {
      return this->staChannel;
    }

    bool resetWifi() {
      // set policy manual for work 13 ch
      {
        const wifi_country_t country = {"JP", 1, 14, CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER, WIFI_COUNTRY_POLICY_MANUAL};
        esp_wifi_set_country(&country);
      }

      WiFi.persistent(false);
      #if ESP_ARDUINO_VERSION_MAJOR < 3
      WiFi.setAutoConnect(false);
      #endif
      WiFi.setAutoReconnect(false);

      WiFi.setSleep(USE_BLE ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);

      WiFi.softAPdisconnect();

      WiFi.disconnect(false, true);

      #if ESP_ARDUINO_VERSION_MAJOR < 3
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
        this->disconnect();
      }

      if (!this->hasStaCredentials()) {
        return false;
      }

      this->delayCallback(250);

      if (this->setWifiHostname(this->hostname)) {
        Log.straceln(L_NETWORK, "Set hostname '%s': success", this->hostname);

      } else {
        Log.serrorln(L_NETWORK, "Set hostname '%s': fail", this->hostname);
      }

      if (!WiFi.mode((WiFiMode_t)(WiFi.getMode() | WIFI_STA))) {
        return false;
      }

      this->delayCallback(250);

      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
      WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

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
      WiFi.disconnectAsync(false, true);

      const unsigned long start = millis();
      while (WiFi.isConnected() && (millis() - start) < 5000) {
        this->delayCallback(100);
      }
    }

    void loop() {
      if (this->reconnectFlag) {
        this->delayCallback(5000);

        Log.sinfoln(L_NETWORK, "Reconnecting...");
        this->reconnectFlag = false;
        this->disconnect();
        NetworkConnection::reset();
        this->delayCallback(1000);

      } else if (this->isConnected() && !this->hasStaCredentials()) {
        Log.sinfoln(L_NETWORK, "Reset");
        this->resetWifi();
        NetworkConnection::reset();
        this->delayCallback(1000);

      } else if (this->isConnected()) {
        if (!this->connected) {
          this->connectedTime = millis();
          this->connected = true;

          Log.sinfoln(
            L_NETWORK,
            "Connected, downtime: %lu s., IP: %s, RSSI: %hhd",
            (millis() - this->disconnectedTime) / 1000,
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI()
          );
        }

        if (this->isApEnabled() && millis() - this->connectedTime > this->reconnectInterval && !this->hasApClients()) {
          Log.sinfoln(L_NETWORK, "Stop AP because STA connected");

          WiFi.mode(WIFI_STA);
          return;
        }

      } else {
        if (this->connected) {
          this->disconnectedTime = millis();
          this->connected = false;

          Log.sinfoln(
            L_NETWORK,
            "Disconnected, reason: %d, uptime: %lu s.",
            NetworkConnection::getDisconnectReason(),
            (millis() - this->connectedTime) / 1000
          );
        }

        if (!this->hasStaCredentials() && !this->isApEnabled()) {
          Log.sinfoln(L_NETWORK, "No STA credentials, start AP");

          WiFi.mode(WIFI_AP_STA);
          this->delayCallback(250);
          WiFi.softAP(this->apName, this->apPassword, this->apChannel);

        } else if (!this->isApEnabled() && millis() - this->disconnectedTime > this->failedConnectTimeout) {
          Log.sinfoln(L_NETWORK, "Disconnected for a long time, start AP");

          WiFi.mode(WIFI_AP_STA);
          this->delayCallback(250);
          WiFi.softAP(this->apName, this->apPassword, this->apChannel);

        } else if (this->isConnecting() && millis() - this->prevReconnectingTime > this->resetConnectionTimeout) {
          Log.swarningln(L_NETWORK, "Connection timeout, reset wifi...");
          this->resetWifi();
          NetworkConnection::reset();
          this->delayCallback(250);

        } else if (!this->isConnecting() && this->hasStaCredentials() && (!this->prevReconnectingTime || millis() - this->prevReconnectingTime > this->reconnectInterval)) {
          Log.sinfoln(L_NETWORK, "Try connect...");

          NetworkConnection::reset();
          if (!this->connect(true, this->connectionTimeout)) {
            Log.straceln(L_NETWORK, "Connection failed. Status: %d, reason: %d, raw reason: %d", NetworkConnection::getStatus(), NetworkConnection::getDisconnectReason(), NetworkConnection::rawDisconnectReason);
          }

          this->prevReconnectingTime = millis();
        }
      }
    }

    static uint8_t rssiToSignalQuality(short int rssi) {
      return constrain(map(rssi, -100, -50, 0, 100), 0, 100);
    }

    static bool isHostnameValid(const char* value) {
      if (value == nullptr) {
        return false;
      }

      size_t len = strlen(value);
      if (len == 0 || len > 24 || value[0] == '-' || value[len - 1] == '-') {
        return false;
      }

      for (size_t i = 0; i < len; i++) {
        const char symbol = value[i];

        // Lowercase ASCII letters.
        if (symbol >= 'a' && symbol <= 'z') {
          continue;
        }

        // Uppercase ASCII letters.
        if (symbol >= 'A' && symbol <= 'Z') {
          continue;
        }

        // Decimal digits.
        if (symbol >= '0' && symbol <= '9') {
          continue;
        }

        // Hyphens are allowed inside the hostname.
        if (symbol == '-') {
          continue;
        }

        // All other characters are invalid.
        return false;
      }

      return true;
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

  };
}
