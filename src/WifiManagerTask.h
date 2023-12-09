#define WM_MDNS
#include <WiFiManager.h>
#include <UnsignedIntParameter.h>
#include <UnsignedShortParameter.h>
#include <CheckboxParameter.h>
#include <SeparatorParameter.h>
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
CheckboxParameter* wmOtModSyncWithHeating;

UnsignedIntParameter* wmOutdoorSensorPin;
UnsignedIntParameter* wmIndoorSensorPin;

CheckboxParameter* wmExtPumpUse;
UnsignedIntParameter* wmExtPumpPin;
UnsignedShortParameter* wmExtPumpPostCirculationTime;
UnsignedIntParameter* wmExtPumpAntiStuckInterval;
UnsignedShortParameter* wmExtPumpAntiStuckTime;

SeparatorParameter* wmSep;

extern EEManager eeSettings;
#if USE_TELNET
  extern BufferedTelnetStream TelnetStream;
#endif

const char S_WIFI[]          PROGMEM = "WIFI";
const char S_WIFI_SETTINGS[] PROGMEM = "WIFI.SETTINGS";


class WifiManagerTask : public Task {
public:
  WifiManagerTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

  WifiManagerTask* addTaskForDisable(AbstractTask* task) {
    this->tasksForDisable.push_back(task);
    return this;
  }

protected:
  bool connected = false;
  unsigned long lastArpGratuitous = 0;
  std::vector<AbstractTask*> tasksForDisable;

  const char* getTaskName() {
    return "WifiManager";
  }
  
  /*int getTaskCore() {
    return 1;
  }*/

  int getTaskPriority() {
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

    wmSep = new SeparatorParameter();

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

    wm.addParameter(wmSep);

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

    wmOtModSyncWithHeating = new CheckboxParameter("ot_mod_sync_with_heating", "Modulation sync with heating", settings.opentherm.modulationSyncWithHeating);
    wm.addParameter(wmOtModSyncWithHeating);

    wm.addParameter(wmSep);

    wmOutdoorSensorPin = new UnsignedIntParameter("outdoor_sensor_pin", "Outdoor sensor GPIO", settings.sensors.outdoor.pin, 2);
    wm.addParameter(wmOutdoorSensorPin);

    wmIndoorSensorPin = new UnsignedIntParameter("indoor_sensor_pin", "Indoor sensor GPIO", settings.sensors.indoor.pin, 2);
    wm.addParameter(wmIndoorSensorPin);

    wm.addParameter(wmSep);

    wmExtPumpUse = new CheckboxParameter("ext_pump_use", "Use external pump", settings.externalPump.use);
    wm.addParameter(wmExtPumpUse);

    wmExtPumpPin = new UnsignedIntParameter("ext_pump_pin", "External pump GPIO", settings.externalPump.pin, 2);
    wm.addParameter(wmExtPumpPin);

    wmExtPumpPostCirculationTime = new UnsignedShortParameter("ext_pump_ps_time", "External pump post circulation time", settings.externalPump.postCirculationTime, 5);
    wm.addParameter(wmExtPumpPostCirculationTime);

    wmExtPumpAntiStuckInterval = new UnsignedIntParameter("ext_pump_as_interval", "External pump anti stuck interval", settings.externalPump.antiStuckInterval, 7);
    wm.addParameter(wmExtPumpAntiStuckInterval);

    wmExtPumpAntiStuckTime = new UnsignedShortParameter("ext_pump_as_time", "External pump anti stuck time", settings.externalPump.antiStuckTime, 5);
    wm.addParameter(wmExtPumpAntiStuckTime);

    //wm.setCleanConnect(true);
    wm.setRestorePersistent(false);

    wm.setHostname(settings.hostname);
    wm.setWiFiAutoReconnect(true);
    wm.setAPClientCheck(true);
    wm.setConfigPortalBlocking(false);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setPreOtaUpdateCallback([this] {
      for (AbstractTask* task : this->tasksForDisable) {
        if (task->isEnabled()) {
          task->disable();
        }
      }
      this->delay(10);
    });
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

      Log.sinfoln(FPSTR(S_WIFI), F("Disconnected"));

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

      Log.sinfoln(FPSTR(S_WIFI), F("Connected. IP: %s, RSSI: %hh"), WiFi.localIP().toString().c_str(), WiFi.RSSI());
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

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New hostname: %s"), settings.hostname);
    }

