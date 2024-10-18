#include <Blinker.h>

using namespace NetworkUtils;

extern NetworkMgr* network;
extern MqttTask* tMqtt;
extern OpenThermTask* tOt;
extern FileData fsSettings, fsNetworkSettings;
extern ESPTelnetStream* telnetStream;


class MainTask : public Task {
public:
  MainTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {
    this->blinker = new Blinker();

    network->setDelayCallback([this](unsigned int time) {
      this->delay(time);
    })->setYieldCallback([this]() {
      this->yield();
    });
  }

  ~MainTask() {
    delete this->blinker;
  }

protected:
  enum class PumpStartReason {NONE, HEATING, ANTISTUCK};

  Blinker* blinker = nullptr;
  unsigned long lastHeapInfo = 0;
  unsigned int minFreeHeap = 0;
  unsigned int minMaxFreeBlockHeap = 0;
  unsigned long restartSignalTime = 0;
  bool heatingEnabled = false;
  unsigned long heatingDisabledTime = 0;
  PumpStartReason extPumpStartReason = PumpStartReason::NONE;
  unsigned long externalPumpStartTime = 0;
  bool telnetStarted = false;
  bool emergencyDetected = false;
  unsigned long emergencyFlipTime = 0;

  #if defined(ARDUINO_ARCH_ESP32)
  const char* getTaskName() override {
    return "Main";
  }

  /*BaseType_t getTaskCore() override {
    return 1;
  }*/

  int getTaskPriority() override {
    return 3;
  }
  #endif

  void setup() {}

  void loop() {
    network->loop();

    if (fsSettings.tick() == FD_WRITE) {
      Log.sinfoln(FPSTR(L_SETTINGS), F("Updated"));
    }

    if (fsNetworkSettings.tick() == FD_WRITE) {
      Log.sinfoln(FPSTR(L_NETWORK_SETTINGS), F("Updated"));
    }

    if (vars.actions.restart) {
      vars.actions.restart = false;
      this->restartSignalTime = millis();

      // save settings
      fsSettings.updateNow();

      // force save network settings
      if (fsNetworkSettings.updateNow() == FD_FILE_ERR && LittleFS.begin()) {
        fsNetworkSettings.write();
      }

      Log.sinfoln(FPSTR(L_MAIN), F("Restart signal received. Restart after 10 sec."));
    }

    vars.states.mqtt = tMqtt->isConnected();
    vars.sensors.rssi = network->isConnected() ? WiFi.RSSI() : 0;

    if (settings.system.logLevel >= TinyLogger::Level::SILENT && settings.system.logLevel <= TinyLogger::Level::VERBOSE) {
      if (Log.getLevel() != settings.system.logLevel) {
        Log.setLevel(static_cast<TinyLogger::Level>(settings.system.logLevel));
      }
    }

    if (network->isConnected()) {
      if (!this->telnetStarted && telnetStream != nullptr) {
        telnetStream->begin(23, false);
        this->telnetStarted = true;
      }

      if (settings.mqtt.enable && !tMqtt->isEnabled()) {
        tMqtt->enable();

      } else if (!settings.mqtt.enable && tMqtt->isEnabled()) {
        tMqtt->disable();
      }

    } else {
      if (this->telnetStarted) {
        telnetStream->stop();
        this->telnetStarted = false;
      }

      if (tMqtt->isEnabled()) {
        tMqtt->disable();
      }
    }
    this->yield();

    this->emergency();
    this->ledStatus();
    this->cascadeControl();
    this->externalPump();
    this->yield();


    // telnet
    if (this->telnetStarted) {
      telnetStream->loop();
      this->yield();
    }


    // anti memory leak
    for (Stream* stream : Log.getStreams()) {
      while (stream->available() > 0) {
        stream->read();
        #ifdef ARDUINO_ARCH_ESP8266
        ::delay(0);
        #endif
      }
    }

    // heap info
    this->heap();


    // restart
    if (this->restartSignalTime > 0 && millis() - this->restartSignalTime > 10000) {
      this->restartSignalTime = 0;
      ESP.restart();
    }
  }

