#include <CustomOpenTherm.h>
extern FileData fsSettings;

class OpenThermTask : public Task {
public:
  OpenThermTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

  ~OpenThermTask() {
    delete this->instance;
  }

protected:
  const unsigned short readyTime = 60000;
  const unsigned short dhwSetTempInterval = 60000;
  const unsigned short heatingSetTempInterval = 60000;
  const unsigned int initializingInterval = 3600000;

  CustomOpenTherm* instance = nullptr;
  unsigned long instanceCreatedTime = 0;
  byte instanceInGpio = 0;
  byte instanceOutGpio = 0;
  bool isInitialized = false;
  unsigned long initializedTime = 0;
  unsigned int initializedMemberIdCode = 0;
  bool pump = true;
  unsigned long lastSuccessResponse = 0;
  unsigned long prevUpdateNonEssentialVars = 0;
  unsigned long dhwSetTempTime = 0;
  unsigned long heatingSetTempTime = 0;
  byte configuredRxLedGpio = GPIO_IS_NOT_CONFIGURED;

  #if defined(ARDUINO_ARCH_ESP32)
  const char* getTaskName() override {
    return "OpenTherm";
  }
  
  BaseType_t getTaskCore() override {
    return 1;
  }

  int getTaskPriority() override {
    return 5;
  }
  #endif

  void setup() {
    if (settings.system.unitSystem != UnitSystem::METRIC) {
      vars.parameters.heatingMinTemp = convertTemp(vars.parameters.heatingMinTemp, UnitSystem::METRIC, settings.system.unitSystem);
      vars.parameters.heatingMaxTemp = convertTemp(vars.parameters.heatingMaxTemp, UnitSystem::METRIC, settings.system.unitSystem);
      vars.parameters.dhwMinTemp = convertTemp(vars.parameters.dhwMinTemp, UnitSystem::METRIC, settings.system.unitSystem);
      vars.parameters.dhwMaxTemp = convertTemp(vars.parameters.dhwMaxTemp, UnitSystem::METRIC, settings.system.unitSystem);
    }

    // delete instance
    if (this->instance != nullptr) {
      delete this->instance;
      this->instance = nullptr;
      Log.sinfoln(FPSTR(L_OT), F("Stopped"));
    }

    if (!GPIO_IS_VALID(settings.opentherm.inGpio) || !GPIO_IS_VALID(settings.opentherm.outGpio)) {
      Log.swarningln(
        FPSTR(L_OT),
        F("Not started. GPIO IN: %hhu or GPIO OUT: %hhu is not valid"),
        settings.opentherm.inGpio,
        settings.opentherm.outGpio
      );
      return;
    }

    // create instance
    this->instance = new CustomOpenTherm(settings.opentherm.inGpio, settings.opentherm.outGpio);

    // flags
    this->instanceCreatedTime = millis();
    this->instanceInGpio = settings.opentherm.inGpio;
    this->instanceOutGpio = settings.opentherm.outGpio;
    this->isInitialized = false;

    Log.sinfoln(FPSTR(L_OT), F("Started. GPIO IN: %hhu, GPIO OUT: %hhu"), settings.opentherm.inGpio, settings.opentherm.outGpio);

    this->instance->setAfterSendRequestCallback([this](unsigned long request, unsigned long response, OpenThermResponseStatus status, byte attempt) {
      Log.straceln(
        FPSTR(L_OT),
        F("ID: %4d   Request: %8lx   Response: %8lx   Attempt: %2d   Status: %s"),
        CustomOpenTherm::getDataID(request), request, response, attempt, CustomOpenTherm::statusToString(status)
      );

      if (status == OpenThermResponseStatus::SUCCESS) {
        this->lastSuccessResponse = millis();

        if (this->configuredRxLedGpio != GPIO_IS_NOT_CONFIGURED) {
          digitalWrite(this->configuredRxLedGpio, HIGH);
          delayMicroseconds(2000);
          digitalWrite(this->configuredRxLedGpio, LOW);
        }
      }
    });

    this->instance->setYieldCallback([this]() {
      this->delay(25);
    });

    this->instance->begin();
  }

