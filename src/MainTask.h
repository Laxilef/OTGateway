#include <Blinker.h>

extern Network::Manager* network;
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
  unsigned long firstFailConnect = 0;
  unsigned long lastHeapInfo = 0;
  unsigned int minFreeHeap = 0;
  unsigned int minMaxFreeBlockHeap = 0;
  unsigned long restartSignalTime = 0;
  bool heatingEnabled = false;
  unsigned long heatingDisabledTime = 0;
  PumpStartReason extPumpStartReason = PumpStartReason::NONE;
  unsigned long externalPumpStartTime = 0;
  unsigned long relaysStartTime[4] = {0, 0, 0, 0};
  bool telnetStarted = false;

  const char* getTaskName() {
    return "Main";
  }

  /*int getTaskCore() {
    return 1;
  }*/

  int getTaskPriority() {
    return 3;
  }

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

    if (vars.states.emergency && !settings.emergency.enable) {
      vars.states.emergency = false;
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

      if (!vars.states.emergency && settings.emergency.enable && settings.emergency.onMqttFault && !tMqtt->isEnabled()) {
        vars.states.emergency = true;

      } else if (vars.states.emergency && !settings.emergency.onMqttFault) {
        vars.states.emergency = false;
      }

      if (this->firstFailConnect != 0) {
        this->firstFailConnect = 0;
      }

      if ( Log.getLevel() != TinyLogger::Level::INFO && !settings.system.debug ) {
        Log.setLevel(TinyLogger::Level::INFO);

      } else if ( Log.getLevel() != TinyLogger::Level::VERBOSE && settings.system.debug ) {
        Log.setLevel(TinyLogger::Level::VERBOSE);
      }

    } else {
      if (this->telnetStarted) {
        telnetStream->stop();
        this->telnetStarted = false;
      }

      if (tMqtt->isEnabled()) {
        tMqtt->disable();
      }

      if (!vars.states.emergency && settings.emergency.enable && settings.emergency.onNetworkFault) {
        if (this->firstFailConnect == 0) {
          this->firstFailConnect = millis();
        }

        if (millis() - this->firstFailConnect > (settings.emergency.tresholdTime * 1000)) {
          vars.states.emergency = true;
          Log.sinfoln(FPSTR(L_MAIN), F("Emergency mode enabled"));
        }
      }
    }
    this->yield();


    this->ledStatus();
    this->externalPump();
    this->relays();
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

    if (!settings.system.debug) {
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

  void relays() {

    static unsigned int holdTime = 100;
    
    uint8_t gpio = settings.relay1.gpio;
    if (GPIO_IS_VALID(gpio)) {
      pinMode(gpio, OUTPUT);
      if (vars.parameters.relay1LastTurnTime == 0) {
        digitalWrite(gpio, settings.relay1.normal == true ? HIGH : LOW);
        Log.sinfoln("RELAY1", F("set normal (%s)"), settings.relay1.normal == true ? "ON" : "OFF");
        vars.relays.relay1 = settings.relay1.normal;
        vars.parameters.relay1LastTurnTime = millis();
      } else if (vars.relays.relay1 != (digitalRead(gpio) == HIGH ? true : false)) {
        digitalWrite(gpio, vars.relays.relay1 == true ? HIGH : LOW);
        Log.sinfoln("RELAY1", F("turn (%s)"), vars.relays.relay1 == true ? "ON" : "OFF");
        vars.parameters.relay1LastTurnTime = millis();
      }
      if ((settings.relay1.selfLocking == false) && (vars.relays.relay1 != settings.relay1.normal)) {
        if (millis() - vars.parameters.relay1LastTurnTime > holdTime) {
          vars.parameters.relay1LastTurnTime = 0;
        }
      }
    }

    if ((settings.relay1.selfLocking == false) && (vars.relays.relay1 != settings.relay1.normal)) {
      return;
    }

    gpio = settings.relay2.gpio;
    if (GPIO_IS_VALID(gpio)) {
      pinMode(gpio, OUTPUT);
      if (vars.parameters.relay2LastTurnTime == 0) {
        digitalWrite(gpio, settings.relay2.normal == true ? HIGH : LOW);
        Log.sinfoln("RELAY2", F("set normal (%s)"), settings.relay2.normal == true ? "ON" : "OFF");
        vars.relays.relay2 = settings.relay2.normal;
        vars.parameters.relay2LastTurnTime = millis();
      } else if (vars.relays.relay2 != (digitalRead(gpio) == HIGH ? true : false)) {
        digitalWrite(gpio, vars.relays.relay2 == true ? HIGH : LOW);
        Log.sinfoln("RELAY2", F("turn (%s)"), vars.relays.relay2 == true ? "ON" : "OFF");
        vars.parameters.relay2LastTurnTime = millis();
      }
      if ((settings.relay2.selfLocking == false) && (vars.relays.relay2 != settings.relay2.normal)) {
        if (millis() - vars.parameters.relay2LastTurnTime > holdTime) {
          vars.parameters.relay2LastTurnTime = 0;
        }
      }
    }

    if ((settings.relay2.selfLocking == false) && (vars.relays.relay2 != settings.relay2.normal)) {
      return;
    }

    gpio = settings.relay3.gpio;
    if (GPIO_IS_VALID(gpio)) {
      pinMode(gpio, OUTPUT);
      if (vars.parameters.relay3LastTurnTime == 0) {
        digitalWrite(gpio, settings.relay3.normal == true ? HIGH : LOW);
        Log.sinfoln("RELAY3", F("set normal (%s)"), settings.relay3.normal == true ? "ON" : "OFF");
        vars.relays.relay3 = settings.relay3.normal;
        vars.parameters.relay3LastTurnTime = millis();
      } else if (vars.relays.relay3 != (digitalRead(gpio) == HIGH ? true : false)) {
        digitalWrite(gpio, vars.relays.relay3 == true ? HIGH : LOW);
        Log.sinfoln("RELAY3", F("turn (%s)"), vars.relays.relay3 == true ? "ON" : "OFF");
        vars.parameters.relay3LastTurnTime = millis();
      }
      if ((settings.relay3.selfLocking == false) && (vars.relays.relay3 != settings.relay3.normal)) {
        if (millis() - vars.parameters.relay3LastTurnTime > holdTime) {
          vars.parameters.relay3LastTurnTime = 0;
        }
      }
    }

    if ((settings.relay3.selfLocking == false) && (vars.relays.relay3 != settings.relay3.normal)) {
      return;
    }

    gpio = settings.relay4.gpio;
    if (GPIO_IS_VALID(gpio)) {
      pinMode(gpio, OUTPUT);
      if (vars.parameters.relay4LastTurnTime == 0) {
        digitalWrite(gpio, settings.relay4.normal == true ? HIGH : LOW);
        Log.sinfoln("RELAY4", F("set normal (%s)"), settings.relay4.normal == true ? "ON" : "OFF");
        vars.relays.relay4 = settings.relay4.normal;
        vars.parameters.relay4LastTurnTime = millis();
      } else if (vars.relays.relay4 != (digitalRead(gpio) == HIGH ? true : false)) {
        digitalWrite(gpio, vars.relays.relay4 == true ? HIGH : LOW);
        Log.sinfoln("RELAY4", F("turn (%s)"), vars.relays.relay4 == true ? "ON" : "OFF");
        vars.parameters.relay4LastTurnTime = millis();
      }
      if ((settings.relay4.selfLocking == false) && (vars.relays.relay4 != settings.relay4.normal)) {
        if (millis() - vars.parameters.relay4LastTurnTime > holdTime) {
          vars.parameters.relay4LastTurnTime = 0;
        }
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