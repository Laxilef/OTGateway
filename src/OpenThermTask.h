#include <new>
#include <CustomOpenTherm.h>

CustomOpenTherm* ot;

class OpenThermTask : public Task {
public:
  OpenThermTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

  void static IRAM_ATTR handleInterrupt() {
    ot->handleInterrupt();
  }

protected:
  const char* getTaskName() {
    return "OpenTherm";
  }
  
  int getTaskCore() {
    return 1;
  }

  void setup() {
    ot = new CustomOpenTherm(settings.opentherm.inPin, settings.opentherm.outPin);

    ot->setHandleSendRequestCallback(this->sendRequestCallback);
    ot->begin(OpenThermTask::handleInterrupt, this->responseCallback);

    ot->setYieldCallback([](void* self) {
      static_cast<OpenThermTask*>(self)->delay(10);
    }, this);

    #ifdef LED_OT_RX_PIN
      pinMode(LED_OT_RX_PIN, OUTPUT);
      digitalWrite(LED_OT_RX_PIN, false);
    #endif

    #ifdef HEATING_STATUS_PIN
      pinMode(HEATING_STATUS_PIN, OUTPUT);
      digitalWrite(HEATING_STATUS_PIN, false);
    #endif
  }

  void loop() {
    static byte currentHeatingTemp, currentDhwTemp = 0;
    unsigned long localResponse;

    if (setMasterMemberIdCode()) {
      Log.straceln("OT", "Slave member id code: %u", vars.parameters.slaveMemberIdCode);
      Log.straceln("OT", "Master member id code: %u", settings.opentherm.memberIdCode > 0 ? settings.opentherm.memberIdCode : vars.parameters.slaveMemberIdCode);

    } else {
      Log.swarningln("OT", "Slave member id failed");
    }

    bool heatingEnabled = (vars.states.emergency || settings.heating.enable) && pump && isReady();
    bool heatingCh2Enabled = settings.opentherm.heatingCh2Enabled;
    if (settings.opentherm.heatingCh1ToCh2) {
      heatingCh2Enabled = heatingEnabled;

    } else if (settings.opentherm.dhwToCh2) {
      heatingCh2Enabled = settings.opentherm.dhwPresent && settings.dhw.enable;
    }

    localResponse = ot->setBoilerStatus(
      heatingEnabled,
      settings.opentherm.dhwPresent && settings.dhw.enable,
      false,
      false,
      heatingCh2Enabled,
      settings.opentherm.summerWinterMode,
      false
    );

    if (!ot->isValidResponse(localResponse)) {
      Log.swarningln("OT", "Invalid response after setBoilerStatus: %s", ot->statusToString(ot->getLastResponseStatus()));
      return;
    }

    if (vars.parameters.heatingEnabled != heatingEnabled) {
      vars.parameters.heatingEnabled = heatingEnabled;
      Log.sinfoln("OT.HEATING", "%s", heatingEnabled ? "Enabled" : "Disabled");

      #ifdef HEATING_STATUS_PIN
      digitalWrite(HEATING_STATUS_PIN, heatingEnabled);
      #endif
    }

    vars.states.heating = ot->isCentralHeatingActive(localResponse);
    vars.states.dhw = settings.opentherm.dhwPresent ? ot->isHotWaterActive(localResponse) : false;
    vars.states.flame = ot->isFlameOn(localResponse);
    vars.states.fault = ot->isFault(localResponse);
    vars.states.diagnostic = ot->isDiagnostic(localResponse);

    setMaxModulationLevel(heatingEnabled ? settings.heating.maxModulation : 0);
    yield();

    // Команды чтения данных котла
    if (millis() - prevUpdateNonEssentialVars > 60000) {
      updateSlaveParameters();
      updateMasterParameters();

      Log.straceln("OT", "Master type: %u, version: %u", vars.parameters.masterType, vars.parameters.masterVersion);
      Log.straceln("OT", "Slave type: %u, version: %u", vars.parameters.slaveType, vars.parameters.slaveVersion);

      // DHW min/max temp
      if (settings.opentherm.dhwPresent) {
        if (updateMinMaxDhwTemp()) {
          if (settings.dhw.minTemp < vars.parameters.dhwMinTemp) {
            settings.dhw.minTemp = vars.parameters.dhwMinTemp;
            eeSettings.update();
            Log.snoticeln("OT.DHW", "Updated min temp: %d", settings.dhw.minTemp);
          }

          if (settings.dhw.maxTemp > vars.parameters.dhwMaxTemp) {
            settings.dhw.maxTemp = vars.parameters.dhwMaxTemp;
            eeSettings.update();
            Log.snoticeln("OT.DHW", "Updated max temp: %d", settings.dhw.maxTemp);
          }

        } else {
          Log.swarningln("OT.DHW", "Failed get min/max temp");
        }

        if (settings.dhw.minTemp >= settings.dhw.maxTemp) {
          settings.dhw.minTemp = 30;
          settings.dhw.maxTemp = 60;
          eeSettings.update();
        }
      }


      // Heating min/max temp
      if (updateMinMaxHeatingTemp()) {
        if (settings.heating.minTemp < vars.parameters.heatingMinTemp) {
          settings.heating.minTemp = vars.parameters.heatingMinTemp;
          eeSettings.update();
          Log.snoticeln("OT.HEATING", "Updated min temp: %d", settings.heating.minTemp);
        }

        if (settings.heating.maxTemp > vars.parameters.heatingMaxTemp) {
          settings.heating.maxTemp = vars.parameters.heatingMaxTemp;
          eeSettings.update();
          Log.snoticeln("OT.HEATING", "Updated max temp: %d", settings.heating.maxTemp);
        }

      } else {
        Log.swarningln("OT.HEATING", "Failed get min/max temp");
      }

      if (settings.heating.minTemp >= settings.heating.maxTemp) {
        settings.heating.minTemp = 20;
        settings.heating.maxTemp = 90;
        eeSettings.update();
      }

      // force
      setMaxHeatingTemp(settings.heating.maxTemp);

      if (settings.sensors.outdoor.type == 0) {
        updateOutsideTemp();
      }

      if (vars.states.fault) {
        updateFaultCode();
      }

      prevUpdateNonEssentialVars = millis();
      yield();
    }

    updatePressure();
    if ((settings.opentherm.dhwPresent && settings.dhw.enable) || settings.heating.enable || heatingEnabled) {
      updateModulationLevel();

    } else {
      vars.sensors.modulation = 0;
    }
    yield();

    if (settings.opentherm.dhwPresent) {
      updateDhwTemp();
      updateDhwFlowRate();
    } else {
      vars.temperatures.dhw = 0;
      vars.sensors.dhwFlowRate = 0.0f;
    }

    updateHeatingTemp();
    yield();

    // fault reset action
    if (vars.actions.resetFault) {
      if (vars.states.fault) {
        if (ot->sendBoilerReset()) {
          Log.sinfoln("OT", "Boiler fault reset successfully");

        } else {
          Log.serrorln("OT", "Boiler fault reset failed");
        }
      }

      vars.actions.resetFault = false;
      yield();
    }

    // diag reset action
    if (vars.actions.resetDiagnostic) {
      if (vars.states.diagnostic) {
        if (ot->sendServiceReset()) {
          Log.sinfoln("OT", "Boiler diagnostic reset successfully");
          
        } else {
          Log.serrorln("OT", "Boiler diagnostic reset failed");
        }
      }

      vars.actions.resetDiagnostic = false;
      yield();
    }

    //
    // Температура ГВС
    byte newDhwTemp = settings.dhw.target;
    if (settings.opentherm.dhwPresent && settings.dhw.enable && (needSetDhwTemp() || newDhwTemp != currentDhwTemp)) {
      if (newDhwTemp < settings.dhw.minTemp || newDhwTemp > settings.dhw.maxTemp) {
        newDhwTemp = constrain(newDhwTemp, settings.dhw.minTemp, settings.dhw.maxTemp);
      }

      Log.sinfoln("OT.DHW", "Set temp = %u", newDhwTemp);

      // Записываем заданную температуру ГВС
      if (ot->setDhwTemp(newDhwTemp)) {
        currentDhwTemp = newDhwTemp;
        dhwSetTempTime = millis();

      } else {
        Log.swarningln("OT.DHW", "Failed set temp");
      }

      if (settings.opentherm.dhwToCh2) {
        if (!ot->setHeatingCh2Temp(newDhwTemp)) {
          Log.swarningln("OT.DHW", "Failed set ch2 temp");
        }
      }
    }

    //
    // Температура отопления
    if (heatingEnabled && (needSetHeatingTemp() || fabs(vars.parameters.heatingSetpoint - currentHeatingTemp) > 0.0001)) {
      Log.sinfoln("OT.HEATING", "Set temp = %u", vars.parameters.heatingSetpoint);

      // Записываем заданную температуру
      if (ot->setHeatingCh1Temp(vars.parameters.heatingSetpoint)) {
        currentHeatingTemp = vars.parameters.heatingSetpoint;
        heatingSetTempTime = millis();

      } else {
        Log.swarningln("OT.HEATING", "Failed set temp");
      }

      if (settings.opentherm.heatingCh1ToCh2) {
        if (!ot->setHeatingCh2Temp(vars.parameters.heatingSetpoint)) {
          Log.swarningln("OT.HEATING", "Failed set ch2 temp");
        }
      }
    }

    // коммутационная разность (hysteresis)
    // только для pid и/или equitherm
    if (settings.heating.hysteresis > 0 && !vars.states.emergency && (settings.equitherm.enable || settings.pid.enable)) {
      float halfHyst = settings.heating.hysteresis / 2;
      if (pump && vars.temperatures.indoor - settings.heating.target + 0.0001 >= halfHyst) {
        pump = false;

      } else if (!pump && vars.temperatures.indoor - settings.heating.target - 0.0001 <= -(halfHyst)) {
        pump = true;
      }

    } else if (!pump) {
      pump = true;
    }
  }

