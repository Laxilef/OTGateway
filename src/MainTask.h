#include <Blinker.h>

extern NetworkTask* tNetwork;
extern MqttTask* tMqtt;
extern OpenThermTask* tOt;
extern FileData fsSettings, fsNetworkSettings;
#if USE_TELNET
  extern ESPTelnetStream TelnetStream;
#endif


class MainTask : public Task {
public:
  MainTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {
    this->blinker = new Blinker();
  }

  ~MainTask() {
    if (this->blinker != nullptr) {
      delete this->blinker;
    }
  }

protected:
  const static byte REASON_PUMP_START_HEATING = 1;
  const static byte REASON_PUMP_START_ANTISTUCK = 2;

  Blinker* blinker = nullptr;
  bool blinkerInitialized = false;
  unsigned long firstFailConnect = 0;
  unsigned long lastHeapInfo = 0;
  unsigned int minFreeHeapSize = 0;
  unsigned int minMaxFreeHeapBlockSize = 0;
  unsigned long restartSignalTime = 0;
  bool heatingEnabled = false;
  unsigned long heatingDisabledTime = 0;
  byte externalPumpStartReason;
  unsigned long externalPumpStartTime = 0;
#if USE_TELNET
  bool telnetStarted = false;
#endif

  const char* getTaskName() {
    return "Main";
  }

  /*int getTaskCore() {
    return 1;
  }*/

  int getTaskPriority() {
    return 3;
  }

  void setup() {
    #ifdef LED_STATUS_PIN
      pinMode(LED_STATUS_PIN, OUTPUT);
      digitalWrite(LED_STATUS_PIN, false);
    #endif

    if (settings.externalPump.pin != 0) {
      pinMode(settings.externalPump.pin, OUTPUT);
      digitalWrite(settings.externalPump.pin, false);
    }

    this->minFreeHeapSize = getTotalHeap();
    this->minMaxFreeHeapBlockSize = getTotalHeap();
  }

  void loop() {
    if (fsSettings.tick() == FD_WRITE) {
      Log.sinfoln(FPSTR(L_SETTINGS), F("Updated"));
    }

    if (fsNetworkSettings.tick() == FD_WRITE) {
      Log.sinfoln(FPSTR(L_NETWORK_SETTINGS), F("Updated"));
    }

    #if USE_TELNET
    if (this->telnetStarted) {
      TelnetStream.loop();
    }
    #endif

    if (vars.actions.restart) {
      Log.sinfoln(FPSTR(L_MAIN), F("Restart signal received. Restart after 10 sec."));
      fsSettings.updateNow();
      fsNetworkSettings.updateNow();
      this->restartSignalTime = millis();
      vars.actions.restart = false;
    }

    if (!tOt->isEnabled() && settings.opentherm.inPin > 0 && settings.opentherm.outPin > 0 && settings.opentherm.inPin != settings.opentherm.outPin) {
      tOt->enable();
    }

    if (tNetwork->isConnected()) {
      #if USE_TELNET
      if (!this->telnetStarted) {
        TelnetStream.begin(23, false);
        this->telnetStarted = true;
      }
      #endif

      vars.sensors.rssi = WiFi.RSSI();

      if (!tMqtt->isEnabled() && strlen(settings.mqtt.server) > 0) {
        tMqtt->enable();
      }

      if (this->firstFailConnect != 0) {
        this->firstFailConnect = 0;
      }

      if ( Log.getLevel() != TinyLogger::Level::INFO && !settings.debug ) {
        Log.setLevel(TinyLogger::Level::INFO);

      } else if ( Log.getLevel() != TinyLogger::Level::VERBOSE && settings.debug ) {
        Log.setLevel(TinyLogger::Level::VERBOSE);
      }

    } else {
      #if USE_TELNET
      if (this->telnetStarted) {
        TelnetStream.stop();
        this->telnetStarted = false;
      }
      #endif

      if (tMqtt->isEnabled()) {
        tMqtt->disable();
      }

      if (settings.emergency.enable && !vars.states.emergency) {
        if (this->firstFailConnect == 0) {
          this->firstFailConnect = millis();
        }

        if (millis() - this->firstFailConnect > EMERGENCY_TIME_TRESHOLD) {
          vars.states.emergency = true;
          Log.sinfoln(FPSTR(L_MAIN), F("Emergency mode enabled"));
        }
      }
    }

    yield();
    #ifdef LED_STATUS_PIN
      ledStatus(LED_STATUS_PIN);
    #endif
    externalPump();

    // anti memory leak
    yield();
    for (Stream* stream : Log.getStreams()) {
      while (stream->available() > 0) {
        stream->read();
      }
    }

    heap();
    
    if (this->restartSignalTime > 0 && millis() - this->restartSignalTime > 10000) {
      this->restartSignalTime = 0;
      ESP.restart();
    }
  }

