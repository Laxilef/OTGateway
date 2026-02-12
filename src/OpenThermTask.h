#include <CustomOpenTherm.h>
extern FileData fsSettings;

class OpenThermTask : public Task {
public:
  OpenThermTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

  ~OpenThermTask() {
    delete this->instance;
  }

protected:
  const unsigned short readyTime = 60000u;
  const unsigned int resetBusInterval = 120000u;
  const unsigned short heatingSetTempInterval = 60000u;
  const unsigned short dhwSetTempInterval = 60000u;
  const unsigned short ch2SetTempInterval = 60000u;
  const unsigned int initializingInterval = 3600000u;

  CustomOpenTherm* instance = nullptr;
  unsigned long instanceCreatedTime = 0;
  uint8_t instanceInGpio = 0;
  uint8_t instanceOutGpio = 0;
  bool initialized = false;
  unsigned long connectedTime = 0;
  unsigned long disconnectedTime = 0;
  unsigned long resetBusTime = 0;
  unsigned long initializedTime = 0;
  unsigned long lastSuccessResponse = 0;
  unsigned long prevUpdateNonEssentialVars = 0;
  unsigned long heatingSetTempTime = 0;
  unsigned long dhwSetTempTime = 0;
  unsigned long ch2SetTempTime = 0;
  uint8_t configuredRxLedGpio = GPIO_IS_NOT_CONFIGURED;

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
    // Convert defaults at start
    if (settings.system.unitSystem != UnitSystem::METRIC) {
      vars.slave.heating.minTemp = convertTemp(vars.slave.heating.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
      vars.slave.heating.maxTemp = convertTemp(vars.slave.heating.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);
      vars.slave.dhw.minTemp = convertTemp(vars.slave.dhw.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
      vars.slave.dhw.maxTemp = convertTemp(vars.slave.dhw.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);
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

    #ifdef OT_BYPASS_RELAY_GPIO
    pinMode(OT_BYPASS_RELAY_GPIO, OUTPUT);
    digitalWrite(OT_BYPASS_RELAY_GPIO, true);
    #endif

    // create instance
    this->instance = new CustomOpenTherm(settings.opentherm.inGpio, settings.opentherm.outGpio);

    // flags
    this->instanceCreatedTime = millis();
    this->instanceInGpio = settings.opentherm.inGpio;
    this->instanceOutGpio = settings.opentherm.outGpio;
    this->resetBusTime = millis();
    this->initialized = false;

    Log.sinfoln(FPSTR(L_OT), F("Started. GPIO IN: %hhu, GPIO OUT: %hhu"), settings.opentherm.inGpio, settings.opentherm.outGpio);

    this->instance->setAfterSendRequestCallback([this](unsigned long request, unsigned long response, OpenThermResponseStatus status, uint8_t attempt) {
      Log.sverboseln(
        FPSTR(L_OT),
        F("ID: %4d   Request: %8lx   Response: %8lx   Msg type: %s   Attempt: %2d   Status: %s"),
        CustomOpenTherm::getDataID(request),
        request,
        response,
        CustomOpenTherm::getResponseMessageTypeString(response),
        attempt,
        CustomOpenTherm::statusToString(status)
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

    this->instance->setDelayCallback([this](unsigned int time) {
      this->delay(time);
    });
  }

  void loop() {
    if (vars.states.restarting || vars.states.upgrading) {
      return;
    }

    if (this->instanceInGpio != settings.opentherm.inGpio || this->instanceOutGpio != settings.opentherm.outGpio) {
      this->setup();

    } else if (vars.master.memberId != settings.opentherm.memberId || vars.master.flags != settings.opentherm.flags) {
      this->initialized = false;
      vars.master.memberId = settings.opentherm.memberId;
      vars.master.flags = settings.opentherm.flags;
      vars.master.protocolVersion = 2.2f;
      vars.master.appVersion = 0x3F;
      vars.master.type = 0x01;

    } else if (millis() - this->initializedTime > this->initializingInterval) {
      this->initialized = false;
    }

    if (this->instance == nullptr) {
      this->delay(5000);
      return;

    } else if (this->instance->status == OpenThermStatus::NOT_INITIALIZED) {
      if (!this->instance->begin()) {
        Log.swarningln(FPSTR(L_OT), F("Failed begin"));

        this->delay(5000);
        return;
      }
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

    // Heating settings
    vars.master.heating.enabled = this->isReady()
      && settings.heating.enabled
      && (vars.master.heating.freezing || (vars.cascadeControl.input && !vars.master.heating.blocking))
      && !vars.master.heating.overheat;

    // DHW settings
    vars.master.dhw.enabled = settings.opentherm.options.dhwSupport 
      && settings.dhw.enabled 
      && !vars.master.dhw.overheat;
    vars.master.dhw.targetTemp = settings.dhw.target;

    // CH2 settings
    vars.master.ch2.enabled = settings.opentherm.options.ch2AlwaysEnabled
      || (settings.opentherm.options.heatingToCh2 && vars.master.heating.enabled)
      || (settings.opentherm.options.dhwToCh2 && settings.opentherm.options.dhwSupport && settings.dhw.enabled);

    if (settings.opentherm.options.heatingToCh2) {
      vars.master.ch2.targetTemp = !settings.opentherm.options.nativeOTC
        ? vars.master.heating.setpointTemp
        : vars.master.heating.targetTemp;

    } else if (settings.opentherm.options.dhwToCh2) {
      vars.master.ch2.targetTemp = vars.master.dhw.targetTemp;
    }

    // Set boiler status LB
    // Some boilers require this, although this is against protocol
    uint8_t statusLb = 0;

    // Immergas fix
    // https://arduino.ru/forum/programmirovanie/termostat-opentherm-na-esp8266?page=15#comment-649392
    if (settings.opentherm.options.immergasFix) {
      statusLb = 0xCA;
    }

    // Summer/winter mode
    bool summerWinterMode = settings.opentherm.options.summerWinterMode;
    if (settings.opentherm.options.heatingStateToSummerWinterMode) {
      summerWinterMode = vars.master.heating.enabled == summerWinterMode;
    }

    // DHW blocking
    bool dhwBlocking = settings.opentherm.options.dhwBlocking;
    if (settings.opentherm.options.dhwStateAsDhwBlocking) {
      dhwBlocking = vars.master.dhw.enabled == dhwBlocking;
    }

    unsigned long response = this->instance->setBoilerStatus(
      vars.master.heating.enabled,
      vars.master.dhw.enabled,
      settings.opentherm.options.coolingSupport,
      settings.opentherm.options.nativeOTC,
      vars.master.ch2.enabled,
      summerWinterMode,
      dhwBlocking,
      statusLb
    );

    if (!CustomOpenTherm::isValidResponse(response) || !CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Status)) {
      Log.swarningln(
        FPSTR(L_OT),
        F("Failed receive boiler status: %s"),
        CustomOpenTherm::statusToString(this->instance->getLastResponseStatus())
      );
      
    } else {
      vars.slave.heating.active = CustomOpenTherm::isCentralHeatingActive(response);
      vars.slave.dhw.active = settings.opentherm.options.dhwSupport ? CustomOpenTherm::isHotWaterActive(response) : false;
      vars.slave.flame = CustomOpenTherm::isFlameOn(response);
      vars.slave.cooling.active = CustomOpenTherm::isCoolingActive(response);
      vars.slave.ch2.active = CustomOpenTherm::isCh2Active(response);
      vars.slave.fault.active = CustomOpenTherm::isFault(response);

      if (!settings.opentherm.options.ignoreDiagState) {
        vars.slave.diag.active = CustomOpenTherm::isDiagnostic(response);

      } else if (vars.slave.diag.active) {
        vars.slave.diag.active = false;
      }
  
      Log.snoticeln(
        FPSTR(L_OT), F("Received boiler status. Heating: %hhu; DHW: %hhu; flame: %hhu; cooling: %hhu; channel 2: %hhu; fault: %hhu; diag: %hhu"),
        vars.slave.heating.active, vars.slave.dhw.active,
        vars.slave.flame, vars.slave.cooling.active, vars.slave.ch2.active, vars.slave.fault.active, vars.slave.diag.active
      );
    }

    // 5 request retries
    // 1000ms maximum response waiting time
    // 100ms delay between requests
    // +15%
    // 5 * (1000 + 100) * 1.15 = 6325 ms
    if (!vars.slave.connected && millis() - this->lastSuccessResponse < 6325) {
      Log.sinfoln(
        FPSTR(L_OT),
        F("Connected, downtime: %lu s."),
        (millis() - this->disconnectedTime) / 1000
      );
      
      this->connectedTime = millis();
      vars.slave.connected = true;
      
    } else if (vars.slave.connected && millis() - this->lastSuccessResponse > 6325) {
      Log.swarningln(
        FPSTR(L_OT),
        F("Disconnected, uptime: %lu s."),
        (millis() - this->connectedTime) / 1000  
      );

      // Mark sensors as disconnected
      Sensors::setConnectionStatusByType(Sensors::Type::OT_OUTDOOR_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_RETURN_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_TEMP2, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_FLOW_RATE, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_CH2_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_EXHAUST_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_HEAT_EXCHANGER_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_PRESSURE, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_MODULATION_LEVEL, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_CURRENT_POWER, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_EXHAUST_CO2, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_EXHAUST_FAN_SPEED, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_SUPPLY_FAN_SPEED, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_SOLAR_STORAGE_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_SOLAR_COLLECTOR_TEMP, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_FAN_SPEED_SETPOINT, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_FAN_SPEED_CURRENT, false);

      Sensors::setConnectionStatusByType(Sensors::Type::OT_BURNER_STARTS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_BURNER_STARTS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_PUMP_STARTS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_PUMP_STARTS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_BURNER_HOURS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_BURNER_HOURS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_PUMP_HOURS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_PUMP_HOURS, false);
      Sensors::setConnectionStatusByType(Sensors::Type::OT_COOLING_HOURS, false);

      this->initialized = false;
      this->disconnectedTime = millis();
      vars.slave.connected = false;
    }

    // If boiler is disconnected, no need try setting other OT stuff
    if (!vars.slave.connected) {
      vars.slave.heating.enabled = false;
      vars.slave.heating.active = false;
      vars.slave.dhw.enabled = false;
      vars.slave.dhw.active = false;
      vars.slave.flame = false;
      vars.slave.cooling.active = false;
      vars.slave.cooling.setpoint = 0;
      vars.slave.fault.active = false;
      vars.slave.fault.code = 0;
      vars.slave.diag.active = false;
      vars.slave.diag.code = 0;

      // reset bus
      if (millis() - this->disconnectedTime > this->resetBusInterval) {
        if (millis() - this->resetBusTime > this->resetBusInterval) {
          Log.sinfoln(FPSTR(L_OT), F("Reset bus..."));

          this->instance->end();
          this->instance->status = OpenThermStatus::NOT_INITIALIZED;

          digitalWrite(this->instanceOutGpio, LOW);
          this->resetBusTime = millis();
          this->delay(5000);
        }
      }

      return;
    }

    if (!this->initialized) {
      Log.sinfoln(FPSTR(L_OT), F("Initializing..."));
      this->initialized = true;
      this->initializedTime = millis();
      this->initialize();
    }

    if (vars.master.heating.enabled != vars.slave.heating.enabled) {
      this->prevUpdateNonEssentialVars = 0;
      vars.slave.heating.enabled = vars.master.heating.enabled;
      Log.sinfoln(FPSTR(L_OT_HEATING), vars.master.heating.enabled ? F("Enabled") : F("Disabled"));
    }

    if (vars.master.dhw.enabled != vars.slave.dhw.enabled) {
      this->prevUpdateNonEssentialVars = 0;
      vars.slave.dhw.enabled = vars.master.dhw.enabled;
      Log.sinfoln(FPSTR(L_OT_DHW), vars.master.dhw.enabled ? F("Enabled") : F("Disabled"));
    }

    // These parameters will be updated every minute
    if (millis() - this->prevUpdateNonEssentialVars > 60000) {
      // Set date & time
      if (settings.opentherm.options.setDateAndTime) {
        struct tm ti;

        if (getLocalTime(&ti)) {
          if (this->setYear(&ti)) {
            Log.sinfoln(FPSTR(L_OT), F("Year of date set successfully"));

          } else {
            Log.sinfoln(FPSTR(L_OT), F("Failed set year of date"));
          }

          if (this->setDayAndMonth(&ti)) {
            Log.sinfoln(FPSTR(L_OT), F("Day and month of date set successfully"));

          } else {
            Log.sinfoln(FPSTR(L_OT), F("Failed set day and month of date"));
          }

          if (this->setTime(&ti)) {
            Log.sinfoln(FPSTR(L_OT), F("Time set successfully"));

          } else {
            Log.sinfoln(FPSTR(L_OT), F("Failed set time"));
          }
        }
      }

      // Get min modulation level & max power
      if (this->updateMinModulationLevel()) {
        Log.snoticeln(
          FPSTR(L_OT), F("Received min modulation: %hhu%%, max power: %.2f kW"),
          vars.slave.modulation.min, vars.slave.power.max
        );
        
        if (settings.heating.maxModulation < vars.slave.modulation.min) {
          settings.heating.maxModulation = vars.slave.modulation.min;
          fsSettings.update();

          Log.swarningln(
            FPSTR(L_SETTINGS_HEATING), F("Updated min modulation: %hhu%%"),
            settings.heating.maxModulation
          );
        }

        if (settings.dhw.maxModulation < vars.slave.modulation.min) {
          settings.dhw.maxModulation = vars.slave.modulation.min;
          fsSettings.update();

          Log.swarningln(
            FPSTR(L_SETTINGS_DHW), F("Updated min modulation: %hhu%%"),
            settings.dhw.maxModulation
          );
        }

        if (fabsf(settings.opentherm.maxPower) < 0.1f && vars.slave.power.max > 0.1f) {
          settings.opentherm.maxPower = vars.slave.power.max;
          settings.opentherm.minPower = vars.slave.power.min;

          fsSettings.update();
          Log.swarningln(
            FPSTR(L_SETTINGS_OT), F("Updated power, min: %.2f kW, max: %.2f kW"),
            settings.opentherm.minPower, settings.opentherm.maxPower
          );
        }

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive min modulation and max power"));
      }


      // Get DHW min/max temp (if necessary)
      if (settings.opentherm.options.dhwSupport && settings.opentherm.options.getMinMaxTemp) {
        if (this->updateMinMaxDhwTemp()) {
          uint8_t convertedMinTemp = convertTemp(
            vars.slave.dhw.minTemp,
            settings.opentherm.unitSystem,
            settings.system.unitSystem
          );

          uint8_t convertedMaxTemp = convertTemp(
            vars.slave.dhw.maxTemp,
            settings.opentherm.unitSystem,
            settings.system.unitSystem
          );

          Log.snoticeln(
            FPSTR(L_OT_DHW), F("Received min temp: %hhu (converted: %hhu), max temp: %hhu (converted: %hhu)"),
            vars.slave.dhw.minTemp, convertedMinTemp, vars.slave.dhw.maxTemp, convertedMaxTemp
          );

          if (settings.dhw.minTemp < convertedMinTemp) {
            settings.dhw.minTemp = convertedMinTemp;
            fsSettings.update();

            Log.swarningln(FPSTR(L_SETTINGS_DHW), F("Updated min temp: %hhu"), settings.dhw.minTemp);
          }

          if (settings.dhw.maxTemp > convertedMaxTemp) {
            settings.dhw.maxTemp = convertedMaxTemp;
            fsSettings.update();

            Log.swarningln(FPSTR(L_SETTINGS_DHW), F("Updated max temp: %hhu"), settings.dhw.maxTemp);
          }

        } else {
          Log.swarningln(FPSTR(L_OT_DHW), F("Failed receive min/max temp"));
        }
      }

      if (settings.dhw.minTemp >= settings.dhw.maxTemp) {
        settings.dhw.minTemp = convertTemp(DEFAULT_DHW_MIN_TEMP, UnitSystem::METRIC, settings.system.unitSystem);
        settings.dhw.maxTemp = convertTemp(DEFAULT_DHW_MAX_TEMP, UnitSystem::METRIC, settings.system.unitSystem);
        fsSettings.update();
      }


      // Get heating min/max temp
      if (settings.opentherm.options.getMinMaxTemp) {
        if (this->updateMinMaxHeatingTemp()) {
          uint8_t convertedMinTemp = convertTemp(
            vars.slave.heating.minTemp,
            settings.opentherm.unitSystem,
            settings.system.unitSystem
          );

          uint8_t convertedMaxTemp = convertTemp(
            vars.slave.heating.maxTemp,
            settings.opentherm.unitSystem,
            settings.system.unitSystem
          );

          Log.snoticeln(
            FPSTR(L_OT_HEATING), F("Received min temp: %hhu (converted: %hhu), max temp: %hhu (converted: %hhu)"),
            vars.slave.heating.minTemp, convertedMinTemp, vars.slave.heating.maxTemp, convertedMaxTemp
          );

          if (settings.heating.minTemp < convertedMinTemp) {
            settings.heating.minTemp = convertedMinTemp;
            fsSettings.update();

            Log.swarningln(FPSTR(L_SETTINGS_HEATING), F("Updated min temp: %hhu"), settings.heating.minTemp);
          }

          if (settings.heating.maxTemp > convertedMaxTemp) {
            settings.heating.maxTemp = convertedMaxTemp;
            fsSettings.update();

            Log.swarningln(FPSTR(L_SETTINGS_HEATING), F("Updated max temp: %hhu"), settings.heating.maxTemp);
          }
          
        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed receive min/max temp"));
        }
      }

      if (settings.heating.minTemp >= settings.heating.maxTemp) {
        settings.heating.minTemp = convertTemp(DEFAULT_HEATING_MIN_TEMP, UnitSystem::METRIC, settings.system.unitSystem);;
        settings.heating.maxTemp = convertTemp(DEFAULT_HEATING_MAX_TEMP, UnitSystem::METRIC, settings.system.unitSystem);;
        fsSettings.update();
      }

      // Get fault code (if necessary)
      if (vars.slave.fault.active) {
        if (this->updateFaultCode()) {
          Log.snoticeln(
            FPSTR(L_OT), F("Received fault code: %hhu (0x%02X)"),
            vars.slave.fault.code, vars.slave.fault.code
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive fault code"));
        }
        
      } else if (vars.slave.fault.code != 0) {
        vars.slave.fault.code = 0;
      }

      // Get diagnostic code (if necessary)
      if (vars.slave.fault.active || vars.slave.diag.active) {
        if (this->updateDiagCode()) {
          Log.snoticeln(
            FPSTR(L_OT), F("Received diag code: %hu (0x%02X)"),
            vars.slave.diag.code, vars.slave.diag.code
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive diag code"));
        }
        
      } else if (vars.slave.diag.code != 0) {
        vars.slave.diag.code = 0;
      }

      // Update burner starts
      if (Sensors::getAmountByType(Sensors::Type::OT_BURNER_STARTS, true)) {
        if (this->updateBurnerStarts()) {
          Log.snoticeln(FPSTR(L_OT), F("Received burner starts: %hu"), vars.slave.stats.burnerStarts);

          Sensors::setValueByType(
            Sensors::Type::OT_BURNER_STARTS, vars.slave.stats.burnerStarts,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive burner starts"));
        }
      }

      // Update DHW burner starts
      if (Sensors::getAmountByType(Sensors::Type::OT_DHW_BURNER_STARTS, true)) {
        if (this->updateDhwBurnerStarts()) {
          Log.snoticeln(FPSTR(L_OT), F("Received DHW burner starts: %hu"), vars.slave.stats.dhwBurnerStarts);

          Sensors::setValueByType(
            Sensors::Type::OT_DHW_BURNER_STARTS, vars.slave.stats.dhwBurnerStarts,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive DHW burner starts"));
        }
      }

      // Update heating pump starts
      if (Sensors::getAmountByType(Sensors::Type::OT_HEATING_PUMP_STARTS, true)) {
        if (this->updateHeatingPumpStarts()) {
          Log.snoticeln(FPSTR(L_OT), F("Received heating pump starts: %hu"), vars.slave.stats.heatingPumpStarts);

          Sensors::setValueByType(
            Sensors::Type::OT_HEATING_PUMP_STARTS, vars.slave.stats.heatingPumpStarts,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive heating pump starts"));
        }
      }

      // Update DHW pump starts
      if (Sensors::getAmountByType(Sensors::Type::OT_DHW_PUMP_STARTS, true)) {
        if (this->updateDhwPumpStarts()) {
          Log.snoticeln(FPSTR(L_OT), F("Received DHW pump starts: %hu"), vars.slave.stats.dhwPumpStarts);

          Sensors::setValueByType(
            Sensors::Type::OT_DHW_PUMP_STARTS, vars.slave.stats.dhwPumpStarts,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive DHW pump starts"));
        }
      }

      // Update burner hours
      if (Sensors::getAmountByType(Sensors::Type::OT_BURNER_HOURS, true)) {
        if (this->updateBurnerHours()) {
          Log.snoticeln(FPSTR(L_OT), F("Received burner hours: %hu"), vars.slave.stats.burnerHours);

          Sensors::setValueByType(
            Sensors::Type::OT_BURNER_HOURS, vars.slave.stats.burnerHours,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive burner hours"));
        }
      }

      // Update DHW burner hours
      if (Sensors::getAmountByType(Sensors::Type::OT_DHW_BURNER_HOURS, true)) {
        if (this->updateDhwBurnerHours()) {
          Log.snoticeln(FPSTR(L_OT), F("Received DHW burner hours: %hu"), vars.slave.stats.dhwBurnerHours);

          Sensors::setValueByType(
            Sensors::Type::OT_DHW_BURNER_HOURS, vars.slave.stats.dhwBurnerHours,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive DHW burner hours"));
        }
      }

      // Update heating pump hours
      if (Sensors::getAmountByType(Sensors::Type::OT_HEATING_PUMP_HOURS, true)) {
        if (this->updateHeatingPumpHours()) {
          Log.snoticeln(FPSTR(L_OT), F("Received heating pump hours: %hu"), vars.slave.stats.heatingPumpHours);

          Sensors::setValueByType(
            Sensors::Type::OT_HEATING_PUMP_HOURS, vars.slave.stats.heatingPumpHours,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive heating pump hours"));
        }
      }

      // Update DHW pump hours
      if (Sensors::getAmountByType(Sensors::Type::OT_DHW_PUMP_HOURS, true)) {
        if (this->updateDhwPumpHours()) {
          Log.snoticeln(FPSTR(L_OT), F("Received DHW pump hours: %hu"), vars.slave.stats.dhwPumpHours);

          Sensors::setValueByType(
            Sensors::Type::OT_DHW_PUMP_HOURS, vars.slave.stats.dhwPumpHours,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive DHW pump hours"));
        }
      }

      // Update cooling hours
      if (Sensors::getAmountByType(Sensors::Type::OT_COOLING_HOURS, true)) {
        if (this->updateCoolingHours()) {
          Log.snoticeln(FPSTR(L_OT), F("Received cooling hours: %hu"), vars.slave.stats.coolingHours);

          Sensors::setValueByType(
            Sensors::Type::OT_COOLING_HOURS, vars.slave.stats.coolingHours,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive cooling hours"));
        }
      }

      // Auto fault reset
      if (settings.opentherm.options.autoFaultReset && vars.slave.fault.active && !vars.actions.resetFault) {
        vars.actions.resetFault = true;
      }

      // Auto diag reset
      if (settings.opentherm.options.autoDiagReset && vars.slave.diag.active && !vars.actions.resetDiagnostic) {
        vars.actions.resetDiagnostic = true;
      }

      this->prevUpdateNonEssentialVars = millis();
    }

    // Set cooling setpoint = heating max modulation
    if (settings.opentherm.options.coolingSupport) {
      if (this->setCoolingSetpoint(settings.heating.maxModulation)) {
        Log.snoticeln(
          FPSTR(L_OT), F("Set cooling setpoint: %hhu%% (response: %hhu%%)"),
          settings.heating.maxModulation, vars.slave.cooling.setpoint
        );

      } else {
        Log.swarningln(
          FPSTR(L_OT), F("Failed set cooling setpoint: %hhu%% (response: %hhu%%)"),
          settings.heating.maxModulation, vars.slave.cooling.setpoint
        );
      }
    }

    // Set max modulation level
    uint8_t targetMaxModulation = vars.slave.modulation.max;
    if (vars.slave.heating.active) {
      targetMaxModulation = settings.heating.maxModulation;

    } else if (vars.slave.dhw.active) {
      targetMaxModulation = settings.dhw.maxModulation;
    }

    if (this->setMaxModulationLevel(targetMaxModulation)) {
      Log.snoticeln(
        FPSTR(L_OT), F("Set max modulation: %hhu%% (response: %hhu%%)"),
        targetMaxModulation, vars.slave.modulation.max
      );

    } else {
      Log.swarningln(
        FPSTR(L_OT), F("Failed set max modulation: %hhu%% (response: %hhu%%)"),
        targetMaxModulation, vars.slave.modulation.max
      );
    }

    // Update modulation level
    if (
      Sensors::getAmountByType(Sensors::Type::OT_MODULATION_LEVEL, true) ||
      Sensors::getAmountByType(Sensors::Type::OT_CURRENT_POWER, true)
    ) {
      if (vars.slave.flame) {
        if (this->updateModulationLevel()) {
          float power = 0.0f;
          if (settings.opentherm.maxPower > 0.1f) {
            power += settings.opentherm.minPower;

            if (vars.slave.modulation.current > 0) {
              power += (
                settings.opentherm.maxPower - settings.opentherm.minPower
              ) / 100.0f * vars.slave.modulation.current;
            }
          }
          vars.slave.power.current = power;
          
          Log.snoticeln(
            FPSTR(L_OT), F("Received modulation level: %hhu%%, power: %.2f of %.2f kW (min: %.2f kW)"),
            vars.slave.modulation.current, vars.slave.power.current,
            settings.opentherm.maxPower, settings.opentherm.minPower
          );

          // Modulation level sensors
          Sensors::setValueByType(
            Sensors::Type::OT_MODULATION_LEVEL, vars.slave.modulation.current,
            Sensors::ValueType::PRIMARY, true, true
          );

          // Power sensors
          Sensors::setValueByType(
            Sensors::Type::OT_CURRENT_POWER, vars.slave.power.current,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT), F("Failed receive modulation level"));
        }

      } else {
        vars.slave.modulation.current = 0;
        vars.slave.power.current = 0.0f;

        // Modulation level sensors
        Sensors::setValueByType(
          Sensors::Type::OT_MODULATION_LEVEL, vars.slave.modulation.current,
          Sensors::ValueType::PRIMARY, true, true
        );

        // Power sensors
        Sensors::setValueByType(
          Sensors::Type::OT_CURRENT_POWER, vars.slave.power.current,
          Sensors::ValueType::PRIMARY, true, true
        );
      }
    }

    // Update DHW temp
    if (settings.opentherm.options.dhwSupport && Sensors::getAmountByType(Sensors::Type::OT_DHW_TEMP, true)) {
      bool result = this->updateDhwTemp();

      if (result) {
        const float convertedDhwTemp = convertTemp(
          vars.slave.dhw.currentTemp,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT_DHW), F("Received temp: %.2f (converted: %.2f)"),
          vars.slave.dhw.currentTemp, convertedDhwTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_DHW_TEMP, convertedDhwTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT_DHW), F("Failed receive temp"));
      }
    }

    // Update DHW temp 2
    if (settings.opentherm.options.dhwSupport && Sensors::getAmountByType(Sensors::Type::OT_DHW_TEMP2, true)) {
      if (this->updateDhwTemp2()) {
        const float convertedDhwTemp2 = convertTemp(
          vars.slave.dhw.currentTemp2,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT_DHW), F("Received temp 2: %.2f (converted: %.2f)"),
          vars.slave.dhw.currentTemp2, convertedDhwTemp2
        );

        Sensors::setValueByType(
          Sensors::Type::OT_DHW_TEMP2, convertedDhwTemp2,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT_DHW), F("Failed receive temp 2"));
      }
    }

    // Update DHW flow rate
    if (settings.opentherm.options.dhwSupport && Sensors::getAmountByType(Sensors::Type::OT_DHW_FLOW_RATE, true)) {
      if (this->updateDhwFlowRate()) {
        const float convertedDhwFlowRate = convertVolume(
          vars.slave.dhw.flowRate,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT_DHW), F("Received flow rate: %.2f (converted: %.2f)"),
          vars.slave.dhw.flowRate, convertedDhwFlowRate
        );

        Sensors::setValueByType(
          Sensors::Type::OT_DHW_FLOW_RATE, convertedDhwFlowRate,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT_DHW), F("Failed receive flow rate"));
      }
    }

    // Update heating temp
    if (Sensors::getAmountByType(Sensors::Type::OT_HEATING_TEMP, true)) {
      if (this->updateHeatingTemp()) {
        const float convertedHeatingTemp = convertTemp(
          vars.slave.heating.currentTemp,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT_HEATING), F("Received temp: %.2f"),
          vars.slave.heating.currentTemp, convertedHeatingTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_HEATING_TEMP, convertedHeatingTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT_HEATING), F("Failed receive temp"));
      }
    }

    // Update heating return temp
    if (Sensors::getAmountByType(Sensors::Type::OT_HEATING_RETURN_TEMP, true)) {
      if (this->updateHeatingReturnTemp()) {
        const float convertedHeatingReturnTemp = convertTemp(
          vars.slave.heating.returnTemp,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT_HEATING), F("Received return temp: %.2f (converted: %.2f)"),
          vars.slave.heating.returnTemp, convertedHeatingReturnTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_HEATING_RETURN_TEMP, convertedHeatingReturnTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT_HEATING), F("Failed receive return temp"));
      }
    }

    // Update CH2 temp
    if (Sensors::getAmountByType(Sensors::Type::OT_CH2_TEMP, true)) {
      if (vars.master.ch2.enabled && !settings.opentherm.options.nativeOTC) {
        if (this->updateCh2Temp()) {
          const float convertedCh2Temp = convertTemp(
            vars.slave.ch2.currentTemp,
            settings.opentherm.unitSystem,
            settings.system.unitSystem
          );

          Log.snoticeln(
            FPSTR(L_OT_CH2), F("Received temp: %.2f (converted: %.2f)"),
            vars.slave.ch2.currentTemp, convertedCh2Temp
          );

          Sensors::setValueByType(
            Sensors::Type::OT_CH2_TEMP, convertedCh2Temp,
            Sensors::ValueType::PRIMARY, true, true
          );

        } else {
          Log.swarningln(FPSTR(L_OT_CH2), F("Failed receive temp"));
        }
      }
    }

    // Update exhaust temp
    if (Sensors::getAmountByType(Sensors::Type::OT_EXHAUST_TEMP, true)) {
      if (this->updateExhaustTemp()) {
        const float convertedExhaustTemp = convertTemp(
          vars.slave.exhaust.temp,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT), F("Received exhaust temp: %.2f (converted: %.2f)"),
          vars.slave.exhaust.temp, convertedExhaustTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_EXHAUST_TEMP, convertedExhaustTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive exhaust temp"));
      }
    }

    // Update heat exchanger temp
    if (Sensors::getAmountByType(Sensors::Type::OT_HEAT_EXCHANGER_TEMP, true)) {
      if (this->updateHeatExchangerTemp()) {
        const float convertedHeatExchTemp = convertTemp(
          vars.slave.heatExchangerTemp,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT), F("Received heat exchanger temp: %.2f (converted: %.2f)"),
          vars.slave.heatExchangerTemp, convertedHeatExchTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_HEAT_EXCHANGER_TEMP, convertedHeatExchTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive heat exchanger temp"));
      }
    }

    // Update outdoor temp
    if (Sensors::getAmountByType(Sensors::Type::OT_OUTDOOR_TEMP, true)) {
      if (this->updateOutdoorTemp()) {
        const float convertedOutdoorTemp = convertTemp(
          vars.slave.heating.outdoorTemp,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT), F("Received outdoor temp: %.2f (converted: %.2f)"),
          vars.slave.heating.outdoorTemp, convertedOutdoorTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_OUTDOOR_TEMP, convertedOutdoorTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive outdoor temp"));
      }
    }
    
    // Update solar storage temp
    if (Sensors::getAmountByType(Sensors::Type::OT_SOLAR_STORAGE_TEMP, true)) {
      if (this->updateSolarStorageTemp()) {
        const float convertedSolarStorageTemp = convertTemp(
          vars.slave.solar.storage,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT), F("Received solar storage temp: %.2f (converted: %.2f)"),
          vars.slave.solar.storage, convertedSolarStorageTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_SOLAR_STORAGE_TEMP, convertedSolarStorageTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive solar storage temp"));
      }
    }

    // Update solar collector temp
    if (Sensors::getAmountByType(Sensors::Type::OT_SOLAR_COLLECTOR_TEMP, true)) {
      if (this->updateSolarCollectorTemp()) {
        const float convertedSolarCollectorTemp = convertTemp(
          vars.slave.solar.collector,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT), F("Received solar collector temp: %.2f (converted: %.2f)"),
          vars.slave.solar.collector, convertedSolarCollectorTemp
        );

        Sensors::setValueByType(
          Sensors::Type::OT_SOLAR_COLLECTOR_TEMP, convertedSolarCollectorTemp,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive solar collector temp"));
      }
    }

    // Update fan speed
    if (
      Sensors::getAmountByType(Sensors::Type::OT_FAN_SPEED_SETPOINT, true) ||
      Sensors::getAmountByType(Sensors::Type::OT_FAN_SPEED_CURRENT, true)
    ) {
      if (this->updateFanSpeed()) {
        Log.snoticeln(
          FPSTR(L_OT), F("Received fan speed, setpoint: %hhu%%, current: %hhu%%"),
          vars.slave.fanSpeed.setpoint, vars.slave.fanSpeed.current
        );

        Sensors::setValueByType(
          Sensors::Type::OT_FAN_SPEED_SETPOINT, vars.slave.fanSpeed.setpoint,
          Sensors::ValueType::PRIMARY, true, true
        );
        Sensors::setValueByType(
          Sensors::Type::OT_FAN_SPEED_CURRENT, vars.slave.fanSpeed.current,
          Sensors::ValueType::PRIMARY, true, true
        );
      }
    }

    // Update pressure
    if (Sensors::getAmountByType(Sensors::Type::OT_PRESSURE, true)) {
      if (this->updatePressure()) {
        const float convertedPressure = convertPressure(
          vars.slave.pressure,
          settings.opentherm.unitSystem,
          settings.system.unitSystem
        );

        Log.snoticeln(
          FPSTR(L_OT), F("Received pressure: %.2f (converted: %.2f)"),
          vars.slave.pressure, convertedPressure
        );

        Sensors::setValueByType(
          Sensors::Type::OT_PRESSURE, convertedPressure,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive pressure"));
      }
    }

    // Update exhaust CO2
    if (Sensors::getAmountByType(Sensors::Type::OT_EXHAUST_CO2, true)) {
      if (this->updateExhaustCo2()) {
        Log.snoticeln(
          FPSTR(L_OT), F("Received exhaust CO2: %hu ppm"),
          vars.slave.exhaust.co2
        );

        Sensors::setValueByType(
          Sensors::Type::OT_EXHAUST_CO2, vars.slave.exhaust.co2,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive exhaust CO2"));
      }
    }

    // Update exhaust fan speed
    if (Sensors::getAmountByType(Sensors::Type::OT_EXHAUST_FAN_SPEED, true)) {
      if (this->updateExhaustFanSpeed()) {
        Log.snoticeln(
          FPSTR(L_OT), F("Received exhaust fan speed: %hu rpm"),
          vars.slave.exhaust.fanSpeed
        );

        Sensors::setValueByType(
          Sensors::Type::OT_EXHAUST_FAN_SPEED, vars.slave.exhaust.fanSpeed,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive exhaust fan speed"));
      }
    }

    // Update supply fan speed
    if (Sensors::getAmountByType(Sensors::Type::OT_SUPPLY_FAN_SPEED, true)) {
      if (this->updateSupplyFanSpeed()) {
        Log.snoticeln(
          FPSTR(L_OT), F("Received supply fan speed: %hu rpm"),
          vars.slave.fanSpeed.supply
        );

        Sensors::setValueByType(
          Sensors::Type::OT_SUPPLY_FAN_SPEED, vars.slave.fanSpeed.supply,
          Sensors::ValueType::PRIMARY, true, true
        );

      } else {
        Log.swarningln(FPSTR(L_OT), F("Failed receive supply fan speed"));
      }
    }

    // Fault reset action
    if (vars.actions.resetFault) {
      if (vars.slave.fault.active) {
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
      if (vars.slave.diag.active) {
        if (this->instance->sendServiceReset()) {
          Log.sinfoln(FPSTR(L_OT), F("Boiler diagnostic reset successfully"));
          
        } else {
          Log.serrorln(FPSTR(L_OT), F("Boiler diagnostic reset failed"));
        }
      }

      vars.actions.resetDiagnostic = false;
    }


    // Update DHW temp
    if (vars.master.dhw.enabled) {
      // Target dhw temp
      const float& targetTemp = vars.master.dhw.targetTemp;

      // Converted target dhw temp
      const float convertedTemp = convertTemp(
        targetTemp,
        settings.system.unitSystem,
        settings.opentherm.unitSystem
      );

      // Set DHW temp
      if (this->needSetDhwTemp(convertedTemp)) {
        if (this->setDhwTemp(convertedTemp)) {
          this->dhwSetTempTime = millis();

          Log.sinfoln(
            FPSTR(L_OT_DHW), F("Set temp: %.2f (converted: %.2f, response: %.2f)"),
            targetTemp, convertedTemp, vars.slave.dhw.targetTemp
          );

        } else {
          Log.swarningln(FPSTR(L_OT_DHW), F("Failed set temp"));
        }
      }
    }

    // Send indoor temp if AlwaysSendIndoorTemp option is enabled.
    if (settings.opentherm.options.nativeOTC || settings.opentherm.options.alwaysSendIndoorTemp) {
      // Current indoor temp
      const float& indoorTemp = vars.master.heating.indoorTemp;

      // Converted current indoor temp
      const float convertedTemp = convertTemp(indoorTemp, settings.system.unitSystem, settings.opentherm.unitSystem);

      // Set current indoor temp
      if (this->setRoomTemp(convertedTemp)) {
        Log.sinfoln(
          FPSTR(L_OT_HEATING), F("Set current indoor temp: %.2f (converted: %.2f, response: %.2f)"),
          indoorTemp, convertedTemp, vars.slave.heating.indoorTemp
        );
        
      } else {
        Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set current indoor temp"));
      }

      // Set current CH2 indoor temp
      if (settings.opentherm.options.heatingToCh2) {
        if (this->setRoomTempCh2(convertedTemp)) {
          Log.sinfoln(
            FPSTR(L_OT_HEATING), F("Set current CH2 indoor temp: %.2f (converted: %.2f, response: %.2f)"),
            indoorTemp, convertedTemp, vars.slave.ch2.indoorTemp
          );

        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set current CH2 indoor temp"));
        }
      }
    }

    // NativeOTC
    if (settings.opentherm.options.nativeOTC) {
      // Target indoor temp
      const float& targetTemp = vars.master.heating.targetTemp;

      // Converted target indoor temp
      const float convertedTemp = convertTemp(
        targetTemp,
        settings.system.unitSystem,
        settings.opentherm.unitSystem
      );

      // Set target indoor temp
      if (this->needSetHeatingTemp(convertedTemp)) {
        if (this->setRoomSetpoint(convertedTemp)) {
          this->heatingSetTempTime = millis();

          Log.sinfoln(
            FPSTR(L_OT_HEATING), F("Set target indoor temp: %.2f (converted: %.2f, response: %.2f)"),
            targetTemp, convertedTemp, vars.slave.heating.targetTemp
          );

        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set target indoor temp"));
        }
      }

      // Set target CH2 temp
      if (settings.opentherm.options.heatingToCh2 && this->needSetCh2Temp(convertedTemp)) {
        if (this->setRoomSetpointCh2(convertedTemp)) {
          this->ch2SetTempTime = millis();

          Log.sinfoln(
            FPSTR(L_OT_HEATING), F("Set target CH2 indoor temp: %.2f (converted: %.2f, response: %.2f)"),
            targetTemp, convertedTemp, vars.slave.ch2.targetTemp
          );

        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set target CH2 indoor temp"));
        }
      }
    }

    // Set heating temp
    {
      // Target heating temp
      float targetTemp = 0.0f;
      if (vars.master.heating.enabled) {
        targetTemp = !settings.opentherm.options.nativeOTC
          ? vars.master.heating.setpointTemp
          : vars.master.heating.targetTemp;
      }

      // Converted target heating temp
      const float convertedTemp = convertTemp(
        targetTemp,
        settings.system.unitSystem,
        settings.opentherm.unitSystem
      );

      if (this->needSetHeatingTemp(convertedTemp)) {
        // Set max heating temp
        if (settings.opentherm.options.maxTempSyncWithTargetTemp) {
          if (this->setMaxHeatingTemp(convertedTemp)) {
            Log.sinfoln(
              FPSTR(L_OT_HEATING), F("Set max heating temp: %.2f (converted: %.2f)"),
              targetTemp, convertedTemp
            );

          } else {
            Log.swarningln(
              FPSTR(L_OT_HEATING), F("Failed set max heating temp: %.2f (converted: %.2f)"),
              targetTemp, convertedTemp
            );
          }
        }

        // Set target heating temp
        if (this->setHeatingTemp(convertedTemp)) {
          this->heatingSetTempTime = millis();

          Log.sinfoln(
            FPSTR(L_OT_HEATING), F("Set target temp: %.2f (converted: %.2f, response: %.2f)"),
            targetTemp, convertedTemp, vars.slave.heating.targetTemp
          );

        } else {
          Log.swarningln(FPSTR(L_OT_HEATING), F("Failed set target temp"));
        }
      }
    }

    // Set CH2 temp
    if (settings.opentherm.options.heatingToCh2 || settings.opentherm.options.dhwToCh2) {
      // Target CH2 heating temp
      const float targetTemp = vars.master.ch2.enabled
        ? vars.master.ch2.targetTemp
        : 0.0f;

      // Converted target CH2 temp
      const float convertedTemp = convertTemp(
        targetTemp,
        settings.system.unitSystem,
        settings.opentherm.unitSystem
      );

      if (this->needSetCh2Temp(convertedTemp)) {
        if (this->setCh2Temp(convertedTemp)) {
          this->ch2SetTempTime = millis();

          Log.sinfoln(
            FPSTR(L_OT_CH2), F("Set temp: %.2f (converted: %.2f, response: %.2f)"),
            targetTemp, convertedTemp, vars.slave.ch2.targetTemp
          );

        } else {
          Log.swarningln(FPSTR(L_OT_CH2), F("Failed set temp"));
        }
      }
    }


    // Heating overheat control
    if (settings.heating.overheatProtection.highTemp > 0 && settings.heating.overheatProtection.lowTemp > 0) {
      const float highTemp = convertTemp(
        max({
          vars.slave.heating.currentTemp,
          vars.slave.heating.returnTemp,
          vars.slave.heatExchangerTemp
        }),
        settings.opentherm.unitSystem,
        settings.system.unitSystem
      );

      if (vars.master.heating.overheat) {
        if ((float) settings.heating.overheatProtection.lowTemp - highTemp + 0.0001f >= 0.0f) {
          vars.master.heating.overheat = false;

          Log.sinfoln(
            FPSTR(L_OT_HEATING), F("Overheating not detected. Current high temp: %.2f, threshold (low): %hhu"),
            highTemp, settings.heating.overheatProtection.lowTemp
          );
        }

      } else if (vars.slave.heating.active) {
        if (highTemp - (float) settings.heating.overheatProtection.highTemp + 0.0001f >= 0.0f) {
          vars.master.heating.overheat = true;

          Log.swarningln(
            FPSTR(L_OT_HEATING), F("Overheating detected! Current high temp: %.2f, threshold (high): %hhu"),
            highTemp, settings.heating.overheatProtection.highTemp
          );
        }
      }

    } else if (vars.master.heating.overheat) {
      vars.master.heating.overheat = false;
    }

    // DHW overheat control
    if (settings.dhw.overheatProtection.highTemp > 0 && settings.dhw.overheatProtection.lowTemp > 0) {
      const float highTemp = convertTemp(
        max({
          vars.slave.heating.currentTemp,
          vars.slave.heating.returnTemp,
          vars.slave.heatExchangerTemp,
          vars.slave.dhw.currentTemp,
          vars.slave.dhw.currentTemp2,
          vars.slave.dhw.returnTemp
        }),
        settings.opentherm.unitSystem,
        settings.system.unitSystem
      );

      if (vars.master.dhw.overheat) {
        if ((float) settings.dhw.overheatProtection.lowTemp - highTemp + 0.0001f >= 0.0f) {
          vars.master.dhw.overheat = false;

          Log.sinfoln(
            FPSTR(L_OT_DHW), F("Overheating not detected. Current high temp: %.2f, threshold (low): %hhu"),
            highTemp, settings.dhw.overheatProtection.lowTemp
          );
        }

      } else if (vars.slave.dhw.active) {
        if (highTemp - (float) settings.dhw.overheatProtection.highTemp + 0.0001f >= 0.0f) {
          vars.master.dhw.overheat = true;

          Log.swarningln(
            FPSTR(L_OT_DHW), F("Overheating detected! Current high temp: %.2f, threshold (high): %hhu"),
            highTemp, settings.dhw.overheatProtection.highTemp
          );
        }
      }

    } else if (vars.master.dhw.overheat) {
      vars.master.dhw.overheat = false;
    }
  }

  void initialize() {
    // Not all boilers support these, only try once when the boiler becomes connected
    if (this->updateSlaveVersion()) {
      Log.snoticeln(
        FPSTR(L_OT), F("Received slave app version: %u, type: %u"),
        vars.slave.appVersion, vars.slave.type
      );

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed receive slave version"));
    }

    if (this->setMasterVersion(vars.master.appVersion, vars.master.type)) {
      Log.snoticeln(
        FPSTR(L_OT), F("Set master version: %u, type: %u"), 
        vars.master.appVersion, vars.master.type
      );
      
    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed set master version"));
    }

    if (this->updateSlaveOtVersion()) {
      Log.snoticeln(FPSTR(L_OT), F("Received slave OT version: %f"), vars.slave.protocolVersion);

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed receive slave OT version"));
    }

    if (this->setMasterOtVersion(vars.master.protocolVersion)) {
      Log.snoticeln(FPSTR(L_OT), F("Set master OT version: %f"), vars.master.protocolVersion);

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed set master OT version"));
    }

    if (this->updateSlaveConfig()) {
      Log.snoticeln(
        FPSTR(L_OT), F("Received slave member id: %u, flags: %u"),
        vars.slave.memberId, vars.slave.flags
      );

    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed receive slave config"));
    }

    if (this->setMasterConfig(vars.master.memberId, vars.master.flags)) {
      Log.snoticeln(
        FPSTR(L_OT), F("Set master member id: %u, flags: %u"),
        vars.master.memberId, vars.master.flags
      );
      
    } else {
      Log.swarningln(FPSTR(L_OT), F("Failed set master config"));
    }

    /*char buf[100];
    if (this->instance->getStr(OpenThermMessageID::Brand, buf, sizeof(buf) - 1)) {
      Log.snoticeln(FPSTR(L_OT), F("Slave brand: %s"), buf);
    }

    if (this->instance->getStr(OpenThermMessageID::BrandVersion, buf, sizeof(buf) - 1)) {
      Log.snoticeln(FPSTR(L_OT), F("Slave brand version: %s"), buf);
    }

    if (this->instance->getStr(OpenThermMessageID::BrandSerialNumber, buf, sizeof(buf) - 1)) {
      Log.snoticeln(FPSTR(L_OT), F("Slave brand s/n: %s"), buf);
    }*/
  }

  bool isReady() {
    return millis() - this->instanceCreatedTime > this->readyTime;
  }

  bool needSetDhwTemp(const float target) {
    return millis() - this->dhwSetTempTime > this->dhwSetTempInterval
      || fabsf(target - vars.slave.dhw.targetTemp) > 0.05f;
  }

  bool needSetHeatingTemp(const float target) {
    return millis() - this->heatingSetTempTime > this->heatingSetTempInterval
      || fabsf(target - vars.slave.heating.targetTemp) > 0.05f;
  }

  bool needSetCh2Temp(const float target) {
    return millis() - this->ch2SetTempTime > this->ch2SetTempInterval
      || fabsf(target - vars.slave.ch2.targetTemp) > 0.05f;
  }

  bool updateSlaveConfig() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::SConfigSMemberIDcode,
      0
    ));
    
    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::SConfigSMemberIDcode)) {
      return false;
    }

    vars.slave.memberId = response & 0xFF;
    vars.slave.flags = (response & 0xFFFF) >> 8;

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

  bool setYear(const struct tm *ptm) {
    const unsigned int request = (ptm->tm_year + 1900) & 0xFFFF;
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::Year,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Year)) {
      return false;
    }

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setDayAndMonth(const struct tm *ptm) {
    const uint8_t month = (ptm->tm_mon + 1) & 0xFF;
    const unsigned int request = (month << 8) | (ptm->tm_mday & 0xFF);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::Date,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Date)) {
      return false;
    }

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setTime(const struct tm *ptm) {
    const uint8_t dayOfWeek = ptm->tm_wday == 0 ? 6 : ptm->tm_wday - 1;
    const unsigned int request = ((dayOfWeek & 0x07) << 13)
      | ((ptm->tm_hour & 0x1F) << 8)
      | (ptm->tm_min & 0x3F);
    
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::DayTime,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::DayTime)) {
      return false;
    }

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setCoolingSetpoint(const uint8_t value) {
    const unsigned int request = CustomOpenTherm::toFloat(value);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::CoolingControl,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::CoolingControl)) {
      return false;
    }

    vars.slave.cooling.setpoint = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setMaxModulationLevel(const uint8_t value) {
    const unsigned int request = CustomOpenTherm::toFloat(value);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::MaxRelModLevelSetting,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::MaxRelModLevelSetting)) {
      return false;
    }

