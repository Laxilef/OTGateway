#include <Arduino.h>
#include "defines.h"
#include <ArduinoJson.h>
#include <EEManager.h>
#include <TinyLogger.h>
#include "Settings.h"

#if USE_TELNET
  #include "ESPTelnetStream.h"
#endif

#if defined(ESP32)
  #include <ESP32Scheduler.h>
#elif defined(ESP8266)
  #include <Scheduler.h>
#elif
  #error Wrong board. Supported boards: esp8266, esp32
#endif

#include <Task.h>
#include <LeanTask.h>
#include "WifiManagerTask.h"
#include "MqttTask.h"
#include "OpenThermTask.h"
#include "SensorsTask.h"
#include "RegulatorTask.h"
#include "MainTask.h"

// Vars
EEManager eeSettings(settings, 60000);
#if USE_TELNET
  ESPTelnetStream TelnetStream;
#endif

// Tasks
WifiManagerTask* tWm;
MqttTask* tMqtt;
OpenThermTask* tOt;
SensorsTask* tSensors;
RegulatorTask* tRegulator;
MainTask* tMain;


void setup() {
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
  
  #if USE_SERIAL
    Serial.begin(115200);
    Serial.println("\n\n");
    Log.addStream(&Serial);
  #endif

  #if USE_TELNET
    TelnetStream.setKeepAliveInterval(500);
    Log.addStream(&TelnetStream);
  #endif

  EEPROM.begin(eeSettings.blockSize());
  uint8_t eeSettingsResult = eeSettings.begin(0, 's');
  if (eeSettingsResult == 0) {
    Log.sinfoln("MAIN", F("Settings loaded"));

    if (strcmp(SETTINGS_VALID_VALUE, settings.validationValue) != 0) {
      Log.swarningln("MAIN", F("Settings not valid, reset and restart..."));
      eeSettings.reset();
      delay(5000);
      ESP.restart();
    }

  } else if (eeSettingsResult == 1) {
    Log.sinfoln("MAIN", F("Settings NOT loaded, first start"));

  } else if (eeSettingsResult == 2) {
    Log.serrorln("MAIN", F("Settings NOT loaded (error)"));
  }

  Log.setLevel(settings.debug ? TinyLogger::Level::VERBOSE : TinyLogger::Level::INFO);
  
  tWm = new WifiManagerTask(true, 0);
  Scheduler.start(tWm);

  tMqtt = new MqttTask(false, 100);
  Scheduler.start(tMqtt);

  tOt = new OpenThermTask(false, 1000);
  Scheduler.start(tOt);

  tSensors = new SensorsTask(true, EXT_SENSORS_INTERVAL);
  Scheduler.start(tSensors);

  tRegulator = new RegulatorTask(true, 10000);
  Scheduler.start(tRegulator);

  tMain = new MainTask(true, 100);
  Scheduler.start(tMain);

  tWm
    ->addTaskForDisable(tMain)
    ->addTaskForDisable(tMqtt)
    ->addTaskForDisable(tOt)
    ->addTaskForDisable(tSensors)
    ->addTaskForDisable(tRegulator);
    
  Scheduler.begin();
}

void loop() {
  #if defined(ESP32)
    vTaskDelete(NULL);
  #endif
}