  void loop() {
    static float currentHeatingTemp = 0.0f;
    static float currentDhwTemp = 0.0f;

    if (this->instanceInGpio != settings.opentherm.inGpio || this->instanceOutGpio != settings.opentherm.outGpio) {
      this->setup();

    } else if (this->initializedMemberIdCode != settings.opentherm.memberIdCode || millis() - this->initializedTime > this->initializingInterval) {
      this->isInitialized = false; 
    }

    if (this->instance == nullptr) {
      this->delay(5000);
      return;
    }

    // RX LED GPIO setup
    if (settings.opentherm.rxLedGpio != this->configuredRxLedGpio) {
      if (this->configuredRxLedGpio != GPIO_IS_NOT_CONFIGURED) {
        digitalWrite(this->configuredRxLedGpio, LOW);
      }
      
      if (GPIO_IS_VALID(settings.opentherm.rxLedGpio)) {
        this->configuredRxLedGpio = settings.opentherm.rxLedGpio;
        pinMode(this->configuredRxLedGpio, OUTPUT);
        digitalWrite(this->configuredRxLedGpio, LOW);

      } else if (this->configuredRxLedGpio != GPIO_IS_NOT_CONFIGURED) {
        this->configuredRxLedGpio = GPIO_IS_NOT_CONFIGURED;
      }
    }

    bool heatingEnabled = (vars.states.emergency || settings.heating.enable) && this->pump && vars.cascadeControl.input && this->isReady();
    bool heatingCh2Enabled = settings.opentherm.heatingCh2Enabled;
    if (settings.opentherm.heatingCh1ToCh2) {
      heatingCh2Enabled = heatingEnabled;

    } else if (settings.opentherm.dhwToCh2) {
      heatingCh2Enabled = settings.opentherm.dhwPresent && settings.dhw.enable;
    }

    // Set boiler status LB
    // Some boilers require this, although this is against protocol
    uint8_t statusLb = 0;

    // Immergas fix
    // https://arduino.ru/forum/programmirovanie/termostat-opentherm-na-esp8266?page=15#comment-649392
    if (settings.opentherm.immergasFix) {
      statusLb = 0xCA;
    }

    unsigned long response = this->instance->setBoilerStatus(
      heatingEnabled,
      settings.opentherm.dhwPresent && settings.dhw.enable,
      false,
      settings.opentherm.nativeHeatingControl,
      heatingCh2Enabled,
      settings.opentherm.summerWinterMode,
      settings.opentherm.dhwBlocking,
      statusLb
    );

    if (!CustomOpenTherm::isValidResponse(response)) {
      Log.swarningln(
        FPSTR(L_OT),
        F("Failed receive boiler status: %s"),
        CustomOpenTherm::statusToString(this->instance->getLastResponseStatus())
      );
    }

    if (!vars.states.otStatus && millis() - this->lastSuccessResponse < 1150) {
      Log.sinfoln(FPSTR(L_OT), F("Connected"));
      
      vars.states.otStatus = true;
      
    } else if (vars.states.otStatus && millis() - this->lastSuccessResponse > 1150) {
      Log.swarningln(FPSTR(L_OT), F("Disconnected"));

      vars.states.otStatus = false;
      this->isInitialized = false;
    }

    // If boiler is disconnected, no need try setting other OT stuff
    if (!vars.states.otStatus) {
      vars.states.heating = false;
      vars.states.dhw = false;
      vars.states.flame = false;
      vars.states.fault = false;
      vars.states.diagnostic = false;

      return;
    }

    if (!this->isInitialized) {
      Log.sinfoln(FPSTR(L_OT), F("Initializing..."));
      this->isInitialized = true;
      this->initializedTime = millis();
      this->initializedMemberIdCode = settings.opentherm.memberIdCode;
      this->initialize();
    }

    if (vars.parameters.heatingEnabled != heatingEnabled) {
      this->prevUpdateNonEssentialVars = 0;
      vars.parameters.heatingEnabled = heatingEnabled;
      Log.sinfoln(FPSTR(L_OT_HEATING), "%s", heatingEnabled ? F("Enabled") : F("Disabled"));
    }

    vars.states.heating = CustomOpenTherm::isCentralHeatingActive(response);
    vars.states.dhw = settings.opentherm.dhwPresent ? CustomOpenTherm::isHotWaterActive(response) : false;
    vars.states.flame = CustomOpenTherm::isFlameOn(response);
    vars.states.fault = CustomOpenTherm::isFault(response);
    vars.states.diagnostic = CustomOpenTherm::isDiagnostic(response);

    Log.snoticeln(
      FPSTR(L_OT),
      F("Received boiler status. Heating: %hhu; DHW: %hhu; flame: %hhu; fault: %hhu; diag: %hhu"),
      vars.states.heating, vars.states.dhw, vars.states.flame, vars.states.fault, vars.states.diagnostic
    );

    // These parameters will be updated every minute
    if (millis() - this->prevUpdateNonEssentialVars > 60000) {
      if (this->updateMinModulationLevel()) {
        Log.snoticeln(
          FPSTR(L_OT),
          F("Received min modulation: %hhu%%, max power: %hhu kW"),
          vars.parameters.minModulation,
          vars.parameters.maxPower
        );
        
        if (settings.opentherm.maxModulation < vars.parameters.minModulation) {
          settings.opentherm.maxModulation = vars.parameters.minModulation;
          fsSettings.update();
          Log.swarningln(FPSTR(L_SETTINGS_OT), F("Updated min modulation: %hhu%%"), settings.opentherm.maxModulation);
        }

        if (fabsf(settings.opentherm.maxPower) < 0.1f && vars.parameters.maxPower > 0) {
          settings.opentherm.maxPower = vars.parameters.maxPower;
          fsSettings.update();
          Log.swarningln(FPSTR(L_SETTINGS_OT), F("Updated max power: %.2f kW"), settings.opentherm.maxPower);
        }

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive min modulation and max power"));
      }

      if (!heatingEnabled && settings.opentherm.modulationSyncWithHeating) {
        if (this->setMaxModulationLevel(0)) {
          Log.snoticeln(FPSTR(L_OT), F("Set max modulation: 0% (off)"));

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed set max modulation: 0% (off)"));
        }

      } else {
        if (this->setMaxModulationLevel(settings.opentherm.maxModulation)) {
          Log.snoticeln(FPSTR(L_OT), F("Set max modulation: %hhu%%"), settings.opentherm.maxModulation);

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed set max modulation: %hhu%%"), settings.opentherm.maxModulation);
        }
      }


      // Get DHW min/max temp (if necessary)
      if (settings.opentherm.dhwPresent && settings.opentherm.getMinMaxTemp) {
        if (this->updateMinMaxDhwTemp()) {
          Log.snoticeln(
            FPSTR(L_OT_DHW),
            F("Received min temp: %hhu, max temp: %hhu"),
            vars.parameters.dhwMinTemp,
            vars.parameters.dhwMaxTemp
          );

          if (settings.dhw.minTemp < vars.parameters.dhwMinTemp) {
            settings.dhw.minTemp = vars.parameters.dhwMinTemp;
            fsSettings.update();
            Log.swarningln(FPSTR(L_SETTINGS_DHW), F("Updated min temp: %hhu"), settings.dhw.minTemp);
          }

          if (settings.dhw.maxTemp > vars.parameters.dhwMaxTemp) {
            settings.dhw.maxTemp = vars.parameters.dhwMaxTemp;
            fsSettings.update();
            Log.swarningln(FPSTR(L_SETTINGS_DHW), F("Updated max temp: %hhu"), settings.dhw.maxTemp);
          }

        } else {
          vars.parameters.dhwMinTemp = convertTemp(DEFAULT_DHW_MIN_TEMP, UnitSystem::METRIC, settings.system.unitSystem);
          vars.parameters.dhwMaxTemp = convertTemp(DEFAULT_DHW_MAX_TEMP, UnitSystem::METRIC, settings.system.unitSystem);

          Log.swarningln(FPSTR(L_OT_DHW), F("Failed receive min/max temp"));
        }

        if (settings.dhw.minTemp >= settings.dhw.maxTemp) {
          settings.dhw.minTemp = vars.parameters.dhwMinTemp;
          settings.dhw.maxTemp = vars.parameters.dhwMaxTemp;
          fsSettings.update();
        }
      }


      // Get heating min/max temp
      if (settings.opentherm.getMinMaxTemp) {
        if (this->updateMinMaxHeatingTemp()) {
          Log.snoticeln(
            FPSTR(L_OT_HEATING),
            F("Received min temp: %hhu, max temp: %hhu"),
            vars.parameters.heatingMinTemp,
            vars.parameters.heatingMaxTemp
          );

          if (settings.heating.minTemp < vars.parameters.heatingMinTemp) {
            settings.heating.minTemp = vars.parameters.heatingMinTemp;
            fsSettings.update();
            Log.swarningln(FPSTR(L_SETTINGS_HEATING), F("Updated min temp: %hhu"), settings.heating.minTemp);
          }

          if (settings.heating.maxTemp > vars.parameters.heatingMaxTemp) {
            settings.heating.maxTemp = vars.parameters.heatingMaxTemp;
            fsSettings.update();
            Log.swarningln(FPSTR(L_SETTINGS_HEATING), F("Updated max temp: %hhu"), settings.heating.maxTemp);
          }
          
        } else {
          vars.parameters.heatingMinTemp = convertTemp(DEFAULT_HEATING_MIN_TEMP, UnitSystem::METRIC, settings.system.unitSystem);
          vars.parameters.heatingMaxTemp = convertTemp(DEFAULT_HEATING_MAX_TEMP, UnitSystem::METRIC, settings.system.unitSystem);

          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed receive min/max temp"));
        }
      }

      if (settings.heating.minTemp >= settings.heating.maxTemp) {
        settings.heating.minTemp = vars.parameters.heatingMinTemp;
        settings.heating.maxTemp = vars.parameters.heatingMaxTemp;
        fsSettings.update();
      }

      // Get fault code (if necessary)
      if (vars.states.fault) {
        if (this->updateFaultCode()) {
          Log.snoticeln(
            FPSTR(L_OT),
            F("Received fault code: %hhu (0x%02X)"),
            vars.sensors.faultCode,
            vars.sensors.faultCode
          );

        } else {
          vars.sensors.faultCode = 0;

          Log.swarningln(FPSTR(L_OT), F("Failed receive fault code"));
        }
        
      } else if (vars.sensors.faultCode != 0) {
        vars.sensors.faultCode = 0;
      }

      // Get diagnostic code (if necessary)
      if (vars.states.fault || vars.states.diagnostic) {
        if (this->updateDiagCode()) {
          Log.snoticeln(
            FPSTR(L_OT),
            F("Received diag code: %hu (0x%02X)"),
            vars.sensors.diagnosticCode,
            vars.sensors.diagnosticCode
          );

        } else {
          vars.sensors.diagnosticCode = 0;

          Log.swarningln(FPSTR(L_OT), F("Failed receive diag code"));
        }
        
      } else if (vars.sensors.diagnosticCode != 0) {
        vars.sensors.diagnosticCode = 0;
      }

      // If filtering is disabled, then it is enough to
      // update these parameters once a minute
      if (!settings.opentherm.filterNumValues.enable) {
        // Get outdoor temp (if necessary)
        if (settings.sensors.outdoor.type == SensorType::BOILER) {
          if (this->updateOutdoorTemp()) {
            Log.snoticeln(FPSTR(L_OT), F("Received outdoor temp: %.2f"), vars.temperatures.outdoor);

          } else {
            Log.swarningln(FPSTR(L_OT), F("Failed receive outdoor temp"));
          }
        }
        
        // Get pressure
        if (this->updatePressure()) {
          Log.snoticeln(FPSTR(L_OT), F("Received pressure: %.2f"), vars.sensors.pressure);

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive pressure"));
        }
      }

      this->prevUpdateNonEssentialVars = millis();
    }


    // Get current modulation level (if necessary)
    if (vars.states.flame) {
      if (this->updateModulationLevel()) {
        if (settings.opentherm.maxPower > 0.1f) {
          float modulatedPower = settings.opentherm.maxPower - settings.opentherm.minPower;
          vars.sensors.power = settings.opentherm.minPower + (modulatedPower / 100.0f * vars.sensors.modulation);

        } else {
          vars.sensors.power = 0.0f;
        }
        
        Log.snoticeln(
          FPSTR(L_OT),
          F("Received modulation level: %.2f%%, power: %.2f of %.2f kW (min: %.2f kW)"),
          vars.sensors.modulation,
          vars.sensors.power,
          settings.opentherm.maxPower,
          settings.opentherm.minPower
        );

      } else {
        vars.sensors.modulation = 0;
        vars.sensors.power = 0;

        Log.swarningln(FPSTR(L_OT), F("Failed receive modulation level"));
      }

    } else {
      vars.sensors.modulation = 0;
      vars.sensors.power = 0;
    }

    // Update DHW sensors (if necessary)
    if (settings.opentherm.dhwPresent) {
      if (this->updateDhwTemp()) {
        Log.snoticeln(FPSTR(L_OT_DHW), F("Received temp: %.2f"), vars.temperatures.dhw);

      } else {
        Log.swarningln(FPSTR(L_OT_DHW), F("Failed receive temp"));
      }

      if (this->updateDhwFlowRate()) {
        Log.snoticeln(FPSTR(L_OT_DHW), F("Received flow rate: %.2f"), vars.sensors.dhwFlowRate);

      } else {
        Log.swarningln(FPSTR(L_OT_DHW), F("Failed receive flow rate"));
      }

    } else {
      vars.temperatures.dhw = 0.0f;
      vars.sensors.dhwFlowRate = 0.0f;
    }

    // Get current heating temp
    if (this->updateHeatingTemp()) {
      Log.snoticeln(FPSTR(L_OT_HEATING), F("Received temp: %.2f"), vars.temperatures.heating);

    } else {
      Log.swarningln(FPSTR(L_OT_HEATING), F("Failed receive temp"));
    }

    // Get heating return temp
    if (this->updateHeatingReturnTemp()) {
      Log.snoticeln(FPSTR(L_OT_HEATING), F("Received return temp: %.2f"), vars.temperatures.heatingReturn);

    } else {
      Log.swarningln(FPSTR(L_OT_HEATING), F("Failed receive return temp"));
    }

    // Get exhaust temp
    if (this->updateExhaustTemp()) {
      Log.snoticeln(FPSTR(L_OT), F("Received exhaust temp: %.2f"), vars.temperatures.exhaust);

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed receive exhaust temp"));
    }

    // If filtering is enabled, these parameters
    // must be updated every time.
    if (settings.opentherm.filterNumValues.enable) {
      // Get outdoor temp (if necessary)
      if (settings.sensors.outdoor.type == SensorType::BOILER) {
        if (this->updateOutdoorTemp()) {
          Log.snoticeln(FPSTR(L_OT), F("Received outdoor temp: %.2f"), vars.temperatures.outdoor);

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive outdoor temp"));
        }
      }
      
      // Get pressure
      if (this->updatePressure()) {
        Log.snoticeln(FPSTR(L_OT), F("Received pressure: %.2f"), vars.sensors.pressure);

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive pressure"));
      }
    }


    // Fault reset action
    if (vars.actions.resetFault) {
      if (vars.states.fault) {
        if (this->instance->sendBoilerReset()) {
          Log.sinfoln(FPSTR(L_OT), F("Boiler fault reset successfully"));

        } else {
          Log.serrorln(FPSTR(L_OT), F("Boiler fault reset failed"));
        }
      }

      vars.actions.resetFault = false;
    }

    // Diag reset action
    if (vars.actions.resetDiagnostic) {
      if (vars.states.diagnostic) {
        if (this->instance->sendServiceReset()) {
          Log.sinfoln(FPSTR(L_OT), F("Boiler diagnostic reset successfully"));
          
        } else {
          Log.serrorln(FPSTR(L_OT), F("Boiler diagnostic reset failed"));
        }
      }

      vars.actions.resetDiagnostic = false;
    }


    // Update DHW temp
    if (settings.opentherm.dhwPresent && settings.dhw.enable && (this->needSetDhwTemp() || fabs(settings.dhw.target - currentDhwTemp) > 0.0001f)) {
      float convertedTemp = convertTemp(settings.dhw.target, settings.system.unitSystem, settings.opentherm.unitSystem);
      Log.sinfoln(FPSTR(L_OT_DHW), F("Set temp: %.2f (converted: %.2f)"), settings.dhw.target, convertedTemp);

      // Set DHW temp
      if (this->instance->setDhwTemp(convertedTemp)) {
        currentDhwTemp = settings.dhw.target;
        this->dhwSetTempTime = millis();

      } else {
        Log.swarningln(FPSTR(L_OT_DHW), F("Failed set temp"));
      }

      // Set DHW temp to CH2
      if (settings.opentherm.dhwToCh2) {
        if (!this->instance->setHeatingCh2Temp(convertedTemp)) {
          Log.swarningln(FPSTR(L_OT_DHW), F("Failed set CH2 temp"));
        }
      }
    }


    // Native heating control
    if (settings.opentherm.nativeHeatingControl) {
      // Set current indoor temp
      float indoorTemp = 0.0f;
      float convertedTemp = 0.0f;

      if (!vars.states.emergency || settings.sensors.indoor.type != SensorType::MANUAL) {
        indoorTemp = vars.temperatures.indoor;
        convertedTemp = convertTemp(indoorTemp, settings.system.unitSystem, settings.opentherm.unitSystem);
      }

      Log.sinfoln(FPSTR(L_OT_HEATING), F("Set current indoor temp: %.2f (converted: %.2f)"), indoorTemp, convertedTemp);
      if (!this->instance->setRoomTemp(convertedTemp)) {
        Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set current indoor temp"));
      }

      // Set target indoor temp
      if (this->needSetHeatingTemp() || fabs(vars.parameters.heatingSetpoint - currentHeatingTemp) > 0.0001f) {
        convertedTemp = convertTemp(vars.parameters.heatingSetpoint, settings.system.unitSystem, settings.opentherm.unitSystem);
        Log.sinfoln(FPSTR(L_OT_HEATING), F("Set target indoor temp: %.2f (converted: %.2f)"), vars.parameters.heatingSetpoint, convertedTemp);

        if (this->instance->setRoomSetpoint(convertedTemp)) {
          currentHeatingTemp = vars.parameters.heatingSetpoint;
          this->heatingSetTempTime = millis();

        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set target indoor temp"));
        }

        // Set target temp to CH2
        if (settings.opentherm.heatingCh1ToCh2) {
          if (!this->instance->setRoomSetpointCh2(convertedTemp)) {
            Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set target indoor temp to CH2"));
          }
        }
      }

      // force enable pump
      if (!this->pump) {
        this->pump = true;
      }

    } else {
      // Update heating temp
      if (heatingEnabled && (this->needSetHeatingTemp() || fabs(vars.parameters.heatingSetpoint - currentHeatingTemp) > 0.0001f)) {
        float convertedTemp = convertTemp(vars.parameters.heatingSetpoint, settings.system.unitSystem, settings.opentherm.unitSystem);
        Log.sinfoln(FPSTR(L_OT_HEATING), F("Set temp: %.2f (converted: %.2f)"), vars.parameters.heatingSetpoint, convertedTemp);

        // Set max heating temp
        if (this->setMaxHeatingTemp(convertedTemp)) {
          currentHeatingTemp = vars.parameters.heatingSetpoint;
          this->heatingSetTempTime = millis();

        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set max heating temp"));
        }

        // Set heating temp
        if (this->instance->setHeatingCh1Temp(convertedTemp)) {
          currentHeatingTemp = vars.parameters.heatingSetpoint;
          this->heatingSetTempTime = millis();

        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set CH1 temp"));
        }

        // Set heating temp to CH2
        if (settings.opentherm.heatingCh1ToCh2) {
          if (!this->instance->setHeatingCh2Temp(convertedTemp)) {
            Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set CH2 temp"));
          }
        }
      }


      // Hysteresis
      // Only if enabled PID or/and Equitherm
      if (settings.heating.hysteresis > 0 && (!vars.states.emergency || settings.emergency.usePid) && (settings.equitherm.enable || settings.pid.enable)) {
        float halfHyst = settings.heating.hysteresis / 2;
        if (this->pump && vars.temperatures.indoor - settings.heating.target + 0.0001f >= halfHyst) {
          this->pump = false;

        } else if (!this->pump && vars.temperatures.indoor - settings.heating.target - 0.0001f <= -(halfHyst)) {
          this->pump = true;
        }

      } else if (!this->pump) {
        this->pump = true;
      }
    }
  }