    if (strcmp(wmMqttServer->getValue(), settings.mqtt.server) != 0) {
      changed = true;
      strcpy(settings.mqtt.server, wmMqttServer->getValue());

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New mqtt.server: %s"), settings.mqtt.server);
    }

    if (wmMqttPort->getValue() != settings.mqtt.port) {
      changed = true;
      settings.mqtt.port = wmMqttPort->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New mqtt.port: %du"), settings.mqtt.port);
    }

    if (strcmp(wmMqttUser->getValue(), settings.mqtt.user) != 0) {
      changed = true;
      strcpy(settings.mqtt.user, wmMqttUser->getValue());

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New mqtt.user: %s"), settings.mqtt.user);
    }

    if (strcmp(wmMqttPassword->getValue(), settings.mqtt.password) != 0) {
      changed = true;
      strcpy(settings.mqtt.password, wmMqttPassword->getValue());

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New mqtt.password: %s"), settings.mqtt.password);
    }

    if (strcmp(wmMqttPrefix->getValue(), settings.mqtt.prefix) != 0) {
      changed = true;
      strcpy(settings.mqtt.prefix, wmMqttPrefix->getValue());

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New mqtt.prefix: %s"), settings.mqtt.prefix);
    }

    if (wmMqttPublishInterval->getValue() != settings.mqtt.interval) {
      changed = true;
      settings.mqtt.interval = wmMqttPublishInterval->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New mqtt.interval: %du"), settings.mqtt.interval);
    }

    if (wmOtInPin->getValue() != settings.opentherm.inPin) {
      changed = true;
      needRestart = true;
      settings.opentherm.inPin = wmOtInPin->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.inPin: %hhu"), settings.opentherm.inPin);
    }

    if (wmOtOutPin->getValue() != settings.opentherm.outPin) {
      changed = true;
      needRestart = true;
      settings.opentherm.outPin = wmOtOutPin->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.outPin: %hhu"), settings.opentherm.outPin);
    }

    if (wmOtMemberIdCode->getValue() != settings.opentherm.memberIdCode) {
      changed = true;
      settings.opentherm.memberIdCode = wmOtMemberIdCode->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.memberIdCode: %du"), settings.opentherm.memberIdCode);
    }

    if (wmOtDhwPresent->getCheckboxValue() != settings.opentherm.dhwPresent) {
      changed = true;
      settings.opentherm.dhwPresent = wmOtDhwPresent->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.dhwPresent: %s"), settings.opentherm.dhwPresent ? "on" : "off");
    }

    if (wmOtSummerWinterMode->getCheckboxValue() != settings.opentherm.summerWinterMode) {
      changed = true;
      settings.opentherm.summerWinterMode = wmOtSummerWinterMode->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.summerWinterMode: %s"), settings.opentherm.summerWinterMode ? "on" : "off");
    }

    if (wmOtHeatingCh2Enabled->getCheckboxValue() != settings.opentherm.heatingCh2Enabled) {
      changed = true;
      settings.opentherm.heatingCh2Enabled = wmOtHeatingCh2Enabled->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.heatingCh2Enabled: %s"), settings.opentherm.heatingCh2Enabled ? "on" : "off");

      if (settings.opentherm.heatingCh1ToCh2) {
        settings.opentherm.heatingCh1ToCh2 = false;
        wmOtHeatingCh1ToCh2->setValue(false);

        Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.heatingCh1ToCh2: %s"), settings.opentherm.heatingCh1ToCh2 ? "on" : "off");
      }

      if (settings.opentherm.dhwToCh2) {
        settings.opentherm.dhwToCh2 = false;
        wmOtDhwToCh2->setValue(false);

        Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.dhwToCh2: %s"), settings.opentherm.dhwToCh2 ? "on" : "off");
      }
    }

    if (wmOtHeatingCh1ToCh2->getCheckboxValue() != settings.opentherm.heatingCh1ToCh2) {
      changed = true;
      settings.opentherm.heatingCh1ToCh2 = wmOtHeatingCh1ToCh2->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.heatingCh1ToCh2: %s"), settings.opentherm.heatingCh1ToCh2 ? "on" : "off");

      if (settings.opentherm.heatingCh2Enabled) {
        settings.opentherm.heatingCh2Enabled = false;
        wmOtHeatingCh2Enabled->setValue(false);

        Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.heatingCh2Enabled: %s"), settings.opentherm.heatingCh2Enabled ? "on" : "off");
      }

      if (settings.opentherm.dhwToCh2) {
        settings.opentherm.dhwToCh2 = false;
        wmOtDhwToCh2->setValue(false);

        Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.dhwToCh2: %s"), settings.opentherm.dhwToCh2 ? "on" : "off");
      }
    }

