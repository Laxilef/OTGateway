#define WM_MDNS
#include <WiFiManager.h>
#include <WiFiManagerParameters.h>
#include <netif/etharp.h>

WiFiManager wm;
WiFiManagerParameter* wmHostname;
WiFiManagerParameter* wmMqttServer;
UnsignedIntParameter* wmMqttPort;
WiFiManagerParameter* wmMqttUser;
WiFiManagerParameter* wmMqttPassword;
WiFiManagerParameter* wmMqttPrefix;
UnsignedIntParameter* wmMqttPublishInterval;
UnsignedIntParameter* wmOtInPin;
UnsignedIntParameter* wmOtOutPin;
UnsignedIntParameter* wmOtMemberIdCode;
CheckboxParameter* wmOtDhwPresent;
CheckboxParameter* wmOtSummerWinterMode;
CheckboxParameter* wmOtHeatingCh2Enabled;
CheckboxParameter* wmOtHeatingCh1ToCh2;
CheckboxParameter* wmOtDhwToCh2;
CheckboxParameter* wmOtDhwBlocking;
UnsignedIntParameter* wmOutdoorSensorPin;
UnsignedIntParameter* wmIndoorSensorPin;

SeparatorParameter* wmSep1;
SeparatorParameter* wmSep2;

extern EEManager eeSettings;
#if USE_TELNET
  extern ESPTelnetStream TelnetStream;