  void heap() {
    unsigned int freeHeap = getFreeHeap();
    unsigned int maxFreeBlockHeap = getMaxFreeBlockHeap();

    if (!this->restartSignalTime && (freeHeap < 2048 || maxFreeBlockHeap < 2048)) {
      this->restartSignalTime = millis();
    }

    if (settings.system.logLevel < TinyLogger::Level::VERBOSE) {
      return;
    }

    size_t minFreeHeap = getFreeHeap(true);
    size_t minFreeHeapDiff = 0;
    if (minFreeHeap < this->minFreeHeap || this->minFreeHeap == 0) {
      minFreeHeapDiff = this->minFreeHeap - minFreeHeap;
      this->minFreeHeap = minFreeHeap;
    }
    
    size_t minMaxFreeBlockHeap = getMaxFreeBlockHeap(true);
    size_t minMaxFreeBlockHeapDiff = 0;
    if (minMaxFreeBlockHeap < this->minMaxFreeBlockHeap || this->minMaxFreeBlockHeap == 0) {
      minMaxFreeBlockHeapDiff = this->minMaxFreeBlockHeap - minMaxFreeBlockHeap;
      this->minMaxFreeBlockHeap = minMaxFreeBlockHeap;
    }

    if (millis() - this->lastHeapInfo > 20000 || minFreeHeapDiff > 0 || minMaxFreeBlockHeapDiff > 0) {
      Log.sverboseln(
        FPSTR(L_MAIN),
        F("Free heap size: %u of %u bytes (min: %u, diff: %u), max free block: %u (min: %u, diff: %u, frag: %hhu%%)"),
        freeHeap, getTotalHeap(), this->minFreeHeap, minFreeHeapDiff, maxFreeBlockHeap, this->minMaxFreeBlockHeap, minMaxFreeBlockHeapDiff, getHeapFrag()
      );
      this->lastHeapInfo = millis();
    }
  }

  void emergency() {
    if (!settings.emergency.enable && vars.states.emergency) {
      this->emergencyDetected = false;
      vars.states.emergency = false;

      Log.sinfoln(FPSTR(L_MAIN), F("Emergency mode disabled"));
    }

    if (!settings.emergency.enable) {
      return;
    }

    // flags
    uint8_t emergencyFlags = 0b00000000;

    // set network flag
    if (settings.emergency.onNetworkFault && !network->isConnected()) {
      emergencyFlags |= 0b00000001;
    }

    // set mqtt flag
    if (settings.emergency.onMqttFault && (!tMqtt->isEnabled() || !tMqtt->isConnected())) {
      emergencyFlags |= 0b00000010;
    }

    // set outdoor sensor flag
    if (settings.sensors.outdoor.type == SensorType::DS18B20 || settings.sensors.outdoor.type == SensorType::BLUETOOTH) {
      if (settings.emergency.onOutdoorSensorDisconnect && !vars.sensors.outdoor.connected) {
        emergencyFlags |= 0b00000100;
      }
    }

    // set indoor sensor flag
    if (settings.sensors.indoor.type == SensorType::DS18B20 || settings.sensors.indoor.type == SensorType::BLUETOOTH) {
      if (settings.emergency.onIndoorSensorDisconnect && !vars.sensors.indoor.connected) {
        emergencyFlags |= 0b00001000;
      }
    }

    // if any flags is true
    if ((emergencyFlags & 0b00001111) != 0) {
      if (!this->emergencyDetected) {
        // flip flag
        this->emergencyDetected = true;
        this->emergencyFlipTime = millis();

      } else if (this->emergencyDetected && !vars.states.emergency) {
        // enable emergency
        if (millis() - this->emergencyFlipTime > (settings.emergency.tresholdTime * 1000)) {
          vars.states.emergency = true;
          Log.sinfoln(FPSTR(L_MAIN), F("Emergency mode enabled (%hhu)"), emergencyFlags);
        }
      }

    } else {
      if (this->emergencyDetected) {
        // flip flag
        this->emergencyDetected = false;
        this->emergencyFlipTime = millis();

      } else if (!this->emergencyDetected && vars.states.emergency) {
        // disable emergency
        if (millis() - this->emergencyFlipTime > 30000) {
          vars.states.emergency = false;
          Log.sinfoln(FPSTR(L_MAIN), F("Emergency mode disabled"));
        }
      }
    }
  }

