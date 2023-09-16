#include <WiFiManager.h>

// Wifimanager
WiFiManager wm;
WiFiManagerParameter* wmHostname;
WiFiManagerParameter* wmOtInPin;
WiFiManagerParameter* wmOtOutPin;
WiFiManagerParameter* wmOtMemberIdCode;
WiFiManagerParameter* wmMqttServer;
WiFiManagerParameter* wmMqttPort;
WiFiManagerParameter* wmMqttUser;
WiFiManagerParameter* wmMqttPassword;
WiFiManagerParameter* wmMqttPrefix;

class WifiManagerTask: public Task {
public:
  WifiManagerTask(bool _enabled = false, unsigned long _interval = 0): Task(_enabled, _interval) {}

protected:
  void setup() {
    //WiFi.mode(WIFI_STA);
    wm.setDebugOutput(settings.debug);

    wmHostname = new WiFiManagerParameter("hostname", "Hostname", settings.hostname, 80);
    wm.addParameter(wmHostname);

    sprintf(buffer, "%d", settings.opentherm.inPin);
    wmOtInPin = new WiFiManagerParameter("ot_in_pin", "Opentherm pin IN", buffer, 1);
    wm.addParameter(wmOtInPin);

    sprintf(buffer, "%d", settings.opentherm.outPin);
    wmOtOutPin = new WiFiManagerParameter("ot_out_pin", "Opentherm pin OUT", buffer, 1);
    wm.addParameter(wmOtOutPin);

    sprintf(buffer, "%d", settings.opentherm.memberIdCode);
    wmOtMemberIdCode = new WiFiManagerParameter("ot_member_id_code", "Opentherm member id code", buffer, 5);
    wm.addParameter(wmOtMemberIdCode);

    wmMqttServer = new WiFiManagerParameter("mqtt_server", "MQTT server", settings.mqtt.server, 80);
    wm.addParameter(wmMqttServer);

    sprintf(buffer, "%d", settings.mqtt.port);
    wmMqttPort = new WiFiManagerParameter("mqtt_port", "MQTT port", buffer, 6);
    wm.addParameter(wmMqttPort);

    wmMqttUser = new WiFiManagerParameter("mqtt_user", "MQTT username", settings.mqtt.user, 32);
    wm.addParameter(wmMqttUser);

    wmMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT password", settings.mqtt.password, 32);
    wm.addParameter(wmMqttPassword);

    wmMqttPrefix = new WiFiManagerParameter("mqtt_prefix", "MQTT prefix", settings.mqtt.prefix, 32);
    wm.addParameter(wmMqttPrefix);

    //wm.setCleanConnect(true);
    wm.setRestorePersistent(false);

    wm.setHostname(settings.hostname);
    wm.setWiFiAutoReconnect(true);
    wm.setAPClientCheck(true);
    wm.setConfigPortalBlocking(false);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setConfigPortalTimeout(180);
    //wm.setDisableConfigPortal(false);

    wm.autoConnect(AP_SSID, AP_PASSWORD);
  }

  void loop() {
    static unsigned short prevCounterLiters = 0;

    /*if (WiFi.status() != WL_CONNECTED && !wm.getWebPortalActive() && !wm.getConfigPortalActive()) {
      wm.autoConnect(AP_SSID);
    }*/

    if (connected && WiFi.status() != WL_CONNECTED) {
      connected = false;
      INFO("[wifi] Disconnected");

    } else if (!connected && WiFi.status() == WL_CONNECTED) {
      connected = true;

      if (wm.getConfigPortalActive()) {
        wm.stopConfigPortal();
      }

      INFO_F("[wifi] Connected. IP address: %s, RSSI: %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }

    if (WiFi.status() == WL_CONNECTED && !wm.getWebPortalActive() && !wm.getConfigPortalActive()) {
      wm.startWebPortal();
    }

    wm.process();
  }

  void static saveParamsCallback() {
    strcpy(settings.hostname, wmHostname->getValue());
    settings.opentherm.inPin = atoi(wmOtInPin->getValue());
    settings.opentherm.outPin = atoi(wmOtOutPin->getValue());
    settings.opentherm.memberIdCode = atoi(wmOtMemberIdCode->getValue());
    strcpy(settings.mqtt.server, wmMqttServer->getValue());
    settings.mqtt.port = atoi(wmMqttPort->getValue());
    strcpy(settings.mqtt.user, wmMqttUser->getValue());
    strcpy(settings.mqtt.password, wmMqttPassword->getValue());
    strcpy(settings.mqtt.prefix, wmMqttPrefix->getValue());

    INFO_F(
      "New settings:\r\n"
      "  Hostname: %s\r\n"
      "  OT in pin: %d"
      "  OT out pin: %d"
      "  OT member id code: %d"
      "  Mqtt server: %s:%d\r\n"
      "  Mqtt user: %s\r\n"
      "  Mqtt pass: %s\r\n",
      settings.hostname,
      settings.opentherm.inPin,
      settings.opentherm.outPin,
      settings.opentherm.memberIdCode,
      settings.mqtt.server,
      settings.mqtt.port,
      settings.mqtt.user,
      settings.mqtt.password
    );
    eeSettings.updateNow();
    INFO(F("Settings saved"));
  }

  bool connected = false;
};