    vars.slave.modulation.max = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setDhwTemp(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::TdhwSet,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TdhwSet)) {
      return false;
    }

    vars.slave.dhw.targetTemp = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }


  bool setRoomTemp(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::Tr,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Tr)) {
      return false;
    }

    vars.slave.heating.indoorTemp = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setRoomTempCh2(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::TrCH2,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TrCH2)) {
      return false;
    }

    vars.slave.ch2.indoorTemp = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setRoomSetpoint(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::TrSet,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TrSet)) {
      return false;
    }

    vars.slave.heating.targetTemp = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setRoomSetpointCh2(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::TrSetCH2,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TrSetCH2)) {
      return false;
    }

    vars.slave.ch2.targetTemp = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setMaxHeatingTemp(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::MaxTSet,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::MaxTSet)) {
      return false;
    }

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setHeatingTemp(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::TSet,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TSet)) {
      return false;
    }

    vars.slave.heating.targetTemp = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setCh2Temp(const float temperature) {
    const unsigned int request = CustomOpenTherm::temperatureToData(temperature);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::TsetCH2,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TsetCH2)) {
      return false;
    }

    vars.slave.ch2.targetTemp = CustomOpenTherm::getFloat(response);

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool setMasterVersion(const uint8_t version, const uint8_t type) {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::MasterVersion,
      (unsigned int) version | (unsigned int) type << 8
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::MasterVersion)) {
      return false;
    }

    uint8_t rVersion = response & 0xFF;
    uint8_t rType = (response & 0xFFFF) >> 8;

    return rVersion == version && rType == type;
  }

  bool setMasterOtVersion(const float version) {
    const unsigned int request = CustomOpenTherm::toFloat(version);
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::OpenThermVersionMaster,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::OpenThermVersionMaster)) {
      return false;
    }

    return CustomOpenTherm::getUInt(response) == request;
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
  bool setMasterConfig(const uint8_t id, const uint8_t flags, const bool force = false) {
    const uint8_t rMemberId = (force || id > 0) ? id : vars.slave.memberId;
    const uint8_t rFlags = (force || flags > 0) ? flags : vars.slave.flags;
    const unsigned int request = (unsigned int) rMemberId | (unsigned int) rFlags << 8;

    // if empty request
    if (!request) {
      return true;
    }

    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::MConfigMMemberIDcode,
      request
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::MConfigMMemberIDcode)) {
      return false;
    }

    //uint8_t rMemberId = response & 0xFF;
    //uint8_t rFlags = (response & 0xFFFF) >> 8;

    return CustomOpenTherm::getUInt(response) == request;
  }

  bool updateSlaveOtVersion() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::OpenThermVersionSlave,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::OpenThermVersionSlave)) {
      return false;
    }

    vars.slave.protocolVersion = CustomOpenTherm::getFloat(response);

    return true;
  }

  bool updateSlaveVersion() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::SlaveVersion,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::SlaveVersion)) {
      return false;
    }

    vars.slave.appVersion = response & 0xFF;
    vars.slave.type = (response & 0xFFFF) >> 8;

    return true;
  }

  bool updateMinModulationLevel() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::MaxCapacityMinModLevel,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::MaxCapacityMinModLevel)) {
      return false;
    }

    vars.slave.modulation.min = response & 0xFF;
    vars.slave.power.max = (response & 0xFFFF) >> 8;
    vars.slave.power.min = vars.slave.modulation.min > 0 && vars.slave.power.max > 0.1f
      ? (vars.slave.modulation.min * 0.01f) * vars.slave.power.max
      : 0.0f;

    return true;
  }

  bool updateMinMaxDhwTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::TdhwSetUBTdhwSetLB,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TdhwSetUBTdhwSetLB)) {
      return false;
    }

    uint8_t minTemp = response & 0xFF;
    uint8_t maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.slave.dhw.minTemp = minTemp;
      vars.slave.dhw.maxTemp = maxTemp;

      return true;
    }

    return false;
  }

  bool updateMinMaxHeatingTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::MaxTSetUBMaxTSetLB,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::MaxTSetUBMaxTSetLB)) {
      return false;
    }

    uint8_t minTemp = response & 0xFF;
    uint8_t maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.slave.heating.minTemp = minTemp;
      vars.slave.heating.maxTemp = maxTemp;

      return true;
    }

    return false;
  }

  bool updateFaultCode() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::ASFflags,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::ASFflags)) {
      return false;
    }

    vars.slave.fault.code = response & 0xFF;

    return true;
  }

  bool updateDiagCode() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::OEMDiagnosticCode,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::OEMDiagnosticCode)) {
      return false;
    }

    vars.slave.diag.code = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateBurnerStarts() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::SuccessfulBurnerStarts,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::SuccessfulBurnerStarts)) {
      return false;
    }

    vars.slave.stats.burnerStarts = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateDhwBurnerStarts() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::DHWBurnerStarts,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::DHWBurnerStarts)) {
      return false;
    }

    vars.slave.stats.dhwBurnerStarts = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateHeatingPumpStarts() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::CHPumpStarts,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::CHPumpStarts)) {
      return false;
    }

    vars.slave.stats.heatingPumpStarts = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateDhwPumpStarts() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::DHWPumpValveStarts,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::DHWPumpValveStarts)) {
      return false;
    }

    vars.slave.stats.dhwPumpStarts = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateBurnerHours() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::BurnerOperationHours,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::BurnerOperationHours)) {
      return false;
    }

    vars.slave.stats.burnerHours = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateDhwBurnerHours() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::DHWBurnerOperationHours,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::DHWBurnerOperationHours)) {
      return false;
    }

    vars.slave.stats.dhwBurnerHours = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateHeatingPumpHours() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::CHPumpOperationHours,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::CHPumpOperationHours)) {
      return false;
    }

    vars.slave.stats.heatingPumpHours = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateDhwPumpHours() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::DHWPumpValveOperationHours,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::DHWPumpValveOperationHours)) {
      return false;
    }

    vars.slave.stats.dhwPumpHours = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateCoolingHours() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::CoolingOperationHours,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::CoolingOperationHours)) {
      return false;
    }

    vars.slave.stats.coolingHours = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateModulationLevel() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::RelModLevel,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::RelModLevel)) {
      return false;
    }

    const float value = CustomOpenTherm::getFloat(response);
    if (value < 0) {
      return false;
    }

    vars.slave.modulation.current = value;

    return true;
  }

  bool updateDhwTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tdhw,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Tdhw)) {
      return false;
    }

    const float value = CustomOpenTherm::getFloat(response);
    if (value <= 0) {
      return false;
    }

    vars.slave.dhw.currentTemp = value;

    return true;
  }

  bool updateDhwTemp2() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tdhw2,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Tdhw2)) {
      return false;
    }

    const float value = CustomOpenTherm::getFloat(response);
    if (value <= 0) {
      return false;
    }

    vars.slave.dhw.currentTemp2 = value;

    return true;
  }

  bool updateDhwFlowRate() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::DHWFlowRate,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::DHWFlowRate)) {
      return false;
    }

    float value = CustomOpenTherm::getFloat(response);
    if (value < 0) {
      return false;
    }

    // no minuscule values
    // some boilers send a response of 0.06 when there is no flow
    if (value < 0.1f) {
      value = 0.0f;
    }

    vars.slave.dhw.flowRate = value;
    
    return true;
  }

  bool updateHeatingTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tboiler,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Tboiler)) {
      return false;
    }

    const float value = CustomOpenTherm::getFloat(response);
    if (value <= 0) {
      return false;
    }

    vars.slave.heating.currentTemp = value;

    return true;
  }

  bool updateHeatingReturnTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tret,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Tret)) {
      return false;
    }

    vars.slave.heating.returnTemp = CustomOpenTherm::getFloat(response);
    
    return true;
  }

  bool updateCh2Temp() {
    unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::TflowCH2,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TflowCH2)) {
      return false;
    }

    vars.slave.ch2.currentTemp = CustomOpenTherm::getFloat(response);

    return true;
  }



  bool updateExhaustTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::Texhaust,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Texhaust)) {
      return false;
    }

    const float value = (float) CustomOpenTherm::getInt(response);
    if (!isValidTemp(value, settings.opentherm.unitSystem, -40, 500)) {
      return false;
    }

    vars.slave.exhaust.temp = value;

    return true;
  }

  bool updateHeatExchangerTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::TboilerHeatExchanger,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::TboilerHeatExchanger)) {
      return false;
    }

    const float value = (float) CustomOpenTherm::getInt(response);
    if (value <= 0) {
      return false;
    }

    vars.slave.heatExchangerTemp = value;

    return true;
  }

  bool updateOutdoorTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::Toutside,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Toutside)) {
      return false;
    }

    vars.slave.heating.outdoorTemp = CustomOpenTherm::getFloat(response);

    return true;
  }

  bool updateSolarStorageTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tstorage,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Tstorage)) {
      return false;
    }

    vars.slave.solar.storage = CustomOpenTherm::getFloat(response);
    
    return true;
  }

  bool updateSolarCollectorTemp() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Tcollector,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::Tcollector)) {
      return false;
    }

    vars.slave.solar.collector = CustomOpenTherm::getFloat(response);
    
    return true;
  }

  bool updateFanSpeed() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::BoilerFanSpeedSetpointAndActual,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::BoilerFanSpeedSetpointAndActual)) {
      return false;
    }

    vars.slave.fanSpeed.setpoint = (response & 0xFFFF) >> 8;
    vars.slave.fanSpeed.current = response & 0xFF;

    return true;
  }

  bool updatePressure() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::CHPressure,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::CHPressure)) {
      return false;
    }

    const float value = CustomOpenTherm::getFloat(response);
    if (value < 0) {
      return false;
    }

    vars.slave.pressure = value;

    return true;
  }

  bool updateExhaustCo2() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::CO2exhaust,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::CO2exhaust)) {
      return false;
    }

    vars.slave.exhaust.co2 = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateExhaustFanSpeed() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::RPMexhaust,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::RPMexhaust)) {
      return false;
    }

    vars.slave.exhaust.fanSpeed = CustomOpenTherm::getUInt(response);

    return true;
  }

  bool updateSupplyFanSpeed() {
    const unsigned long response = this->instance->sendRequest(CustomOpenTherm::buildRequest(
      OpenThermRequestType::READ_DATA,
      OpenThermMessageID::RPMsupply,
      0
    ));

    if (!CustomOpenTherm::isValidResponse(response)) {
      return false;

    } else if (!CustomOpenTherm::isValidResponseId(response, OpenThermMessageID::RPMsupply)) {
      return false;
    }

    vars.slave.fanSpeed.supply = CustomOpenTherm::getUInt(response);

    return true;
  }
};
