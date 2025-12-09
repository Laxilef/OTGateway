#include <Blinker.h>

using namespace NetworkUtils;

extern NetworkMgr* network;
extern MqttTask* tMqtt;
extern OpenThermTask* tOt;
extern FileData fsNetworkSettings, fsSettings, fsSensorsSettings;
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
  unsigned long miscRunned = 0;
  unsigned long lastHeapInfo = 0;
  unsigned int minFreeHeap = 0;
  unsigned int minMaxFreeBlockHeap = 0;
  bool restartSignalReceived = false;
  unsigned long restartSignalReceivedTime = 0;
  bool heatingEnabled = false;
  unsigned long heatingDisabledTime = 0;
  PumpStartReason extPumpStartReason = PumpStartReason::NONE;
  unsigned long externalPumpStartTime = 0;
  bool ntpStarted = false;
  bool telnetStarted = false;
  bool emergencyDetected = false;
  unsigned long emergencyFlipTime = 0;
  bool freezeDetected = false;
  unsigned long freezeDetectedTime = 0;

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

    if (fsNetworkSettings.tick() == FD_WRITE) {
      Log.sinfoln(FPSTR(L_NETWORK_SETTINGS), F("Updated"));
    }

    if (fsSettings.tick() == FD_WRITE) {
      Log.sinfoln(FPSTR(L_SETTINGS), F("Updated"));
    }

    if (fsSensorsSettings.tick() == FD_WRITE) {
      Log.sinfoln(FPSTR(L_SENSORS_SETTINGS), F("Updated"));
    }

    if (vars.actions.restart) {
      this->restartSignalReceivedTime = millis();
      this->restartSignalReceived = true;
      vars.actions.restart = false;

      Log.sinfoln(FPSTR(L_MAIN), F("Received restart signal"));
    }

    if (!vars.states.restarting && this->restartSignalReceived && millis() - this->restartSignalReceivedTime > 5000) {
      vars.states.restarting = true;

      // save settings
      fsSettings.updateNow();

      // save sensors settings
      fsSensorsSettings.updateNow();

      // force save network settings
      if (fsNetworkSettings.updateNow() == FD_FILE_ERR && LittleFS.begin()) {
        fsNetworkSettings.write();
      }

      Log.sinfoln(FPSTR(L_MAIN), F("Restart scheduled in 10 sec."));
    }

    vars.mqtt.connected = tMqtt->isConnected();
    vars.network.connected = network->isConnected();
    vars.network.rssi = network->isConnected() ? WiFi.RSSI() : 0;

    if (settings.system.logLevel >= TinyLogger::Level::SILENT && settings.system.logLevel <= TinyLogger::Level::VERBOSE) {
      if (Log.getLevel() != settings.system.logLevel) {
        Log.setLevel(static_cast<TinyLogger::Level>(settings.system.logLevel));
      }
    }

    if (network->isConnected()) {
      if (!this->ntpStarted) {
        if (strlen(settings.system.ntp.server)) {
          configTime(0, 0, settings.system.ntp.server);
          setenv("TZ", settings.system.ntp.timezone, 1);
          tzset();

          this->ntpStarted = true;
        }
      }

      if (!this->telnetStarted && telnetStream != nullptr) {
        telnetStream->begin(23, false);
        this->telnetStarted = true;
      }

      if (settings.mqtt.enabled && !tMqtt->isEnabled()) {
        tMqtt->enable();

      } else if (!settings.mqtt.enabled && tMqtt->isEnabled()) {
        tMqtt->disable();
      }

      Sensors::setConnectionStatusByType(Sensors::Type::MANUAL, !settings.mqtt.enabled || vars.mqtt.connected, false);

    } else {
      if (this->ntpStarted) {
        this->ntpStarted = false;
      }

      if (this->telnetStarted) {
        telnetStream->stop();
        this->telnetStarted = false;
      }

      if (tMqtt->isEnabled()) {
        tMqtt->disable();
      }

      Sensors::setConnectionStatusByType(Sensors::Type::MANUAL, false, false);
    }

    this->yield();
    if (this->misc()) {
      this->yield();
    }
    this->ledStatus();

    // telnet
    if (this->telnetStarted) {
      this->yield();
      telnetStream->loop();
      this->yield();
    }


    // anti memory leak
    for (Stream* stream : Log.getStreams()) {
      while (stream->available() > 0) {
        stream->read();

        #ifdef ARDUINO_ARCH_ESP8266
        ::optimistic_yield(1000);
        #endif
      }
    }

    // heap info
    this->heap();
  }

  bool misc() {
    if (millis() - this->miscRunned < 1000) {
      return false;
    }

    // restart if required
    if (this->restartSignalReceived && millis() - this->restartSignalReceivedTime > 15000) {
      this->restartSignalReceived = false;

      ESP.restart();
    }

    this->heating();
    this->emergency();
    this->cascadeControl();
    this->externalPump();
    this->miscRunned = millis();
    
    return true;
  }

  void heap() {
    unsigned int freeHeap = getFreeHeap();
    unsigned int maxFreeBlockHeap = getMaxFreeBlockHeap();

    // critical heap
    if (!vars.states.restarting && (freeHeap < 2048 || maxFreeBlockHeap < 2048)) {
      this->restartSignalReceivedTime = millis();
      this->restartSignalReceived = true;
      vars.states.restarting = true;
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

  void heating() {
    // freeze protection
    if (!settings.heating.enabled) {
      float lowTemp = 255.0f;
      uint8_t availableSensors = 0;

      if (Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::INDOOR_TEMP)) {
        auto value = Sensors::getMeanValueByPurpose(Sensors::Purpose::INDOOR_TEMP, Sensors::ValueType::PRIMARY);
        if (value < lowTemp) {
          lowTemp = value;
        }
        
        availableSensors++;
      }

      if (Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::HEATING_TEMP)) {
        auto value = Sensors::getMeanValueByPurpose(Sensors::Purpose::HEATING_TEMP, Sensors::ValueType::PRIMARY);
        if (value < lowTemp) {
          lowTemp = value;
        }
        
        availableSensors++;
      }

      if (Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::HEATING_RETURN_TEMP)) {
        auto value = Sensors::getMeanValueByPurpose(Sensors::Purpose::HEATING_RETURN_TEMP, Sensors::ValueType::PRIMARY);
        if (value < lowTemp) {
          lowTemp = value;
        }

        availableSensors++;
      }

      if (availableSensors && lowTemp <= settings.heating.freezeProtection.lowTemp) {
        if (!this->freezeDetected) {
          this->freezeDetected = true;
          this->freezeDetectedTime = millis();

        } else if (millis() - this->freezeDetectedTime > (settings.heating.freezeProtection.thresholdTime * 1000)) {
          this->freezeDetected = false;
          settings.heating.enabled = true;
          fsSettings.update();

          Log.sinfoln(
            FPSTR(L_MAIN),
            F("Heating turned on by freeze protection, current low temp: %.2f, threshold: %hhu"),
            lowTemp, settings.heating.freezeProtection.lowTemp
          );
        }

      } else if (this->freezeDetected) {
        this->freezeDetected = false;
      }

    } else if (this->freezeDetected) {
      this->freezeDetected = false;
    }
  }

  void emergency() {
    // flags
    uint8_t emergencyFlags = 0b00000000;

    // set outdoor sensor flag
    if (settings.equitherm.enabled) {
      if (!Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::OUTDOOR_TEMP)) {
        emergencyFlags |= 0b00000001;
      }
    }

    // set indoor sensor flags
    if (!Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::INDOOR_TEMP)) {
      if (!settings.equitherm.enabled && settings.pid.enabled) {
        emergencyFlags |= 0b00000010;
      }
      
      if (settings.opentherm.options.nativeHeatingControl) {
        emergencyFlags |= 0b00000100;
      }
    }

    // if any flags is true
    if ((emergencyFlags & 0b00001111) != 0) {
      if (!this->emergencyDetected) {
        // flip flag
        this->emergencyDetected = true;
        this->emergencyFlipTime = millis();

      } else if (this->emergencyDetected && !vars.emergency.state) {
        // enable emergency
        if (millis() - this->emergencyFlipTime > (settings.emergency.tresholdTime * 1000)) {
          vars.emergency.state = true;
          Log.sinfoln(FPSTR(L_MAIN), F("Emergency mode enabled (%hhu)"), emergencyFlags);
        }
      }

    } else {
      if (this->emergencyDetected) {
        // flip flag
        this->emergencyDetected = false;
        this->emergencyFlipTime = millis();

      } else if (!this->emergencyDetected && vars.emergency.state) {
        // disable emergency
        if (millis() - this->emergencyFlipTime > (settings.emergency.tresholdTime * 1000)) {
          vars.emergency.state = false;
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

    if (!vars.slave.connected) {
      errors[errCount++] = 3;
    }

    if (vars.slave.fault.active) {
      errors[errCount++] = 4;
    }

    if (vars.emergency.state) {
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
    if (settings.cascadeControl.input.enabled) {
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
    
    if (!settings.cascadeControl.input.enabled || configuredInputGpio == GPIO_IS_NOT_CONFIGURED) {
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
    if (settings.cascadeControl.output.enabled) {
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
        if (settings.cascadeControl.output.onFault && vars.slave.fault.active) {
          value = true;

        } else if (settings.cascadeControl.output.onLossConnection && !vars.slave.connected) {
          value = true;

        } else if (settings.cascadeControl.output.onEnabledHeating && settings.heating.enabled && vars.cascadeControl.input) {
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

    if (!settings.cascadeControl.output.enabled || configuredOutputGpio == GPIO_IS_NOT_CONFIGURED) {
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
        digitalWrite(
          configuredGpio,
          settings.externalPump.invertState
            ? HIGH 
            : LOW
        );

      } else if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
        configuredGpio = GPIO_IS_NOT_CONFIGURED;
      }
    }

    if (configuredGpio == GPIO_IS_NOT_CONFIGURED) {
      if (vars.externalPump.state) {
        vars.externalPump.state = false;
        vars.externalPump.lastEnabledTime = millis();

        Log.sinfoln(FPSTR(L_EXTPUMP), F("Disabled: use = off"));
      }

      return;
    }

    if (!vars.master.heating.enabled && this->heatingEnabled) {
      this->heatingEnabled = false;
      this->heatingDisabledTime = millis();

    } else if (vars.master.heating.enabled && !this->heatingEnabled) {
      this->heatingEnabled = true;
    }
    
    if (!settings.externalPump.use) {
      if (vars.externalPump.state) {
        digitalWrite(
          configuredGpio,
          settings.externalPump.invertState
            ? HIGH 
            : LOW
        );

        vars.externalPump.state = false;
        vars.externalPump.lastEnabledTime = millis();

        Log.sinfoln(FPSTR(L_EXTPUMP), F("Disabled: use = off"));
      }

      return;
    }

    if (vars.externalPump.state && !this->heatingEnabled) {
      if (this->extPumpStartReason == MainTask::PumpStartReason::HEATING && millis() - this->heatingDisabledTime > (settings.externalPump.postCirculationTime * 1000u)) {
        digitalWrite(
          configuredGpio,
          settings.externalPump.invertState
            ? HIGH 
            : LOW
        );

        vars.externalPump.state = false;
        vars.externalPump.lastEnabledTime = millis();

        Log.sinfoln(FPSTR(L_EXTPUMP), F("Disabled: expired post circulation time"));

      } else if (this->extPumpStartReason == MainTask::PumpStartReason::ANTISTUCK && millis() - this->externalPumpStartTime >= (settings.externalPump.antiStuckTime * 1000u)) {
        digitalWrite(
          configuredGpio,
          settings.externalPump.invertState
            ? HIGH 
            : LOW
        );

        vars.externalPump.state = false;
        vars.externalPump.lastEnabledTime = millis();

        Log.sinfoln(FPSTR(L_EXTPUMP), F("Disabled: expired anti stuck time"));
      }

    } else if (vars.externalPump.state && this->heatingEnabled && this->extPumpStartReason == MainTask::PumpStartReason::ANTISTUCK) {
      this->extPumpStartReason = MainTask::PumpStartReason::HEATING;

    } else if (!vars.externalPump.state && this->heatingEnabled) {
      vars.externalPump.state = true;
      this->externalPumpStartTime = millis();
      this->extPumpStartReason = MainTask::PumpStartReason::HEATING;

      digitalWrite(
        configuredGpio,
        settings.externalPump.invertState
          ? LOW 
          : HIGH
      );

      Log.sinfoln(FPSTR(L_EXTPUMP), F("Enabled: heating on"));

    } else if (!vars.externalPump.state && (vars.externalPump.lastEnabledTime == 0 || millis() - vars.externalPump.lastEnabledTime >= (settings.externalPump.antiStuckInterval * 1000lu))) {
      vars.externalPump.state = true;
      this->externalPumpStartTime = millis();
      this->extPumpStartReason = MainTask::PumpStartReason::ANTISTUCK;

      digitalWrite(
        configuredGpio,
        settings.externalPump.invertState
          ? LOW 
          : HIGH
      );

      Log.sinfoln(FPSTR(L_EXTPUMP), F("Enabled: anti stuck"));
    }
  }
};