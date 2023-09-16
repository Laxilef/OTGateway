extern MqttTask* tMqtt;
extern SensorsTask* tSensors;
extern OpenThermTask* tOt;

class MainTask: public Task {
public:
  MainTask(bool _enabled = false, unsigned long _interval = 0): Task(_enabled, _interval) {}

protected:
  unsigned long lastHeapInfo = 0;
  unsigned long firstFailConnect = 0;
  unsigned short minFreeHeapSize = 65535;

  void setup() {
    pinMode(LED_STATUS_PIN, OUTPUT);
    //pinMode(LED_OT_RX_PIN, OUTPUT);
  }

  void loop() {
    if (eeSettings.tick()) {
      INFO("Settings updated (EEPROM)");
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (!tMqtt->isEnabled()) {
        tMqtt->enable();
      }

      if ( firstFailConnect != 0 ) {
        firstFailConnect = 0;
      }

      vars.states.rssi = WiFi.RSSI();

    } else {
      if (tMqtt->isEnabled()) {
        tMqtt->disable();
      }

      if (settings.emergency.enable && !vars.states.emergency) {
        if (firstFailConnect == 0) {
          firstFailConnect = millis();
        }
        
        if (millis() - firstFailConnect > EMERGENCY_TIME_TRESHOLD) {
          vars.states.emergency = true;
          INFO("Emergency mode enabled");
        }
      }
    }

    if (!tSensors->isEnabled() && settings.outdoorTempSource == 2) {
      tSensors->enable();
    } else if (tSensors->isEnabled() && settings.outdoorTempSource != 2) {
      tSensors->disable();
    }

    if (!tOt->isEnabled() && settings.opentherm.inPin > 0 && settings.opentherm.outPin > 0 && settings.opentherm.inPin != settings.opentherm.outPin) {
      tOt->enable();
    }

    ledStatus();

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

  void ledStatus() {
    static byte blinkLeft = 0;
    static bool ledOn = false;
    static unsigned long changeTime = 0;

    byte errNo = 0;
    if (!vars.states.otStatus) {
      errNo = 1;
    } else if (vars.states.fault) {
      errNo = 2;
    } else if (vars.states.emergency) {
      errNo = 3;
    }

    if (errNo == 0) {
      if (!ledOn) {
        digitalWrite(LED_STATUS_PIN, true);
        ledOn = true;
      }

      if (blinkLeft > 0) {
        blinkLeft = 0;
      }

    } else {
      if (blinkLeft == 0) {
        if (ledOn) {
          digitalWrite(LED_STATUS_PIN, false);
          ledOn = false;
          changeTime = millis();
        }

        if (millis() - changeTime >= 3000) {
          blinkLeft = errNo;
        }
      }

      if (blinkLeft > 0 && millis() - changeTime >= 500) {
        if (ledOn) {
          digitalWrite(LED_STATUS_PIN, false);
          ledOn = false;
          blinkLeft--;

        } else {
          digitalWrite(LED_STATUS_PIN, true);
          ledOn = true;
        }

        changeTime = millis();
      }
    }
  }
};