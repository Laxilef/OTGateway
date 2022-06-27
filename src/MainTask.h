extern MqttTask* tMqtt;
extern SensorsTask* tSensors;

class MainTask : public Task {
public:
  MainTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

protected:

  void loop() {
    static unsigned long lastHeapInfo = 0;
    static unsigned short minFreeHeapSize = 65535;

    if (eeSettings.tick()) {
      INFO("Settings updated (EEPROM)");
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (!tMqtt->isEnabled()) {
        tMqtt->enable();
      }

    } else {
      if (tMqtt->isEnabled()) {
        tMqtt->disable();
      }

      vars.states.emergency = true;
    }

    if (!tSensors->isEnabled() && settings.outdoorTempSource == 2) {
      tSensors->enable();
    } else if (tSensors->isEnabled() && settings.outdoorTempSource != 2) {
      tSensors->disable();
    }

#ifdef USE_TELNET
    yield();

    // anti memory leak
    TelnetStream.flush();
    while (TelnetStream.available() > 0) {
      TelnetStream.read();
    }
#endif

    if (settings.debug) {
      unsigned short freeHeapSize = ESP.getFreeHeap();
      unsigned short minFreeHeapSizeDiff = 0;

      if (freeHeapSize < minFreeHeapSize) {
        minFreeHeapSizeDiff = minFreeHeapSize - freeHeapSize;
        minFreeHeapSize = freeHeapSize;
      }
      if (millis() - lastHeapInfo > 10000 || minFreeHeapSizeDiff > 0) {
        DEBUG_F("Free heap size: %hu bytes, min: %hu bytes (diff: %hu bytes)\n", freeHeapSize, minFreeHeapSize, minFreeHeapSizeDiff);
        lastHeapInfo = millis();
      }
    }
  }
};