  void static sendRequestCallback(unsigned long request, unsigned long response, OpenThermResponseStatus status, byte attempt) {
    printRequestDetail(ot->getDataID(request), status, request, response, attempt);
  }

  void static responseCallback(unsigned long result, OpenThermResponseStatus status) {
    static byte attempt = 0;

    switch (status) {
    case OpenThermResponseStatus::TIMEOUT:
      if (vars.states.otStatus && ++attempt > OPENTHERM_OFFLINE_TRESHOLD) {
        vars.states.otStatus = false;
        attempt = OPENTHERM_OFFLINE_TRESHOLD;
      }
      break;

    case OpenThermResponseStatus::SUCCESS:
      attempt = 0;
      if (!vars.states.otStatus) {
        vars.states.otStatus = true;
      }

      #ifdef LED_OT_RX_PIN
      {
        digitalWrite(LED_OT_RX_PIN, true);
        unsigned long ts = millis();
        while (millis() - ts < 2) {}
        digitalWrite(LED_OT_RX_PIN, false);
      }
      #endif
      break;

    default:
      break;
    }
  }

protected:
  unsigned short readyTime = 60000;
  unsigned short dhwSetTempInterval = 60000;
  unsigned short heatingSetTempInterval = 60000;

  bool pump = true;
  unsigned long prevUpdateNonEssentialVars = 0;
  unsigned long startupTime = millis();
  unsigned long dhwSetTempTime = 0;
  unsigned long heatingSetTempTime = 0;


