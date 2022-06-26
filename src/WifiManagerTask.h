// #include <WiFiClient.h>
#define WM_MDNS
#include <WiFiManager.h>
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>

// Wifimanager
WiFiManager wm;
WiFiManagerParameter *wmHostname;
WiFiManagerParameter *wmMqttServer;
WiFiManagerParameter *wmMqttPort;
WiFiManagerParameter *wmMqttUser;
WiFiManagerParameter *wmMqttPassword;
WiFiManagerParameter *wmMqttPrefix;

class WifiManagerTask : public CustomTask {
public:
  WifiManagerTask(bool enabled = false, unsigned long interval = 0) : CustomTask(enabled, interval) {}

protected:
  void setup() {
    WiFi.mode(WIFI_STA);
    wm.setDebugOutput(settings.debug);

    wmHostname = new WiFiManagerParameter("hostname", "Hostname", settings.hostname, 80);
    wm.addParameter(wmHostname);

    wmMqttServer = new WiFiManagerParameter("mqtt_server", "MQTT server", settings.mqtt.server, 80);
    wm.addParameter(wmMqttServer);

    //char mqttPort[6];
    sprintf(buffer, "%d", settings.mqtt.port);
    wmMqttPort = new WiFiManagerParameter("mqtt_port", "MQTT port", buffer, 6);
    wm.addParameter(wmMqttPort);

    wmMqttUser = new WiFiManagerParameter("mqtt_user", "MQTT username", settings.mqtt.user, 32);
    wm.addParameter(wmMqttUser);

    wmMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT password", settings.mqtt.password, 32);
    wm.addParameter(wmMqttPassword);

    wmMqttPrefix = new WiFiManagerParameter("mqtt_prefix", "MQTT prefix", settings.mqtt.prefix, 32);
    wm.addParameter(wmMqttPrefix);

    wm.setHostname(settings.hostname);
    wm.setWiFiAutoReconnect(true);
    wm.setConfigPortalBlocking(false);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setConfigPortalTimeout(300);
    wm.setDisableConfigPortal(false);

    if (wm.autoConnect(AP_SSID)) {
      INFO_F("Wifi connected. IP: %s, RSSI: %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      wm.startWebPortal();

    } else {
      INFO(F("Failed to connect to WIFI, start the configuration portal..."));
    }
  }

  void loop() {
    wm.process();
  }

  void static saveParamsCallback() {
    strcpy(settings.hostname, (*wmHostname).getValue());
    strcpy(settings.mqtt.server, (*wmMqttServer).getValue());
    settings.mqtt.port = atoi((*wmMqttPort).getValue());
    strcpy(settings.mqtt.user, (*wmMqttUser).getValue());
    strcpy(settings.mqtt.password, (*wmMqttPassword).getValue());
    strcpy(settings.mqtt.prefix, (*wmMqttPrefix).getValue());

    INFO_F("Settings\nHostname: %s, Server: %s, port: %d, user: %s, pass: %s\n", settings.hostname, settings.mqtt.server, settings.mqtt.port, settings.mqtt.user, settings.mqtt.password);
    eeSettings.updateNow();
    INFO(F("Settings saved"));
  }
};