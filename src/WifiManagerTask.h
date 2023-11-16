#define WM_MDNS
#include <WiFiManager.h>
#include <WiFiManagerParameters.h>
#include <netif/etharp.h>


WiFiManager wm;
WiFiManagerParameter* wmHostname;
WiFiManagerParameter* wmMqttServer;
IntParameter* wmMqttPort;
WiFiManagerParameter* wmMqttUser;
WiFiManagerParameter* wmMqttPassword;
WiFiManagerParameter* wmMqttPrefix;
IntParameter* wmMqttPublishInterval;
IntParameter* wmOtInPin;
IntParameter* wmOtOutPin;
IntParameter* wmOtMemberIdCode;
CheckboxParameter* wmOtDHWPresent;
IntParameter* wmOutdoorSensorPin;
IntParameter* wmIndoorSensorPin;

SeparatorParameter* wmSep1;
SeparatorParameter* wmSep2;


class WifiManagerTask : public Task {
public:
  WifiManagerTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

protected:
  bool connected = false;
  unsigned long lastArpGratuitous = 0;

  const char* getTaskName() {
    return "WifiManager";
  }
  
  int getTaskCore() {
    return 0;
  }

  void setup() {
    wm.setDebugOutput(settings.debug);
    //wm.setDebugOutput(settings.debug, WM_DEBUG_VERBOSE);

    wmHostname = new WiFiManagerParameter("hostname", "Hostname", settings.hostname, 80);
    wm.addParameter(wmHostname);

    wmMqttServer = new WiFiManagerParameter("mqtt_server", "MQTT server", settings.mqtt.server, 80);
    wm.addParameter(wmMqttServer);

    wmMqttPort = new IntParameter("mqtt_port", "MQTT port", settings.mqtt.port, 6);
    wm.addParameter(wmMqttPort);

    wmMqttUser = new WiFiManagerParameter("mqtt_user", "MQTT username", settings.mqtt.user, 32);
    wm.addParameter(wmMqttUser);

    wmMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT password", settings.mqtt.password, 32, "type=\"password\"");
    wm.addParameter(wmMqttPassword);

    wmMqttPrefix = new WiFiManagerParameter("mqtt_prefix", "MQTT prefix", settings.mqtt.prefix, 32);
    wm.addParameter(wmMqttPrefix);

    wmMqttPublishInterval = new IntParameter("mqtt_publish_interval", "MQTT publish interval", settings.mqtt.interval, 5);
    wm.addParameter(wmMqttPublishInterval);

    wmSep1 = new SeparatorParameter();
    wm.addParameter(wmSep1);

    wmOtInPin = new IntParameter("ot_in_pin", "Opentherm pin IN", settings.opentherm.inPin, 2);
    wm.addParameter(wmOtInPin);

    wmOtOutPin = new IntParameter("ot_out_pin", "Opentherm pin OUT", settings.opentherm.outPin, 2);
    wm.addParameter(wmOtOutPin);

    wmOtMemberIdCode = new IntParameter("ot_member_id_code", "Opentherm member id", settings.opentherm.memberIdCode, 5);
    wm.addParameter(wmOtMemberIdCode);

    wmOtDHWPresent = new CheckboxParameter("ot_dhw_present", "Opentherm DHW present", settings.opentherm.dhwPresent);
    wm.addParameter(wmOtDHWPresent);

    wmSep2 = new SeparatorParameter();
    wm.addParameter(wmSep2);

    wmOutdoorSensorPin = new IntParameter("outdoor_sensor_pin", "Outdoor sensor pin", settings.sensors.outdoor.pin, 2);
    wm.addParameter(wmOutdoorSensorPin);

    wmIndoorSensorPin = new IntParameter("indoor_sensor_pin", "Indoor sensor pin", settings.sensors.indoor.pin, 2);
    wm.addParameter(wmIndoorSensorPin);

    //wm.setCleanConnect(true);
    wm.setRestorePersistent(false);

    wm.setHostname(settings.hostname);
    wm.setWiFiAutoReconnect(true);
    wm.setAPClientCheck(true);
    wm.setConfigPortalBlocking(false);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setConfigPortalTimeout(wm.getWiFiIsSaved() ? 180 : 0);
    wm.setDisableConfigPortal(false);