  void initialize() {
    // Not all boilers support these, only try once when the boiler becomes connected
    if (this->updateSlaveVersion()) {
      Log.snoticeln(FPSTR(L_OT), F("Received slave version: %u, type: %u"), vars.parameters.slaveVersion, vars.parameters.slaveType);

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed receive slave version"));
    }

    // 0x013F
    if (this->setMasterVersion(0x3F, 0x01)) {
      Log.snoticeln(FPSTR(L_OT), F("Set master version: %u, type: %u"), vars.parameters.masterVersion, vars.parameters.masterType);
      
    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed set master version"));
    }

    if (this->updateSlaveOtVersion()) {
      Log.snoticeln(FPSTR(L_OT), F("Received slave OT version: %f"), vars.parameters.slaveOtVersion);

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed receive slave OT version"));
    }

    if (this->setMasterOtVersion(2.2f)) {
      Log.snoticeln(FPSTR(L_OT), F("Set master OT version: %f"), vars.parameters.masterOtVersion);

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed set master OT version"));
    }

    if (this->updateSlaveConfig()) {
      Log.snoticeln(FPSTR(L_OT), F("Received slave member id: %u, flags: %u"), vars.parameters.slaveMemberId, vars.parameters.slaveFlags);

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed receive slave config"));
    }

    if (this->setMasterConfig(settings.opentherm.memberIdCode & 0xFF, (settings.opentherm.memberIdCode & 0xFFFF) >> 8)) {
      Log.snoticeln(FPSTR(L_OT), F("Set master member id: %u, flags: %u"), vars.parameters.masterMemberId, vars.parameters.masterFlags);
      
    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed set master config"));
    }
  }