  bool isReady() {
    return millis() - startupTime > readyTime;
  }

  bool needSetDhwTemp() {
    return millis() - dhwSetTempTime > dhwSetTempInterval;
  }

  bool needSetHeatingTemp() {
    return millis() - heatingSetTempTime > heatingSetTempInterval;
  }

  void static printRequestDetail(OpenThermMessageID id, OpenThermResponseStatus status, unsigned long request, unsigned long response, byte attempt) {
    Log.straceln("OT", "OT REQUEST ID: %4d   Request: %8lx   Response: %8lx   Attempt: %2d   Status: %s", id, request, response, attempt, ot->statusToString(status));
  }

  bool setMasterMemberIdCode() {
    //=======================================================================================
    // Эта группа элементов данных определяет информацию о конфигурации как на ведомых, так
    // и на главных сторонах. Каждый из них имеет группу флагов конфигурации (8 бит)
    // и код MemberID (1 байт). Перед передачей информации об управлении и состоянии
    // рекомендуется обмен сообщениями о допустимой конфигурации ведомого устройства
    // чтения и основной конфигурации записи. Нулевой код MemberID означает клиентское
    // неспецифическое устройство. Номер/тип версии продукта следует использовать в сочетании
    // с "кодом идентификатора участника", который идентифицирует производителя устройства.
    //=======================================================================================

    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SConfigSMemberIDcode, 0)); // 0xFFFF
    if (ot->isValidResponse(response)) {
      vars.parameters.slaveMemberIdCode = response & 0xFF;

      /*uint8_t flags = (response & 0xFFFF) >> 8;
      Log.strace(
        "OT",
        "MasterMemberIdCode:\r\n  DHW present: %u\r\n  Control type: %u\r\n  Cooling configuration: %u\r\n  DHW configuration: %u\r\n  Pump control: %u\r\n  CH2 present: %u\r\n  Remote water filling function: %u\r\n  Heat/cool mode control: %u\r\n  Slave MemberID Code: %u\r\n",
        flags & 0x01,
        flags & 0x02,
        flags & 0x04,
        flags & 0x08,
        flags & 0x10,
        flags & 0x20,
        flags & 0x40,
        flags & 0x80,
        response & 0xFF
      );*/

    } else if (settings.opentherm.memberIdCode <= 0) {
      return false;
    }

    response = ot->sendRequest(ot->buildRequest(
      OpenThermRequestType::WRITE,
      OpenThermMessageID::MConfigMMemberIDcode,
      settings.opentherm.memberIdCode > 0 ? settings.opentherm.memberIdCode : vars.parameters.slaveMemberIdCode
    ));

    return ot->isValidResponse(response);
  }

  bool setMaxModulationLevel(byte value) {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MaxRelModLevelSetting, (unsigned int)(value * 256)));

    return ot->isValidResponse(response);
  }

  bool setOpenThermVersionMaster() {
    unsigned long response;
    response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::OpenThermVersionSlave, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::WRITE_DATA, OpenThermMessageID::OpenThermVersionMaster, response));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    return true;
  }

  bool updateMasterParameters() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MasterVersion, 0x013F));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.parameters.masterType = (response & 0xFFFF) >> 8;
    vars.parameters.masterVersion = response & 0xFF;

    return true;
  }

  bool updateSlaveParameters() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SlaveVersion, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.parameters.slaveType = (response & 0xFFFF) >> 8;
    vars.parameters.slaveVersion = response & 0xFF;

    return true;
  }

  bool updateMinMaxDhwTemp() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSetUBTdhwSetLB, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    byte minTemp = response & 0xFF;
    byte maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.parameters.dhwMinTemp = minTemp;
      vars.parameters.dhwMaxTemp = maxTemp;

      return true;
    }

    return false;
  }

  bool updateMinMaxHeatingTemp() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxTSetUBMaxTSetLB, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    byte minTemp = response & 0xFF;
    byte maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.parameters.heatingMinTemp = minTemp;
      vars.parameters.heatingMaxTemp = maxTemp;
      return true;
    }

    return false;
  }

  bool setMaxHeatingTemp(byte value) {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::MaxTSet, ot->temperatureToData(value)));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    return true;
  }

  bool updateOutsideTemp() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.temperatures.outdoor = ot->getFloat(response) + settings.sensors.outdoor.offset;
    return true;
  }

  bool updateHeatingTemp() {
    unsigned long response = ot->sendRequest(ot->buildGetBoilerTemperatureRequest());
    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.temperatures.heating = ot->getFloat(response);
    return true;
  }


  bool updateDhwTemp() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermMessageType::READ, OpenThermMessageID::Tdhw, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.temperatures.dhw = ot->getFloat(response);
    return true;
  }

  bool updateDhwFlowRate() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermMessageType::READ, OpenThermMessageID::DHWFlowRate, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.sensors.dhwFlowRate = ot->getFloat(response);
    return true;
  }

  bool updateFaultCode() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0));

    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.sensors.faultCode = response & 0xFF;
    return true;
  }

  bool updateModulationLevel() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0));

    if (!ot->isValidResponse(response)) {
      return false;
    }

    float modulation = ot->f88(response);
    if (!vars.states.flame) {
      vars.sensors.modulation = 0;
    } else {
      vars.sensors.modulation = modulation;
    }

    return true;
  }

  bool updatePressure() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0));

    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.sensors.pressure = ot->getFloat(response);
    return true;
  }
};