    wm.autoConnect(AP_SSID, AP_PASSWORD);
  }

  void loop() {
    if (connected && WiFi.status() != WL_CONNECTED) {
      connected = false;

      if (wm.getWebPortalActive()) {
        wm.stopWebPortal();
      }

      #ifdef USE_TELNET
      TelnetStream.stop();
      #endif

      INFO("[wifi] Disconnected");

    } else if (!connected && WiFi.status() == WL_CONNECTED) {
      connected = true;

      wm.setConfigPortalTimeout(180);
      if (wm.getConfigPortalActive()) {
        wm.stopConfigPortal();
      }

      if (!wm.getWebPortalActive()) {
        wm.startWebPortal();
      }

      #ifdef USE_TELNET
      TelnetStream.begin();
      #endif

      INFO_F("[wifi] Connected. IP address: %s, RSSI: %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }

    #if defined(ESP8266)
    if (connected && millis() - lastArpGratuitous > 60000) {
      arpGratuitous();
      lastArpGratuitous = millis();
    }
    #endif

    wm.process();
  }

  static void saveParamsCallback() {
    bool changed = false;
    bool needRestart = false;

    if (strcmp(wmHostname->getValue(), settings.hostname) != 0) {
      changed = true;
      needRestart = true;

      strcpy(settings.hostname, wmHostname->getValue());
    }

    if (strcmp(wmMqttServer->getValue(), settings.mqtt.server) != 0) {
      changed = true;

      strcpy(settings.mqtt.server, wmMqttServer->getValue());
    }

    if (wmMqttPort->getValue() != settings.mqtt.port) {
      changed = true;

      settings.mqtt.port = wmMqttPort->getValue();
    }

    if (strcmp(wmMqttUser->getValue(), settings.mqtt.user) != 0) {
      changed = true;

      strcpy(settings.mqtt.user, wmMqttUser->getValue());
    }

    if (strcmp(wmMqttPassword->getValue(), settings.mqtt.password) != 0) {
      changed = true;

      strcpy(settings.mqtt.password, wmMqttPassword->getValue());
    }

    if (strcmp(wmMqttPrefix->getValue(), settings.mqtt.prefix) != 0) {
      changed = true;

      strcpy(settings.mqtt.prefix, wmMqttPrefix->getValue());
    }

    if (wmMqttPublishInterval->getValue() != settings.mqtt.interval) {
      changed = true;

      settings.mqtt.interval = wmMqttPublishInterval->getValue();
    }

    if (wmOtInPin->getValue() != settings.opentherm.inPin) {
      changed = true;
      needRestart = true;

      settings.opentherm.inPin = wmOtInPin->getValue();
    }

    if (wmOtOutPin->getValue() != settings.opentherm.outPin) {
      changed = true;
      needRestart = true;

      settings.opentherm.outPin = wmOtOutPin->getValue();
    }

    if (wmOtMemberIdCode->getValue() != settings.opentherm.memberIdCode) {
      changed = true;

      settings.opentherm.memberIdCode = wmOtMemberIdCode->getValue();
    }

    if (wmOtDHWPresent->getCheckboxValue() != settings.opentherm.dhwPresent) {
      changed = true;

      settings.opentherm.dhwPresent = wmOtDHWPresent->getCheckboxValue();
    }

    if (wmOutdoorSensorPin->getValue() != settings.sensors.outdoor.pin) {
      changed = true;
      needRestart = true;

      settings.sensors.outdoor.pin = wmOutdoorSensorPin->getValue();
    }

    if (wmIndoorSensorPin->getValue() != settings.sensors.indoor.pin) {
      changed = true;
      needRestart = true;

      settings.sensors.indoor.pin = wmIndoorSensorPin->getValue();
    }

    if (!changed) {
      return;
    }

    if (needRestart) {
      vars.parameters.restartAfterTime = 5000;
      vars.parameters.restartSignalTime = millis();
    }

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
      "  OT DHW present: %d\r\n"
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
      settings.opentherm.dhwPresent,
      settings.sensors.outdoor.pin,
      settings.sensors.indoor.pin
    );

    eeSettings.updateNow();
    INFO(F("Settings saved"));
  }

  static void arpGratuitous() {
    struct netif* netif = netif_list;
    while (netif) {
      etharp_gratuitous(netif);
      netif = netif->next;
    }
  }
};