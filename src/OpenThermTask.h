#include <new>
#include <CustomOpenTherm.h>

CustomOpenTherm* ot;

class OpenThermTask: public Task {
public:
  OpenThermTask(bool _enabled = false, unsigned long _interval = 0): Task(_enabled, _interval) {}

protected:
  void setup() {
    vars.parameters.heatingMinTemp = settings.heating.minTemp;
    vars.parameters.heatingMaxTemp = settings.heating.maxTemp;
    vars.parameters.dhwMinTemp = settings.dhw.minTemp;
    vars.parameters.dhwMaxTemp = settings.dhw.maxTemp;

    ot = new CustomOpenTherm(settings.opentherm.inPin, settings.opentherm.outPin);

    ot->begin(handleInterrupt, responseCallback);
    ot->setHandleSendRequestCallback(sendRequestCallback);

#ifdef LED_OT_RX_PIN
    pinMode(LED_OT_RX_PIN, OUTPUT);
#endif
  }

  void loop() {
    static byte currentHeatingTemp, currentDHWTemp = 0;
    unsigned long localResponse;

    if ( setMasterMemberIdCode() ) {
      DEBUG_F("Slave member id code: %u\r\n", vars.parameters.slaveMemberIdCode);
      DEBUG_F("Master member id code: %u\r\n", settings.opentherm.memberIdCode > 0 ? settings.opentherm.memberIdCode : vars.parameters.slaveMemberIdCode);

    } else {
      WARN("Slave member id failed");
    }

    bool heatingEnabled = (vars.states.emergency || settings.heating.enable) && pump && isReady();
    localResponse = ot->setBoilerStatus(
      heatingEnabled,
      settings.opentherm.dhwPresent && settings.dhw.enable,
      false, false, true, false, false
    );
    
    if (!ot->isValidResponse(localResponse)) {
      WARN_F("Invalid response after setBoilerStatus: %s\r\n", ot->statusToString(ot->getLastResponseStatus()));
      return;
    }

    if ( vars.parameters.heatingEnabled != heatingEnabled ) {
      vars.parameters.heatingEnabled = heatingEnabled;
      INFO_F("Heating enabled: %s\r\n", heatingEnabled ? "on\0" : "off\0");
    }

    vars.states.heating = ot->isCentralHeatingActive(localResponse);
    vars.states.dhw = settings.opentherm.dhwPresent ? ot->isHotWaterActive(localResponse) : false;
    vars.states.flame = ot->isFlameOn(localResponse);
    vars.states.fault = ot->isFault(localResponse);
    vars.states.diagnostic = ot->isDiagnostic(localResponse);

    setMaxModulationLevel(heatingEnabled ? 100 : 0);
    yield();

    // Команды чтения данных котла
    if (millis() - prevUpdateNonEssentialVars > 60000) {
      updateSlaveParameters();
      updateMasterParameters();

      DEBUG_F("Master type: %u, version: %u\r\n", vars.parameters.masterType, vars.parameters.masterVersion);
      DEBUG_F("Slave type: %u, version: %u\r\n", vars.parameters.slaveType, vars.parameters.slaveVersion);

      if ( settings.opentherm.dhwPresent ) {
        updateMinMaxDhwTemp();
      }
      updateMinMaxHeatingTemp();

      if (settings.sensors.outdoor.type == 0) {
        updateOutsideTemp();
      }
      if (vars.states.fault) {
        updateFaultCode();
        ot->sendBoilerReset();
      }

      if ( vars.states.diagnostic ) {
        ot->sendServiceReset();
      }

      prevUpdateNonEssentialVars = millis();
      yield();
    }

    updatePressure();
    if ((settings.opentherm.dhwPresent && settings.dhw.enable) || settings.heating.enable || heatingEnabled ) {
      updateModulationLevel();

    } else {
      vars.sensors.modulation = 0;
    }
    yield();

    if ( settings.opentherm.dhwPresent ) {
      updateDHWTemp();
    } else {
      vars.temperatures.dhw = 0;
    }

    //if ( settings.heating.enable || heatingEnabled ) {
      updateHeatingTemp();
    //} else {
    //  vars.temperatures.heating = 0;
    //}
    yield();

    //
    // Температура ГВС
    byte newDHWTemp = settings.dhw.target;
    if (settings.opentherm.dhwPresent && settings.dhw.enable && (needSetDhwTemp() || newDHWTemp != currentDHWTemp)) {
      if (newDHWTemp < vars.parameters.dhwMinTemp || newDHWTemp > vars.parameters.dhwMaxTemp) {
        newDHWTemp = constrain(newDHWTemp, vars.parameters.dhwMinTemp, vars.parameters.dhwMaxTemp);
      }

      INFO_F("Set DHW temp = %u\r\n", newDHWTemp);

      // Записываем заданную температуру ГВС
      if (ot->setDHWSetpoint(newDHWTemp)) {
        currentDHWTemp = newDHWTemp;
        dhwSetTempTime = millis();

      } else {
        WARN("Failed set DHW temp");
      }
    }

    //
    // Температура отопления
    if (heatingEnabled && (needSetHeatingTemp() || fabs(vars.parameters.heatingSetpoint - currentHeatingTemp) > 0.0001)) {
      INFO_F("Setting heating temp = %u \n", vars.parameters.heatingSetpoint);

      // Записываем заданную температуру
      if (ot->setBoilerTemperature(vars.parameters.heatingSetpoint)) {
        currentHeatingTemp = vars.parameters.heatingSetpoint;
        heatingSetTempTime = millis();

      } else {
        WARN("Failed set heating temp");
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

  void static IRAM_ATTR handleInterrupt() {
    ot->handleInterrupt();
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
    sprintf(buffer, "OT REQUEST ID: %4d   Request: %8lx   Response: %8lx   Attempt: %2d   Status: %s", id, request, response, attempt, ot->statusToString(status));
    if (status != OpenThermResponseStatus::SUCCESS) {
      //WARN(buffer);
      DEBUG(buffer);
    } else {
      DEBUG(buffer);
    }
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
      DEBUG_F(
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

    } else if ( settings.opentherm.memberIdCode <= 0 ) {
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
    // INFO_F("Opentherm version slave: %f\n", ot->getFloat(response));

    response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::WRITE_DATA, OpenThermMessageID::OpenThermVersionMaster, response));
    if (!ot->isValidResponse(response)) {
      return false;
    }
    // INFO_F("Opentherm version master: %f\n", ot->getFloat(response));

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
      vars.parameters.dhwMinTemp = minTemp < settings.dhw.minTemp ? settings.dhw.minTemp : minTemp;
      vars.parameters.dhwMaxTemp = maxTemp > settings.dhw.maxTemp ? settings.dhw.maxTemp : maxTemp;

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
      vars.parameters.heatingMinTemp = minTemp < settings.heating.minTemp ? settings.heating.minTemp : minTemp;
      vars.parameters.heatingMaxTemp = maxTemp > settings.heating.maxTemp ? settings.heating.maxTemp : maxTemp;

      return true;
    }

    return false;
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


  bool updateDHWTemp() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermMessageType::READ, OpenThermMessageID::Tdhw, 0));
    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.temperatures.dhw = ot->getFloat(response);
    return true;
  }

  bool updateFaultCode() {
    unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0));

    if (!ot->isValidResponse(response)) {
      return false;
    }

    vars.states.faultCode = response & 0xFF;
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
