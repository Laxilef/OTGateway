#include <Arduino.h>
#include "defines.h"
#include "strings.h"
#include "CrashRecorder.h"
#include <ArduinoJson.h>
#include <FileData.h>
#include <LittleFS.h>
#include <ESPTelnetStream.h>
#include <TinyLogger.h>
#include <NetworkMgr.h>
#include "Settings.h"
#include "utils.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <ESP32Scheduler.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <Scheduler.h>
#else
  #error Wrong board. Supported boards: esp8266, esp32
#endif

#include <Task.h>
#include <LeanTask.h>
#include "MqttTask.h"
#include "OpenThermTask.h"
#include "SensorsTask.h"
#include "RegulatorTask.h"
#include "PortalTask.h"
#include "MainTask.h"

using namespace NetworkUtils;

// Vars
FileData fsNetworkSettings(&LittleFS, "/network.conf", 'n', &networkSettings, sizeof(networkSettings), 1000);
FileData fsSettings(&LittleFS, "/settings.conf", 's', &settings, sizeof(settings), 60000);
ESPTelnetStream* telnetStream = nullptr;
NetworkMgr* network = nullptr;

// Tasks
MqttTask* tMqtt;
OpenThermTask* tOt;
SensorsTask* tSensors;
RegulatorTask* tRegulator;
PortalTask* tPortal;
MainTask* tMain;


void setup() {
  CrashRecorder::init();
  LittleFS.begin();

  Log.setLevel(TinyLogger::Level::VERBOSE);
  Log.setServiceTemplate("\033[1m[%s]\033[22m");
  Log.setLevelTemplate("\033[1m[%s]\033[22m");
  Log.setMsgPrefix("\033[m ");
  Log.setDateTemplate("\033[1m[%H:%M:%S]\033[22m");
  Log.setDateCallback([] {
    unsigned int time = millis() / 1000;
    int sec = time % 60;
    int min = time % 3600 / 60;
    int hour = time / 3600;

    return tm{sec, min, hour};
  });
  
  Serial.begin(115200);
  Log.addStream(&Serial);
  Log.print("\n\n\r");

  // network settings
  switch (fsNetworkSettings.read()) {
    case FD_FS_ERR:
      Log.swarningln(FPSTR(L_NETWORK_SETTINGS), F("Filesystem error, load default"));
      break;
    case FD_FILE_ERR:
      Log.swarningln(FPSTR(L_NETWORK_SETTINGS), F("Bad data, load default"));
      break;
    case FD_WRITE:
      Log.sinfoln(FPSTR(L_NETWORK_SETTINGS), F("Not found, load default"));
      break;
    case FD_ADD:
    case FD_READ:
      Log.sinfoln(FPSTR(L_NETWORK_SETTINGS), F("Loaded"));
      break;
    default:
      break;
  }

  // settings
  switch (fsSettings.read()) {
    case FD_FS_ERR:
      Log.swarningln(FPSTR(L_SETTINGS), F("Filesystem error, load default"));
      break;
    case FD_FILE_ERR:
      Log.swarningln(FPSTR(L_SETTINGS), F("Bad data, load default"));
      break;
    case FD_WRITE:
      Log.sinfoln(FPSTR(L_SETTINGS), F("Not found, load default"));
      break;
    case FD_ADD:
    case FD_READ:
      Log.sinfoln(FPSTR(L_SETTINGS), F("Loaded"));

      if (strcmp(SETTINGS_VALID_VALUE, settings.validationValue) != 0) {
        Log.swarningln(FPSTR(L_SETTINGS), F("Not valid, set default and restart..."));
        fsSettings.reset();
        ::delay(5000);
        ESP.restart();
      }
      break;
    default:
      break;
  }

  // logs
  if (!settings.system.serial.enable) {
    Serial.end();
    Log.clearStreams();

  } else if (settings.system.serial.baudrate != 115200) {
    Serial.end();
    Log.clearStreams();

    Serial.begin(settings.system.serial.baudrate);
    Log.addStream(&Serial);
  }

  if (settings.system.telnet.enable) {
    telnetStream = new ESPTelnetStream;
    telnetStream->setKeepAliveInterval(500);
    Log.addStream(telnetStream);
  }

  if (settings.system.logLevel >= TinyLogger::Level::SILENT && settings.system.logLevel <= TinyLogger::Level::VERBOSE) {
    Log.setLevel(static_cast<TinyLogger::Level>(settings.system.logLevel));
  }

  // network
  network = (new NetworkMgr)
    ->setHostname(networkSettings.hostname)
    ->setStaCredentials(
      strlen(networkSettings.sta.ssid) ? networkSettings.sta.ssid : nullptr,
      strlen(networkSettings.sta.password) ? networkSettings.sta.password : nullptr,
      networkSettings.sta.channel
    )->setApCredentials(
      strlen(networkSettings.ap.ssid) ? networkSettings.ap.ssid : nullptr,
      strlen(networkSettings.ap.password) ? networkSettings.ap.password : nullptr,
      networkSettings.ap.channel
    )
    ->setUseDhcp(networkSettings.useDhcp)
    ->setStaticConfig(
      networkSettings.staticConfig.ip,
      networkSettings.staticConfig.gateway,
      networkSettings.staticConfig.subnet,
      networkSettings.staticConfig.dns
    );

  // tasks
  tMqtt = new MqttTask(false, 500);
  Scheduler.start(tMqtt);

  tOt = new OpenThermTask(true, 750);
  Scheduler.start(tOt);

  tSensors = new SensorsTask(true, EXT_SENSORS_INTERVAL);
  Scheduler.start(tSensors);

  tRegulator = new RegulatorTask(true, 10000);
  Scheduler.start(tRegulator);

  tPortal = new PortalTask(true, 0);
  Scheduler.start(tPortal);

  tMain = new MainTask(true, 100);
  Scheduler.start(tMain);

  Scheduler.begin();
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  vTaskDelete(NULL);
#endif
}