#endif


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
    #ifdef WOKWI
      WiFi.begin("Wokwi-GUEST", "", 6);
    #endif

    wm.setDebugOutput(settings.debug, (wm_debuglevel_t) WM_DEBUG_MODE);
    wm.setTitle(PROJECT_NAME);
    wm.setCustomMenuHTML(PSTR(
      "<style>.wrap h1 {display: none;} .wrap h3 {display: none;} .nh {margin: 0 0 1em 0;} .nh .logo {font-size: 1.8em; margin: 0.5em; text-align: center;} .nh .links {text-align: center;}</style>"
      "<div class=\"nh\">"
      "<div class=\"logo\">" PROJECT_NAME "</div>"
      "<div class=\"links\"><a href=\"" PROJECT_REPO "\" target=\"_blank\">Repo</a> | <a href=\"" PROJECT_REPO "/issues\" target=\"_blank\">Issues</a> | <a href=\"" PROJECT_REPO "/releases\" target=\"_blank\">Releases</a> | <small>v" PROJECT_VERSION " (" __DATE__ ")</small></div>"
      "</div>"
    ));

    std::vector<const char *> menu = {"custom", "wifi", "param", "sep", "info", "update", "restart"};
    wm.setMenu(menu);

    wmHostname = new WiFiManagerParameter("hostname", "Hostname", settings.hostname, 80);
    wm.addParameter(wmHostname);

    wmMqttServer = new WiFiManagerParameter("mqtt_server", "MQTT server", settings.mqtt.server, 80);
    wm.addParameter(wmMqttServer);

    wmMqttPort = new UnsignedIntParameter("mqtt_port", "MQTT port", settings.mqtt.port, 6);
    wm.addParameter(wmMqttPort);

    wmMqttUser = new WiFiManagerParameter("mqtt_user", "MQTT username", settings.mqtt.user, 32);
    wm.addParameter(wmMqttUser);

    wmMqttPassword = new WiFiManagerParameter("mqtt_password", "MQTT password", settings.mqtt.password, 32, "type=\"password\"");
    wm.addParameter(wmMqttPassword);

    wmMqttPrefix = new WiFiManagerParameter("mqtt_prefix", "MQTT prefix", settings.mqtt.prefix, 32);
    wm.addParameter(wmMqttPrefix);

    wmMqttPublishInterval = new UnsignedIntParameter("mqtt_publish_interval", "MQTT publish interval", settings.mqtt.interval, 5);
    wm.addParameter(wmMqttPublishInterval);

    wmSep1 = new SeparatorParameter();
    wm.addParameter(wmSep1);

    wmOtInPin = new UnsignedIntParameter("ot_in_pin", "Opentherm GPIO IN", settings.opentherm.inPin, 2);
    wm.addParameter(wmOtInPin);

    wmOtOutPin = new UnsignedIntParameter("ot_out_pin", "Opentherm GPIO OUT", settings.opentherm.outPin, 2);
    wm.addParameter(wmOtOutPin);

    wmOtMemberIdCode = new UnsignedIntParameter("ot_member_id_code", "Opentherm Master Member ID", settings.opentherm.memberIdCode, 5);
    wm.addParameter(wmOtMemberIdCode);

    wmOtDhwPresent = new CheckboxParameter("ot_dhw_present", "Opentherm DHW present", settings.opentherm.dhwPresent);
    wm.addParameter(wmOtDhwPresent);

    wmOtSummerWinterMode = new CheckboxParameter("ot_summer_winter_mode", "Opentherm summer/winter mode", settings.opentherm.summerWinterMode);
    wm.addParameter(wmOtSummerWinterMode);

    wmOtHeatingCh2Enabled = new CheckboxParameter("ot_heating_ch2_enabled", "Opentherm CH2 enabled", settings.opentherm.heatingCh2Enabled);
    wm.addParameter(wmOtHeatingCh2Enabled);

    wmOtHeatingCh1ToCh2 = new CheckboxParameter("ot_heating_ch1_to_ch2", "Opentherm heating CH1 to CH2", settings.opentherm.heatingCh1ToCh2);
    wm.addParameter(wmOtHeatingCh1ToCh2);

    wmOtDhwToCh2 = new CheckboxParameter("ot_dhw_to_ch2", "Opentherm DHW to CH2", settings.opentherm.dhwToCh2);
    wm.addParameter(wmOtDhwToCh2);

    wmOtDhwBlocking = new CheckboxParameter("ot_dhw_blocking", "Opentherm DHW blocking", settings.opentherm.dhwBlocking);
    wm.addParameter(wmOtDhwBlocking);

    wmSep2 = new SeparatorParameter();
    wm.addParameter(wmSep2);

    wmOutdoorSensorPin = new UnsignedIntParameter("outdoor_sensor_pin", "Outdoor sensor GPIO", settings.sensors.outdoor.pin, 2);
    wm.addParameter(wmOutdoorSensorPin);

    wmIndoorSensorPin = new UnsignedIntParameter("indoor_sensor_pin", "Indoor sensor GPIO", settings.sensors.indoor.pin, 2);
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

      wm.setCaptivePortalEnable(true);

      if (!wm.getConfigPortalActive()) {
        wm.startConfigPortal();
      }

      #if USE_TELNET
        TelnetStream.stop();
      #endif

      Log.sinfoln("WIFI", PSTR("Disconnected"));

    } else if (!connected && WiFi.status() == WL_CONNECTED) {
      connected = true;

      wm.setConfigPortalTimeout(180);
      if (wm.getConfigPortalActive()) {
        wm.stopConfigPortal();
      }

      wm.setCaptivePortalEnable(false);

      if (!wm.getWebPortalActive()) {
        wm.startWebPortal();
      }

      #if USE_TELNET
        TelnetStream.begin(23, false);
      #endif

      Log.sinfoln("WIFI", PSTR("Connected. IP: %s, RSSI: %d"), WiFi.localIP().toString().c_str(), WiFi.RSSI());
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

    if (wmOtDhwPresent->getCheckboxValue() != settings.opentherm.dhwPresent) {
      changed = true;
      settings.opentherm.dhwPresent = wmOtDhwPresent->getCheckboxValue();
    }

    if (wmOtSummerWinterMode->getCheckboxValue() != settings.opentherm.summerWinterMode) {
      changed = true;
      settings.opentherm.summerWinterMode = wmOtSummerWinterMode->getCheckboxValue();
    }

    if (wmOtHeatingCh2Enabled->getCheckboxValue() != settings.opentherm.heatingCh2Enabled) {
      changed = true;
      settings.opentherm.heatingCh2Enabled = wmOtHeatingCh2Enabled->getCheckboxValue();

      if (settings.opentherm.heatingCh1ToCh2) {
        settings.opentherm.heatingCh1ToCh2 = false;
        wmOtHeatingCh1ToCh2->setValue(false);
      }

      if (settings.opentherm.dhwToCh2) {
        settings.opentherm.dhwToCh2 = false;
        wmOtDhwToCh2->setValue(false);
      }
    }

    if (wmOtHeatingCh1ToCh2->getCheckboxValue() != settings.opentherm.heatingCh1ToCh2) {
      changed = true;
      settings.opentherm.heatingCh1ToCh2 = wmOtHeatingCh1ToCh2->getCheckboxValue();

      if (settings.opentherm.heatingCh2Enabled) {
        settings.opentherm.heatingCh2Enabled = false;
        wmOtHeatingCh2Enabled->setValue(false);
      }

      if (settings.opentherm.dhwToCh2) {
        settings.opentherm.dhwToCh2 = false;
        wmOtDhwToCh2->setValue(false);
      }
    }

    if (wmOtDhwToCh2->getCheckboxValue() != settings.opentherm.dhwToCh2) {
      changed = true;
      settings.opentherm.dhwToCh2 = wmOtDhwToCh2->getCheckboxValue();

      if (settings.opentherm.heatingCh2Enabled) {
        settings.opentherm.heatingCh2Enabled = false;
        wmOtHeatingCh2Enabled->setValue(false);
      }

      if (settings.opentherm.heatingCh1ToCh2) {
        settings.opentherm.heatingCh1ToCh2 = false;
        wmOtHeatingCh1ToCh2->setValue(false);
      }
    }

    if (wmOtDhwBlocking->getCheckboxValue() != settings.opentherm.dhwBlocking) {
      changed = true;
      settings.opentherm.dhwBlocking = wmOtDhwBlocking->getCheckboxValue();
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
      vars.actions.restart = true;
    }

    Log.sinfo(
      "WIFI",
      PSTR("New settings:\r\n"
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
      "  OT summer/winter mode: %d\r\n"
      "  OT heating ch2 enabled: %d\r\n"
      "  OT heating ch1 to ch2: %d\r\n"
      "  OT DHW to ch2: %d\r\n"
      "  OT DHW blocking: %d\r\n"
      "  Outdoor sensor pin: %d\r\n"
      "  Indoor sensor pin: %d\r\n"),
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
      settings.opentherm.summerWinterMode,
      settings.opentherm.heatingCh2Enabled,
      settings.opentherm.heatingCh1ToCh2,
      settings.opentherm.dhwToCh2,
      settings.opentherm.dhwBlocking,
      settings.sensors.outdoor.pin,
      settings.sensors.indoor.pin
    );

    eeSettings.update();
  }

  static void arpGratuitous() {
    struct netif* netif = netif_list;
    while (netif) {
      etharp_gratuitous(netif);
      netif = netif->next;
    }
  }
};