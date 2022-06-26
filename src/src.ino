#include "defines.h"
#include <ArduinoJson.h>
#include <TelnetStream.h>
#include <EEManager.h>
#include <Scheduler.h>
#include "Settings.h"

EEManager eeSettings(settings, 30000);

#include "lib/CustomTask.h"
#include "WifiManagerTask.h"
#include "MqttTask.h"
#include "OpenThermTask.h"
#include "MainTask.h"

// Tasks
WifiManagerTask* tWm;
MqttTask* tMqtt;
OpenThermTask* tOt;
MainTask* tMain;

void setup() {
#ifdef USE_TELNET
  TelnetStream.begin();
  delay(5000);
#else
  Serial.begin(115200);
  Serial.println("\n\n");
#endif

  EEPROM.begin(eeSettings.blockSize());
  uint8_t eeSettingsResult = eeSettings.begin(0, 's');
  if (eeSettingsResult == 0) {
    INFO("Settings loaded");

  } else if (eeSettingsResult == 1) {
    INFO("Settings NOT loaded, first start");

  } else if (eeSettingsResult == 2) {
    INFO("Settings NOT loaded (error)");
  }

  tWm = new WifiManagerTask(true);
  Scheduler.start(tWm);

  tMqtt = new MqttTask(false);
  Scheduler.start(tMqtt);

  tOt = new OpenThermTask(true);
  Scheduler.start(tOt);

  tMain = new MainTask(true);
  Scheduler.start(tMain);

  Scheduler.begin();
}

void loop() {}