    if (wmOtDhwToCh2->getCheckboxValue() != settings.opentherm.dhwToCh2) {
      changed = true;
      settings.opentherm.dhwToCh2 = wmOtDhwToCh2->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.dhwToCh2: %s"), settings.opentherm.dhwToCh2 ? "on" : "off");

      if (settings.opentherm.heatingCh2Enabled) {
        settings.opentherm.heatingCh2Enabled = false;
        wmOtHeatingCh2Enabled->setValue(false);

        Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.heatingCh2Enabled: %s"), settings.opentherm.heatingCh2Enabled ? "on" : "off");
      }

      if (settings.opentherm.heatingCh1ToCh2) {
        settings.opentherm.heatingCh1ToCh2 = false;
        wmOtHeatingCh1ToCh2->setValue(false);

        Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.heatingCh1ToCh2: %s"), settings.opentherm.heatingCh1ToCh2 ? "on" : "off");
      }
    }

    if (wmOtDhwBlocking->getCheckboxValue() != settings.opentherm.dhwBlocking) {
      changed = true;
      settings.opentherm.dhwBlocking = wmOtDhwBlocking->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.dhwBlocking: %s"), settings.opentherm.dhwBlocking ? "on" : "off");
    }

    if (wmOtModSyncWithHeating->getCheckboxValue() != settings.opentherm.modulationSyncWithHeating) {
      changed = true;
      settings.opentherm.modulationSyncWithHeating = wmOtModSyncWithHeating->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New opentherm.modulationSyncWithHeating: %s"), settings.opentherm.modulationSyncWithHeating ? "on" : "off");
    }

    if (wmOutdoorSensorPin->getValue() != settings.sensors.outdoor.pin) {
      changed = true;
      needRestart = true;
      settings.sensors.outdoor.pin = wmOutdoorSensorPin->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New sensors.outdoor.pin: %hhu"), settings.sensors.outdoor.pin);
    }

    if (wmIndoorSensorPin->getValue() != settings.sensors.indoor.pin) {
      changed = true;
      needRestart = true;
      settings.sensors.indoor.pin = wmIndoorSensorPin->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New sensors.indoor.pin: %hhu"), settings.sensors.indoor.pin);
    }

    if (wmExtPumpUse->getCheckboxValue() != settings.externalPump.use) {
      changed = true;
      settings.externalPump.use = wmExtPumpUse->getCheckboxValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New externalPump.use: %s"), settings.externalPump.use ? "on" : "off");
    }

    if (wmExtPumpPin->getValue() != settings.externalPump.pin) {
      changed = true;
      needRestart = true;
      settings.externalPump.pin = wmExtPumpPin->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New externalPump.pin: %hhu"), settings.externalPump.pin);
    }

    if (wmExtPumpPostCirculationTime->getValue() != settings.externalPump.postCirculationTime) {
      changed = true;
      settings.externalPump.postCirculationTime = wmExtPumpPostCirculationTime->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New externalPump.postCirculationTime: %hu"), settings.externalPump.postCirculationTime);
    }

    if (wmExtPumpAntiStuckInterval->getValue() != settings.externalPump.antiStuckInterval) {
      changed = true;
      settings.externalPump.antiStuckInterval = wmExtPumpAntiStuckInterval->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New externalPump.antiStuckInterval: %du"), settings.externalPump.antiStuckInterval);
    }

    if (wmExtPumpAntiStuckTime->getValue() != settings.externalPump.antiStuckTime) {
      changed = true;
      settings.externalPump.antiStuckTime = wmExtPumpAntiStuckTime->getValue();

      Log.sinfoln(FPSTR(S_WIFI_SETTINGS), F("New externalPump.antiStuckTime: %hu"), settings.externalPump.antiStuckTime);
    }

    if (!changed) {
      return;
    } else if (needRestart) {
      vars.actions.restart = true;
    }

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