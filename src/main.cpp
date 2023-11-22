#include <Arduino.h>
#include "defines.h"
#include <ArduinoJson.h>
#include <TelnetStream.h>
#include <EEManager.h>
#include <TinyLogger.h>
#include "Settings.h"

EEManager eeSettings(settings, 30000);

#if defined(ESP32)
  #include <ESP32Scheduler.h>
  #include <Task.h>
  #include <LeanTask.h>
#elif defined(ESP8266)
  #include <Scheduler.h>
  #include <Task.h>
  #include <LeanTask.h>
#elif
  #error Wrong board. Supported boards: esp8266, esp32
#endif

#include "WifiManagerTask.h"
#include "MqttTask.h"
#include "OpenThermTask.h"
#include "SensorsTask.h"
#include "RegulatorTask.h"
#include "MainTask.h"

// Tasks
WifiManagerTask* tWm;
MqttTask* tMqtt;
OpenThermTask* tOt;
SensorsTask* tSensors;
RegulatorTask* tRegulator;
MainTask* tMain;


void setup() {
  #if USE_TELNET
  TelnetStream.begin();
  Log.begin(&TelnetStream, TinyLogger::Level::VERBOSE);
  delay(1000);
  #else
  Serial.begin(115200);
  Log.begin(&Serial, TinyLogger::Level::VERBOSE);
  Serial.println("\n\n");
  #endif
  //Log.setNtpClient(&timeClient);

  EEPROM.begin(eeSettings.blockSize());
  uint8_t eeSettingsResult = eeSettings.begin(0, 's');
  if (eeSettingsResult == 0) {
    Log.sinfoln("MAIN", "Settings loaded");

    if (strcmp(SETTINGS_VALID_VALUE, settings.validationValue) != 0) {
      Log.swarningln("MAIN", "Settings not valid, reset and restart...");
      eeSettings.reset();
      delay(1000);
      ESP.restart();
    }

  } else if (eeSettingsResult == 1) {
    Log.sinfoln("MAIN", "Settings NOT loaded, first start");

  } else if (eeSettingsResult == 2) {
    Log.serrorln("MAIN", "Settings NOT loaded (error)");
  }

  Log.setLevel(settings.debug ? TinyLogger::Level::VERBOSE : TinyLogger::Level::INFO);

  tWm = new WifiManagerTask(true);
  Scheduler.start(tWm);

  tMqtt = new MqttTask(false);
  Scheduler.start(tMqtt);

  tOt = new OpenThermTask(false);
  Scheduler.start(tOt);

  tSensors = new SensorsTask(true, EXT_SENSORS_INTERVAL);
  Scheduler.start(tSensors);

  tRegulator = new RegulatorTask(true, 10000);
  Scheduler.start(tRegulator);

  tMain = new MainTask(true, 50);
  Scheduler.start(tMain);

  Scheduler.begin();
}

void loop() {
  #if defined(ESP32)
    vTaskDelete(NULL);
  #endif
}