  void heap() {
    unsigned int freeHeapSize = getFreeHeap();
    #if defined(ARDUINO_ARCH_ESP32)
      unsigned int maxFreeBlockSize = ESP.getMaxAllocHeap();
    #else
      unsigned int maxFreeBlockSize = ESP.getMaxFreeBlockSize();
    #endif

    if (freeHeapSize < 1024 || maxFreeBlockSize < 1024) {
      vars.actions.restart = true;
      return;
    }

    if (!settings.debug) {
      return;
    }

    unsigned int minFreeHeapSizeDiff = 0;
    #if defined(ARDUINO_ARCH_ESP32)
    unsigned int currentMinFreeHeapSize = ESP.getMinFreeHeap();
    if (currentMinFreeHeapSize < this->minFreeHeapSize) {
      minFreeHeapSizeDiff = this->minFreeHeapSize - currentMinFreeHeapSize;
      this->minFreeHeapSize = currentMinFreeHeapSize;
    }
    #else
    if (freeHeapSize < this->minFreeHeapSize) {
      minFreeHeapSizeDiff = this->minFreeHeapSize - freeHeapSize;
      this->minFreeHeapSize = freeHeapSize;
    }
    #endif

    unsigned int minMaxFreeBlockSizeDiff = 0;
    if (maxFreeBlockSize < this->minMaxFreeHeapBlockSize) {
      minMaxFreeBlockSizeDiff = this->minMaxFreeHeapBlockSize - maxFreeBlockSize;
      this->minMaxFreeHeapBlockSize = maxFreeBlockSize;
    }

    uint8_t heapFrag = 100 - maxFreeBlockSize * 100.0 / freeHeapSize;
    if (millis() - this->lastHeapInfo > 20000 || minFreeHeapSizeDiff > 0 || minMaxFreeBlockSizeDiff > 0) {
      Log.sverboseln(
        FPSTR(L_MAIN),
        F("Free heap size: %u of %u bytes (min: %u, diff: %u), max free block: %u (min: %u, diff: %u, frag: %hhu%%)"),
        freeHeapSize, getTotalHeap(), this->minFreeHeapSize, minFreeHeapSizeDiff, maxFreeBlockSize, this->minMaxFreeHeapBlockSize, minMaxFreeBlockSizeDiff, heapFrag
      );
      this->lastHeapInfo = millis();
    }
  }

  void ledStatus(uint8_t ledPin) {
    uint8_t errors[4];
    uint8_t errCount = 0;
    static uint8_t errPos = 0;
    static unsigned long endBlinkTime = 0;
    static bool ledOn = false;

    if (!this->blinkerInitialized) {
      this->blinker->init(ledPin);
      this->blinkerInitialized = true;
    }

    if (!tNetwork->isConnected()) {
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

  void externalPump() {
    if (!vars.states.heating && this->heatingEnabled) {
      this->heatingEnabled = false;
      this->heatingDisabledTime = millis();

    } else if (vars.states.heating && !this->heatingEnabled) {
      this->heatingEnabled = true;
    }
    
    if (!settings.externalPump.use || settings.externalPump.pin == 0) {
      if (vars.states.externalPump) {
        if (settings.externalPump.pin != 0) {
          digitalWrite(settings.externalPump.pin, false);
        }

        vars.states.externalPump = false;
        vars.parameters.extPumpLastEnableTime = millis();

        Log.sinfoln("EXTPUMP", F("Disabled: use = off"));
      }

      return;
    }

    if (vars.states.externalPump && !this->heatingEnabled) {
      if (this->externalPumpStartReason == MainTask::REASON_PUMP_START_HEATING && millis() - this->heatingDisabledTime > ((unsigned int) settings.externalPump.postCirculationTime * 1000)) {
        digitalWrite(settings.externalPump.pin, false);

        vars.states.externalPump = false;
        vars.parameters.extPumpLastEnableTime = millis();

        Log.sinfoln("EXTPUMP", F("Disabled: expired post circulation time"));

      } else if (this->externalPumpStartReason == MainTask::REASON_PUMP_START_ANTISTUCK && millis() - this->externalPumpStartTime >= ((unsigned int) settings.externalPump.antiStuckTime * 1000)) {
        digitalWrite(settings.externalPump.pin, false);

        vars.states.externalPump = false;
        vars.parameters.extPumpLastEnableTime = millis();

        Log.sinfoln("EXTPUMP", F("Disabled: expired anti stuck time"));
      }

    } else if (vars.states.externalPump && this->heatingEnabled && this->externalPumpStartReason == MainTask::REASON_PUMP_START_ANTISTUCK) {
      this->externalPumpStartReason = MainTask::REASON_PUMP_START_HEATING;

    } else if (!vars.states.externalPump && this->heatingEnabled) {
      vars.states.externalPump = true;
      this->externalPumpStartTime = millis();
      this->externalPumpStartReason = MainTask::REASON_PUMP_START_HEATING;

      digitalWrite(settings.externalPump.pin, true);

      Log.sinfoln("EXTPUMP", F("Enabled: heating on"));

    } else if (!vars.states.externalPump && (vars.parameters.extPumpLastEnableTime == 0 || millis() - vars.parameters.extPumpLastEnableTime >= ((unsigned long) settings.externalPump.antiStuckInterval * 1000))) {
      vars.states.externalPump = true;
      this->externalPumpStartTime = millis();
      this->externalPumpStartReason = MainTask::REASON_PUMP_START_ANTISTUCK;

      digitalWrite(settings.externalPump.pin, true);

      Log.sinfoln("EXTPUMP", F("Enabled: anti stuck"));
    }
  }
};