  void ledStatus() {
    uint8_t errors[4];
    uint8_t errCount = 0;
    static uint8_t errPos = 0;
    static unsigned long endBlinkTime = 0;
    static bool ledOn = false;
    static uint8_t configuredGpio = GPIO_IS_NOT_CONFIGURED;

    if (settings.system.statusLedGpio != configuredGpio) {
      if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
        digitalWrite(configuredGpio, LOW);
      }
      
      if (GPIO_IS_VALID(settings.system.statusLedGpio)) {
        configuredGpio = settings.system.statusLedGpio;
        pinMode(configuredGpio, OUTPUT);
        digitalWrite(configuredGpio, LOW);
        this->blinker->init(configuredGpio);

      } else if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
        configuredGpio = GPIO_IS_NOT_CONFIGURED;
      }
    }

    if (configuredGpio == GPIO_IS_NOT_CONFIGURED) {
      return;
    }

    if (!network->isConnected()) {
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
          digitalWrite(configuredGpio, HIGH);
          ledOn = true;
        }

        return;

      } else if (ledOn) {
        digitalWrite(configuredGpio, LOW);
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

  void cascadeControl() {
    static uint8_t configuredInputGpio = GPIO_IS_NOT_CONFIGURED;
    static uint8_t configuredOutputGpio = GPIO_IS_NOT_CONFIGURED;
    static bool inputTempValue = false;
    static unsigned long inputChangedTs = 0;
    static bool outputTempValue = false;
    static unsigned long outputChangedTs = 0;

    // input
    if (settings.cascadeControl.input.enable) {
      if (settings.cascadeControl.input.gpio != configuredInputGpio) {
        if (configuredInputGpio != GPIO_IS_NOT_CONFIGURED) {
          pinMode(configuredInputGpio, OUTPUT);
          digitalWrite(configuredInputGpio, LOW);

          Log.sinfoln(FPSTR(L_CASCADE_INPUT), F("Deinitialized on GPIO %hhu"), configuredInputGpio);
        }
        
        if (GPIO_IS_VALID(settings.cascadeControl.input.gpio)) {
          configuredInputGpio = settings.cascadeControl.input.gpio;
          pinMode(configuredInputGpio, INPUT);

          Log.sinfoln(FPSTR(L_CASCADE_INPUT), F("Initialized on GPIO %hhu"), configuredInputGpio);

        } else if (configuredInputGpio != GPIO_IS_NOT_CONFIGURED) {
          configuredInputGpio = GPIO_IS_NOT_CONFIGURED;

          Log.swarningln(FPSTR(L_CASCADE_INPUT), F("Failed initialize: GPIO %hhu is not valid!"), configuredInputGpio);
        }
      }

      if (configuredInputGpio != GPIO_IS_NOT_CONFIGURED) {
        bool value;
        if (digitalRead(configuredInputGpio) == HIGH) {
          value = true ^ settings.cascadeControl.input.invertState;
        } else {
          value = false ^ settings.cascadeControl.input.invertState;
        }

        if (value != vars.cascadeControl.input) {
          if (value != inputTempValue) {
            inputTempValue = value;
            inputChangedTs = millis();

          } else if (millis() - inputChangedTs >= settings.cascadeControl.input.thresholdTime * 1000u) {
            vars.cascadeControl.input = value;

            Log.sinfoln(
              FPSTR(L_CASCADE_INPUT),
              F("State changed to %s"),
              value ? F("TRUE") : F("FALSE")
            );
          }

        } else if (value != inputTempValue) {
          inputTempValue = value;
        }
      }
    }
    
    if (!settings.cascadeControl.input.enable || configuredInputGpio == GPIO_IS_NOT_CONFIGURED) {
      if (!vars.cascadeControl.input) {
        vars.cascadeControl.input = true;

        Log.sinfoln(
          FPSTR(L_CASCADE_INPUT),
          F("Disabled, state changed to %s"),
          vars.cascadeControl.input ? F("TRUE") : F("FALSE")
        );
      }
    }


    // output
    if (settings.cascadeControl.output.enable) {
      if (settings.cascadeControl.output.gpio != configuredOutputGpio) {
        if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
          pinMode(configuredOutputGpio, OUTPUT);
          digitalWrite(configuredOutputGpio, LOW);

          Log.sinfoln(FPSTR(L_CASCADE_OUTPUT), F("Deinitialized on GPIO %hhu"), configuredOutputGpio);
        }
        
        if (GPIO_IS_VALID(settings.cascadeControl.output.gpio)) {
          configuredOutputGpio = settings.cascadeControl.output.gpio;
          pinMode(configuredOutputGpio, OUTPUT);
          digitalWrite(
            configuredOutputGpio,
            settings.cascadeControl.output.invertState
              ? HIGH 
              : LOW
          );

          Log.sinfoln(FPSTR(L_CASCADE_OUTPUT), F("Initialized on GPIO %hhu"), configuredOutputGpio);

        } else if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
          configuredOutputGpio = GPIO_IS_NOT_CONFIGURED;

          Log.swarningln(FPSTR(L_CASCADE_OUTPUT), F("Failed initialize: GPIO %hhu is not valid!"), configuredOutputGpio);
        }
      }

      if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
        bool value = false;
        if (settings.cascadeControl.output.onFault && vars.states.fault) {
          value = true;

        } else if (settings.cascadeControl.output.onLossConnection && !vars.states.otStatus) {
          value = true;

        } else if (settings.cascadeControl.output.onEnabledHeating && settings.heating.enable && vars.cascadeControl.input) {
          value = true;
        }

        if (value != vars.cascadeControl.output) {
          if (value != outputTempValue) {
            outputTempValue = value;
            outputChangedTs = millis();
            
          } else if (millis() - outputChangedTs >= settings.cascadeControl.output.thresholdTime * 1000u) {
            vars.cascadeControl.output = value;

            digitalWrite(
              configuredOutputGpio,
              vars.cascadeControl.output ^ settings.cascadeControl.output.invertState
                ? HIGH
                : LOW
            );

            Log.sinfoln(
              FPSTR(L_CASCADE_OUTPUT),
              F("State changed to %s"),
              value ? F("TRUE") : F("FALSE")
            );
          }

        } else if (value != outputTempValue) {
          outputTempValue = value;
        }
      }
    }

    if (!settings.cascadeControl.output.enable || configuredOutputGpio == GPIO_IS_NOT_CONFIGURED) {
      if (vars.cascadeControl.output) {
        vars.cascadeControl.output = false;

        if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
          digitalWrite(
            configuredOutputGpio,
            vars.cascadeControl.output ^ settings.cascadeControl.output.invertState
              ? HIGH
              : LOW
          );
        }

        Log.sinfoln(
          FPSTR(L_CASCADE_OUTPUT),
          F("Disabled, state changed to %s"),
          vars.cascadeControl.output ? F("TRUE") : F("FALSE")
        );
      }
    }
  }

  void externalPump() {
    static uint8_t configuredGpio = GPIO_IS_NOT_CONFIGURED;

    if (settings.externalPump.gpio != configuredGpio) {
      if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
        digitalWrite(configuredGpio, LOW);
      }
      
      if (GPIO_IS_VALID(settings.externalPump.gpio)) {
        configuredGpio = settings.externalPump.gpio;
        pinMode(configuredGpio, OUTPUT);
        digitalWrite(configuredGpio, LOW);

      } else if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
        configuredGpio = GPIO_IS_NOT_CONFIGURED;
      }
    }

    if (configuredGpio == GPIO_IS_NOT_CONFIGURED) {
      if (vars.states.externalPump) {
        vars.states.externalPump = false;
        vars.parameters.extPumpLastEnableTime = millis();

        Log.sinfoln("EXTPUMP", F("Disabled: use = off"));
      }

      return;
    }

    if (!vars.states.heating && this->heatingEnabled) {
      this->heatingEnabled = false;
      this->heatingDisabledTime = millis();

    } else if (vars.states.heating && !this->heatingEnabled) {
      this->heatingEnabled = true;
    }
    
    if (!settings.externalPump.use) {
      if (vars.states.externalPump) {
        digitalWrite(configuredGpio, LOW);

        vars.states.externalPump = false;
        vars.parameters.extPumpLastEnableTime = millis();

        Log.sinfoln("EXTPUMP", F("Disabled: use = off"));
      }

      return;
    }

    if (vars.states.externalPump && !this->heatingEnabled) {
      if (this->extPumpStartReason == MainTask::PumpStartReason::HEATING && millis() - this->heatingDisabledTime > (settings.externalPump.postCirculationTime * 1000u)) {
        digitalWrite(configuredGpio, LOW);

        vars.states.externalPump = false;
        vars.parameters.extPumpLastEnableTime = millis();

        Log.sinfoln("EXTPUMP", F("Disabled: expired post circulation time"));

      } else if (this->extPumpStartReason == MainTask::PumpStartReason::ANTISTUCK && millis() - this->externalPumpStartTime >= (settings.externalPump.antiStuckTime * 1000u)) {
        digitalWrite(configuredGpio, LOW);

        vars.states.externalPump = false;
        vars.parameters.extPumpLastEnableTime = millis();

        Log.sinfoln("EXTPUMP", F("Disabled: expired anti stuck time"));
      }

    } else if (vars.states.externalPump && this->heatingEnabled && this->extPumpStartReason == MainTask::PumpStartReason::ANTISTUCK) {
      this->extPumpStartReason = MainTask::PumpStartReason::HEATING;

    } else if (!vars.states.externalPump && this->heatingEnabled) {
      vars.states.externalPump = true;
      this->externalPumpStartTime = millis();
      this->extPumpStartReason = MainTask::PumpStartReason::HEATING;

      digitalWrite(configuredGpio, HIGH);

      Log.sinfoln("EXTPUMP", F("Enabled: heating on"));

    } else if (!vars.states.externalPump && (vars.parameters.extPumpLastEnableTime == 0 || millis() - vars.parameters.extPumpLastEnableTime >= (settings.externalPump.antiStuckInterval * 1000ul))) {
      vars.states.externalPump = true;
      this->externalPumpStartTime = millis();
      this->extPumpStartReason = MainTask::PumpStartReason::ANTISTUCK;

      digitalWrite(configuredGpio, HIGH);

      Log.sinfoln("EXTPUMP", F("Enabled: anti stuck"));
    }
  }
};