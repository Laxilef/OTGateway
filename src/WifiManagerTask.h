#include <WiFiManager.h>

// Wifimanager
WiFiManager wm;
WiFiManagerParameter* wmHostname;
WiFiManagerParameter* wmMqttServer;
WiFiManagerParameter* wmMqttPort;
WiFiManagerParameter* wmMqttUser;
WiFiManagerParameter* wmMqttPassword;
WiFiManagerParameter* wmMqttPrefix;
WiFiManagerParameter* wmMqttPublishInterval;
WiFiManagerParameter* wmOtInPin;
WiFiManagerParameter* wmOtOutPin;
WiFiManagerParameter* wmOtMemberIdCode;
WiFiManagerParameter* wmOutdoorSensorPin;
WiFiManagerParameter* wmIndoorSensorPin;

class WifiManagerTask: public Task {
public:
  WifiManagerTask(bool _enabled = false, unsigned long _interval = 0): Task(_enabled, _interval) {}

protected:
  void setup() {
    //WiFi.mode(WIFI_STA);
    wm.setDebugOutput(settings.debug);

    wmHostname = new WiFiManagerParameter("hostname", "Hostname", settings.hostname, 80);
    wm.addParameter(wmHostname);

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

    sprintf(buffer, "%d", settings.mqtt.interval);
    wmMqttPublishInterval = new WiFiManagerParameter("mqtt_publish_interval", "MQTT publish interval", buffer, 5);
    wm.addParameter(wmMqttPublishInterval);

    sprintf(buffer, "%d", settings.opentherm.inPin);
    wmOtInPin = new WiFiManagerParameter("ot_in_pin", "Opentherm pin IN", buffer, 2);
    wm.addParameter(wmOtInPin);

    sprintf(buffer, "%d", settings.opentherm.outPin);
    wmOtOutPin = new WiFiManagerParameter("ot_out_pin", "Opentherm pin OUT", buffer, 2);
    wm.addParameter(wmOtOutPin);

    sprintf(buffer, "%d", settings.opentherm.memberIdCode);
    wmOtMemberIdCode = new WiFiManagerParameter("ot_member_id_code", "Opentherm member id", buffer, 5);
    wm.addParameter(wmOtMemberIdCode);

    sprintf(buffer, "%d", settings.sensors.outdoor.pin);
    wmOutdoorSensorPin = new WiFiManagerParameter("outdoor_sensor_pin", "Outdoor sensor pin", buffer, 2);
    wm.addParameter(wmOutdoorSensorPin);

    sprintf(buffer, "%d", settings.sensors.indoor.pin);
    wmIndoorSensorPin = new WiFiManagerParameter("indoor_sensor_pin", "Indoor sensor pin", buffer, 2);
    wm.addParameter(wmIndoorSensorPin);

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
    strcpy(settings.mqtt.server, wmMqttServer->getValue());
    settings.mqtt.port = atoi(wmMqttPort->getValue());
    strcpy(settings.mqtt.user, wmMqttUser->getValue());
    strcpy(settings.mqtt.password, wmMqttPassword->getValue());
    strcpy(settings.mqtt.prefix, wmMqttPrefix->getValue());
    settings.mqtt.interval = atoi(wmMqttPublishInterval->getValue());
    settings.opentherm.inPin = atoi(wmOtInPin->getValue());
    settings.opentherm.outPin = atoi(wmOtOutPin->getValue());
    settings.opentherm.memberIdCode = atoi(wmOtMemberIdCode->getValue());
    settings.sensors.outdoor.pin = atoi(wmOutdoorSensorPin->getValue());
    settings.sensors.indoor.pin = atoi(wmIndoorSensorPin->getValue());

    INFO_F(
      "New settings:\r\n"
      "  Hostname: %s\r\n"
      "  Mqtt server: %s:%d\r\n"
      "  Mqtt user: %s\r\n"
      "  Mqtt pass: %s\r\n"
      "  Mqtt prefix: %s\r\n"
      "  Mqtt publish interval: %d\r\n"
      "  OT in pin: %d\r\n"
      "  OT out pin: %d\r\n"
      "  OT member id code: %d\r\n"
      "  Outdoor sensor pin: %d\r\n"
      "  Indoor sensor pin: %d\r\n",
      settings.hostname,
      settings.mqtt.server,
      settings.mqtt.port,
      settings.mqtt.user,
      settings.mqtt.password,
      settings.mqtt.prefix,
      settings.mqtt.interval,
      settings.opentherm.inPin,
      settings.opentherm.outPin,
      settings.opentherm.memberIdCode,
      settings.sensors.outdoor.pin,
      settings.sensors.indoor.pin
    );
    eeSettings.updateNow();
    INFO(F("Settings saved"));
  }

  bool connected = false;
};