  bool isReady() {
    return millis() - this->instanceCreatedTime > this->readyTime;
  }

  bool needSetDhwTemp() {
    return millis() - this->dhwSetTempTime > this->dhwSetTempInterval;
  }

  bool needSetHeatingTemp() {
    return millis() - this->heatingSetTempTime > this->heatingSetTempInterval;
  }

  bool updateSlaveConfig() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::SConfigSMemberIDcode,
      0
    ));
    
    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.parameters.slaveMemberId = response & 0xFF;
    vars.parameters.slaveFlags = (response & 0xFFFF) >> 8;

    /*uint8_t flags = (response & 0xFFFF) >> 8;
    Log.straceln(
      "OT",
      F("MasterMemberIdCode:\r\n  DHW present: %u\r\n  Control type: %u\r\n  Cooling configuration: %u\r\n  DHW configuration: %u\r\n  Pump control: %u\r\n  CH2 present: %u\r\n  Remote water filling function: %u\r\n  Heat/cool mode control: %u\r\n  Slave MemberID Code: %u\r\n  Raw: %u"),
      (bool) (flags & 0x01),
      (bool) (flags & 0x02),
      (bool) (flags & 0x04),
      (bool) (flags & 0x08),
      (bool) (flags & 0x10),
      (bool) (flags & 0x20),
      (bool) (flags & 0x40),
      (bool) (flags & 0x80),
      response & 0xFF,
      response
    );*/

    return true;
  }

  /**
   * @brief Set the Master Config
   * From slave member id code:
   * id: slave.memberIdCode & 0xFF,
   * flags: (slave.memberIdCode & 0xFFFF) >> 8
   * @param id 
   * @param flags 
   * @param force 
   * @return true 
   * @return false 
   */
  bool setMasterConfig(uint8_t id, uint8_t flags, bool force = false) {
    //uint8_t configId = settings.opentherm.memberIdCode & 0xFF;
    //uint8_t configFlags = (settings.opentherm.memberIdCode & 0xFFFF) >> 8;

    vars.parameters.masterMemberId = (force || id || settings.opentherm.memberIdCode > 65535) 
      ? id 
      : vars.parameters.slaveMemberId;

    vars.parameters.masterFlags = (force || flags || settings.opentherm.memberIdCode > 65535)
      ? flags
      : vars.parameters.slaveFlags;

    unsigned int request = (unsigned int) vars.parameters.masterMemberId | (unsigned int) vars.parameters.masterFlags << 8;
    // if empty request
    if (!request) {
      return true;
    }

    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::MConfigMMemberIDcode,
      request
    ));

    return CustomOpenTherm::isValidResponse(response);
  }

  bool setMaxModulationLevel(byte value) {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::MaxRelModLevelSetting,
      CustomOpenTherm::toFloat(value)
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.parameters.maxModulation = CustomOpenTherm::getFloat(response);
    return true;
  }

  bool updateSlaveOtVersion() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::OpenThermVersionSlave,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.parameters.slaveOtVersion = CustomOpenTherm::getFloat(response);
    return true;
  }

  bool setMasterOtVersion(float version) {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::OpenThermVersionMaster,
      CustomOpenTherm::toFloat(version)
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.parameters.masterOtVersion = CustomOpenTherm::getFloat(response);

    return true;
  }

  bool updateSlaveVersion() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::SlaveVersion,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.parameters.slaveVersion = response & 0xFF;
    vars.parameters.slaveType = (response & 0xFFFF) >> 8;

    return true;
  }

  bool setMasterVersion(uint8_t version, uint8_t type) {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::MasterVersion,
      (unsigned int) version | (unsigned int) type << 8
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.parameters.masterVersion = response & 0xFF;
    vars.parameters.masterType = (response & 0xFFFF) >> 8;

    return true;
  }

  bool updateMinMaxDhwTemp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::TdhwSetUBTdhwSetLB,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    byte minTemp = response & 0xFF;
    byte maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.parameters.dhwMinTemp = convertTemp(minTemp, settings.opentherm.unitSystem, settings.system.unitSystem);
      vars.parameters.dhwMaxTemp = convertTemp(maxTemp, settings.opentherm.unitSystem, settings.system.unitSystem);

      return true;
    }

    return false;
  }

  bool updateMinMaxHeatingTemp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::MaxTSetUBMaxTSetLB,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    byte minTemp = response & 0xFF;
    byte maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.parameters.heatingMinTemp = convertTemp(minTemp, settings.opentherm.unitSystem, settings.system.unitSystem);
      vars.parameters.heatingMaxTemp = convertTemp(maxTemp, settings.opentherm.unitSystem, settings.system.unitSystem);
      return true;
    }

    return false;
  }

  bool setMaxHeatingTemp(byte value) {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::MaxTSet,
      CustomOpenTherm::temperatureToData(value)
    ));

    return CustomOpenTherm::isValidResponse(response);
  }

  bool updateOutdoorTemp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::Toutside,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }
    
    float value = settings.sensors.outdoor.offset + convertTemp(
      CustomOpenTherm::getFloat(response),
      settings.opentherm.unitSystem,
      settings.system.unitSystem
    );

    if (settings.opentherm.filterNumValues.enable && fabs(vars.temperatures.outdoor) >= 0.1f) {
      vars.temperatures.outdoor += (value - vars.temperatures.outdoor) * settings.opentherm.filterNumValues.factor;
      
    } else {
      vars.temperatures.outdoor = value;
    }

    return true;
  }

  bool updateExhaustTemp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::Texhaust,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    float value = (float) CustomOpenTherm::getInt(response);
    if (!isValidTemp(value, settings.opentherm.unitSystem, -40, 500)) {
      return false;
    }

    value = convertTemp(
      value,
      settings.opentherm.unitSystem,
      settings.system.unitSystem
    );

    if (settings.opentherm.filterNumValues.enable && fabs(vars.temperatures.exhaust) >= 0.1f) {
      vars.temperatures.exhaust += (value - vars.temperatures.exhaust) * settings.opentherm.filterNumValues.factor;
      
    } else {
      vars.temperatures.exhaust = value;
    }

    return true;
  }

  bool updateHeatingTemp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tboiler,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    float value = CustomOpenTherm::getFloat(response);
    if (value <= 0) {
      return false;
    }

    value = convertTemp(
      value,
      settings.opentherm.unitSystem,
      settings.system.unitSystem
    );

    if (settings.opentherm.filterNumValues.enable && fabs(vars.temperatures.heating) >= 0.1f) {
      vars.temperatures.heating += (value - vars.temperatures.heating) * settings.opentherm.filterNumValues.factor;

    } else {
      vars.temperatures.heating = value;
    }

    return true;
  }

  bool updateHeatingReturnTemp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tret,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    float value = convertTemp(
      CustomOpenTherm::getFloat(response),
      settings.opentherm.unitSystem,
      settings.system.unitSystem
    );

    if (settings.opentherm.filterNumValues.enable && fabs(vars.temperatures.heatingReturn) >= 0.1f) {
      vars.temperatures.heatingReturn += (value - vars.temperatures.heatingReturn) * settings.opentherm.filterNumValues.factor;
      
    } else {
      vars.temperatures.heatingReturn = value;
    }
    

    return true;
  }


  bool updateDhwTemp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tdhw,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    float value = CustomOpenTherm::getFloat(response);
    if (value <= 0) {
      return false;
    }

    value = convertTemp(
      value,
      settings.opentherm.unitSystem,
      settings.system.unitSystem
    );

    if (settings.opentherm.filterNumValues.enable && fabs(vars.temperatures.dhw) >= 0.1f) {
      vars.temperatures.dhw += (value - vars.temperatures.dhw) * settings.opentherm.filterNumValues.factor;
      
    } else {
      vars.temperatures.dhw = value;
    }

    return true;
  }

  bool updateDhwFlowRate() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::DHWFlowRate,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    float value = CustomOpenTherm::getFloat(response);
    if (value < 0 || value > convertVolume(16, UnitSystem::METRIC, settings.opentherm.unitSystem)) {
      return false;
    }

    value = convertVolume(
      value * settings.opentherm.dhwFlowRateFactor,
      settings.opentherm.unitSystem,
      settings.system.unitSystem
    );

    vars.sensors.dhwFlowRate = value > 0.09f ? value : 0.0f;
    
    return true;
  }

  bool updateFaultCode() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::ASFflags,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.sensors.faultCode = response & 0xFF;
    return true;
  }

  bool updateDiagCode() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::OEMDiagnosticCode,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.sensors.diagnosticCode = CustomOpenTherm::getUInt(response);
    return true;
  }

  bool updateModulationLevel() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::RelModLevel,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    float value = CustomOpenTherm::getFloat(response);
    if (value < 0) {
      return false;
    }

    if (settings.opentherm.filterNumValues.enable && fabs(vars.sensors.modulation) >= 0.1f) {
      vars.sensors.modulation += (value - vars.sensors.modulation) * settings.opentherm.filterNumValues.factor;
      
    } else {
      vars.sensors.modulation = value;
    }

    return true;
  }

  bool updateMinModulationLevel() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::MaxCapacityMinModLevel,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    vars.parameters.minModulation = response & 0xFF;
    vars.parameters.maxPower = (response & 0xFFFF) >> 8;

    return true;
  }

  bool updatePressure() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::CHPressure,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;
    }

    float value = CustomOpenTherm::getFloat(response);
    if (value < 0 || value > convertPressure(5, UnitSystem::METRIC, settings.opentherm.unitSystem)) {
      return false;
    }

    value = convertPressure(
      value * settings.opentherm.pressureFactor,
      settings.opentherm.unitSystem,
      settings.system.unitSystem
    );

    if (settings.opentherm.filterNumValues.enable && fabs(vars.sensors.pressure) >= 0.1f) {
      vars.sensors.pressure += (value - vars.sensors.pressure) * settings.opentherm.filterNumValues.factor;
      
    } else {
      vars.sensors.pressure = value;
    }

    return true;
  }
};
