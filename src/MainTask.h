#include <Blinker.h>

extern MqttTask* tMqtt;
extern SensorsTask* tSensors;
extern OpenThermTask* tOt;

class MainTask : public Task {
public:
  MainTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

protected:
  Blinker* blinker = nullptr;
  unsigned long lastHeapInfo = 0;
  unsigned long firstFailConnect = 0;
  unsigned int heapSize = 0;
  unsigned int minFreeHeapSize = 0;

  const char* getTaskName() {
    return "Main";
  }

  int getTaskCore() {
    return 1;
  }

  void setup() {
    #ifdef LED_STATUS_PIN
      pinMode(LED_STATUS_PIN, OUTPUT);
      digitalWrite(LED_STATUS_PIN, false);
    #endif

    #if defined(ESP32)
      heapSize = ESP.getHeapSize();
    #elif defined(ESP8266)
      heapSize = 81920;
    #elif
      heapSize = 99999;
    #endif
    minFreeHeapSize = heapSize;
  }

  void loop() {
    if (eeSettings.tick()) {
      INFO("Settings updated (EEPROM)");
    }

    if (vars.parameters.restartAfterTime > 0 && millis() - vars.parameters.restartSignalTime > vars.parameters.restartAfterTime) {
      vars.parameters.restartAfterTime = 0;

      INFO("Received restart message...");
      eeSettings.updateNow();
      INFO("Restart...");
      delay(1000);

      ESP.restart();
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (!tMqtt->isEnabled() && strlen(settings.mqtt.server) > 0) {
        tMqtt->enable();
      }

      if (firstFailConnect != 0) {
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

    if (!tOt->isEnabled() && settings.opentherm.inPin > 0 && settings.opentherm.outPin > 0 && settings.opentherm.inPin != settings.opentherm.outPin) {
      tOt->enable();
    }

    #ifdef LED_STATUS_PIN
      ledStatus(LED_STATUS_PIN);
    #endif

    #if USE_TELNET
      yield();

      // anti memory leak
      TelnetStream.flush();
      while (TelnetStream.available() > 0) {
        TelnetStream.read();
      }
    #endif

    if (settings.debug) {
      unsigned int freeHeapSize = ESP.getFreeHeap();
      unsigned int minFreeHeapSizeDiff = 0;

      if (freeHeapSize < minFreeHeapSize) {
        minFreeHeapSizeDiff = minFreeHeapSize - freeHeapSize;
        minFreeHeapSize = freeHeapSize;
      }

      if (millis() - lastHeapInfo > 10000 || minFreeHeapSizeDiff > 0) {
        DEBUG_F("Free heap size: %u of %u bytes, min: %u bytes (diff: %u bytes)\n", freeHeapSize, heapSize, minFreeHeapSize, minFreeHeapSizeDiff);
        lastHeapInfo = millis();
      }
    }
  }

  void ledStatus(uint8_t ledPin) {
    uint8_t errors[4];
    uint8_t errCount = 0;
    static uint8_t errPos = 0;
    static unsigned long endBlinkTime = 0;
    static bool ledOn = false;

    if (this->blinker == nullptr) {
      this->blinker = new Blinker(ledPin);
    }

    if (WiFi.status() != WL_CONNECTED) {
      errors[errCount++] = 2;
    }

    if (!vars.states.otStatus) {
      errors[errCount++] = 3;
    }

    if (vars.states.fault) {
      errors[errCount++] = 4;
    }

    if (vars.states.emergency) {
      errors[errCount++] = 5;
    }


    if (this->blinker->ready()) {
      endBlinkTime = millis();
    }

    if (!this->blinker->running() && millis() - endBlinkTime >= 5000) {
      if (errCount == 0) {
        if (!ledOn) {
          digitalWrite(ledPin, true);
          ledOn = true;
        }

        return;

      } else if (ledOn) {
        digitalWrite(ledPin, false);
        ledOn = false;
        endBlinkTime = millis();
        return;
      }

      if (errPos >= errCount) {
        errPos = 0;

        // end of error list
        this->blinker->blink(10, 50, 50);

      } else {
        this->blinker->blink(errors[errPos++], 300, 300);
      }
    }

    this->blinker->tick();
  }
};