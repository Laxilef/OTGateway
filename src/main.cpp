#include <Arduino.h>
#include "defines.h"
#include <ArduinoJson.h>
#include <TelnetStream.h>
#include <EEManager.h>
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
    Serial.begin(115200);
    Serial.println("\n\n");
  #endif

  EEPROM.begin(eeSettings.blockSize());
  uint8_t eeSettingsResult = eeSettings.begin(0, 's');
  if (eeSettingsResult == 0) {
    INFO("Settings loaded");

    if (strcmp(SETTINGS_VALID_VALUE, settings.validationValue) != 0) {
      INFO("Settings not valid, reset and restart...");
      eeSettings.reset();
      delay(1000);
      ESP.restart();
    }

  } else if (eeSettingsResult == 1) {
    INFO("Settings NOT loaded, first start");

  } else if (eeSettingsResult == 2) {
    INFO("Settings NOT loaded (error)");
  }

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
