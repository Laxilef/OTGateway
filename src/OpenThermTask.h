#include "lib/CustomOpenTherm.h"

CustomOpenTherm ot(OPENTHERM_IN_PIN, OPENTHERM_OUT_PIN);

class OpenThermTask : public CustomTask {
public:
  OpenThermTask(bool enabled = false, unsigned long interval = 0) : CustomTask(enabled, interval) {}

protected:
  void setup() {
    ot.begin(handleInterrupt, responseCallback);
    ot.setHandleSendRequestCallback(sendRequestCallback);
  }

  void loop() {
    static byte currentHeatingTemp, currentDHWTemp = 0;
    byte newHeatingTemp, newDHWTemp = 0;
    unsigned long localResponse;

    setMasterMemberIdCode();
    DEBUG_F("Slave member id code: %u \n", vars.parameters.slaveMemberIdCode);

    localResponse = ot.setBoilerStatus(
      settings.heating.enable && pump,
      settings.dhw.enable
    );

    if (!ot.isValidResponse(localResponse)) {
      return;
    }

    vars.states.heating = ot.isCentralHeatingActive(localResponse);
    vars.states.dhw = ot.isHotWaterActive(localResponse);
    vars.states.flame = ot.isFlameOn(localResponse);
    vars.states.fault = ot.isFault(localResponse);
    vars.states.diagnostic = ot.isDiagnostic(localResponse);

    /*if (vars.dump_request.value)
    {
      testSupportedIDs();
      vars.dump_request.value = false;
    }*/



    /*if ( ot.isValidResponse(localResponse) ) {
      vars.SlaveMemberIDcode.value = localResponse >> 0 & 0xFF;
      uint8_t flags = (localResponse & 0xFFFF) >> 8 & 0xFF;
      vars.dhw_present.value = flags & 0x01;
      vars.control_type.value = flags & 0x02;
      vars.cooling_present.value = flags & 0x04;
      vars.dhw_tank_present.value = flags & 0x08;
      vars.pump_control_present.value = flags & 0x10;
      vars.ch2_present.value = flags & 0x20;
    }*/

    // Команды чтения данных котла
    if (millis() - prevUpdateNonEssentialVars > 30000) {
      updateSlaveParameters();
      updateMasterParameters();
      // crash?
      DEBUG_F("Master type: %u, version: %u \n", vars.parameters.masterType, vars.parameters.masterVersion);
      DEBUG_F("Slave type: %u, version: %u \n", vars.parameters.slaveType, vars.parameters.slaveVersion);

      updateMinMaxDhwTemp();
      updateMinMaxHeatingTemp();
      if (settings.outdoorTempSource == 0) {
        updateOutsideTemp();
      }
      if (vars.states.fault) {
        updateFaultCode();
      }
      updatePressure();

      prevUpdateNonEssentialVars = millis();
    }
    updateHeatingTemp();
    updateDHWTemp();
    updateModulationLevel();

    //
    // Температура ГВС
    newDHWTemp = settings.dhw.target;
    if (newDHWTemp != currentDHWTemp) {
      if (newDHWTemp < vars.parameters.dhwMinTemp || newDHWTemp > vars.parameters.dhwMaxTemp) {
        newDHWTemp = constrain(newDHWTemp, vars.parameters.dhwMinTemp, vars.parameters.dhwMaxTemp);
      }

      INFO_F("Set DHW temp = %u \n", newDHWTemp);

      // Записываем заданную температуру ГВС
      if (ot.setDHWSetpoint(newDHWTemp)) {
        currentDHWTemp = newDHWTemp;
      }
    }

    //
    // Температура отопления
    if (fabs(vars.parameters.heatingSetpoint - currentHeatingTemp) > 0.0001) {
      INFO_F("Set heating temp = %u \n", vars.parameters.heatingSetpoint);

      // Записываем заданную температуру
      if (ot.setBoilerTemperature(vars.parameters.heatingSetpoint)) {
        currentHeatingTemp = vars.parameters.heatingSetpoint;
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
    ot.handleInterrupt();
  }

  void static sendRequestCallback(unsigned long request, unsigned long response, OpenThermResponseStatus status, byte attempt) {
    printRequestDetail(ot.getDataID(request), status, request, response, attempt);
  }

  void static responseCallback(unsigned long result, OpenThermResponseStatus status) {
    static byte attempt = 0;
    switch (status) {
    case OpenThermResponseStatus::TIMEOUT:
      if (++attempt > OPENTHERM_OFFLINE_TRESHOLD) {
        vars.states.otStatus = false;
        attempt = OPENTHERM_OFFLINE_TRESHOLD;
      }
      break;
    case OpenThermResponseStatus::SUCCESS:
      attempt = 0;
      vars.states.otStatus = true;
      break;
    default:
      break;
    }
  }

protected:
  bool pump = true;
  unsigned long prevUpdateNonEssentialVars = 0;

  void static printRequestDetail(OpenThermMessageID id, OpenThermResponseStatus status, unsigned long request, unsigned long response, byte attempt) {
    sprintf(buffer, "OT REQUEST ID: %4d   Request: %8x   Response: %8x   Attempt: %2d   Status: %s", id, request, response, attempt, ot.statusToString(status));
    if (status != OpenThermResponseStatus::SUCCESS) {
      //WARN(buffer);
      DEBUG(buffer);
    } else {
      DEBUG(buffer);
    }
  }

  /*
    bool getBoilerTemp()
    {
      unsigned long response;
      return sendRequest(ot.buildGetBoilerTemperatureRequest(),response);
    }

    bool getDHWTemp()
    {
      unsigned long response;
      unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tdhw, 0);
      return sendRequest(request,response);
    }

    bool getOutsideTemp()
    {
      unsigned long response;
      unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0);
      return sendRequest(request,response);
    }

    bool setDHWTemp(float val)
    {
      unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, ot.temperatureToData(val));
      unsigned long response;
      return sendRequest(request,response);
    }

    bool getFaultCode()
    {
      unsigned long response;
      unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0);
      return sendRequest(request,response);
    }

    bool getModulationLevel() {
      unsigned long response;
      unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0);
      return sendRequest(request,response);
    }

    bool getPressure() {
      unsigned long response;
      unsigned long request = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0);
      return sendRequest(request,response);
    }

    bool sendRequest(unsigned long request, unsigned long& response)
    {
      send_newts = millis();
      if (send_newts - send_ts < 200) {
        // Преждем чем слать что то - надо подождать 100ms согласно специфиикации протокола ОТ
        delay(200 - (send_newts - send_ts));
      }

      bool result = ot.sendRequestAync(request);
      if(!result) {
        WARN("Не могу отправить запрос");
        WARN("Шина " + ot.isReady() ? "готова" : "не готова");
        return false;
      }
      while (!ot.isReady())
      {
        ot.process();
        yield(); // This is local Task yield() call which allow us to switch to another task in scheduler
      }
      send_ts = millis();
      response = ot_response;
      //printRequestDetail(ot.getDataID(request), request, response);

      return true; // Response is global variable
    }

    void testSupportedIDs()
    {
      // Basic
      unsigned long request;
      unsigned long response;
      OpenThermMessageID id;
      //Command
      id = OpenThermMessageID::Command;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);
      //ASFlags
      id = OpenThermMessageID::ASFflags;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //TrOverride
      id = OpenThermMessageID::TrOverride;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //TSP
      id = OpenThermMessageID::TSP;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //TSPindexTSPvalue
      id = OpenThermMessageID::TSPindexTSPvalue;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //FHBsize
      id = OpenThermMessageID::FHBsize;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //FHBindexFHBvalue
      id = OpenThermMessageID::FHBindexFHBvalue;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //MaxCapacityMinModLevel
      id = OpenThermMessageID::MaxCapacityMinModLevel;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //TrSet
      id = OpenThermMessageID::TrSet;
      request = ot.buildRequest(OpenThermRequestType::WRITE, id, ot.temperatureToData(21));
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //RelModLevel
      id = OpenThermMessageID::RelModLevel;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //CHPressure
      id = OpenThermMessageID::CHPressure;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //DHWFlowRate
      id = OpenThermMessageID::DHWFlowRate;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //DayTime
      id = OpenThermMessageID::DayTime;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //Date
      id = OpenThermMessageID::Date;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //Year
      id = OpenThermMessageID::Year;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //TrSetCH2
      id = OpenThermMessageID::TrSetCH2;
      request = ot.buildRequest(OpenThermRequestType::WRITE, id, ot.temperatureToData(21));
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //Tr
      id = OpenThermMessageID::Tr;
      request = ot.buildRequest(OpenThermRequestType::WRITE, id, ot.temperatureToData(21));
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //Tret
      id = OpenThermMessageID::Tret;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //Texhaust
      id = OpenThermMessageID::Texhaust;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //Hcratio
      id = OpenThermMessageID::Hcratio;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //RemoteOverrideFunction
      id = OpenThermMessageID::RemoteOverrideFunction;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //OEMDiagnosticCode
      id = OpenThermMessageID::OEMDiagnosticCode;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //BurnerStarts
      id = OpenThermMessageID::BurnerStarts;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //CHPumpStarts
      id = OpenThermMessageID::CHPumpStarts;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //DHWPumpValveStarts
      id = OpenThermMessageID::DHWPumpValveStarts;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //DHWBurnerStarts
      id = OpenThermMessageID::DHWBurnerStarts;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //BurnerOperationHours
      id = OpenThermMessageID::BurnerOperationHours;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //CHPumpOperationHours
      id = OpenThermMessageID::CHPumpOperationHours;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //DHWPumpValveOperationHours
      id = OpenThermMessageID::DHWPumpValveOperationHours;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);

      //DHWBurnerOperationHours
      id = OpenThermMessageID::DHWBurnerOperationHours;
      request = ot.buildRequest(OpenThermRequestType::READ, id, 0);
      if(sendRequest(request,response))
        printRequestDetail(id, ot.getLastResponseStatus(), request, response);
    }
  */

  void setMasterMemberIdCode() {
    //=======================================================================================
    // Эта группа элементов данных определяет информацию о конфигурации как на ведомых, так
    // и на главных сторонах. Каждый из них имеет группу флагов конфигурации (8 бит)
    // и код MemberID (1 байт). Перед передачей информации об управлении и состоянии
    // рекомендуется обмен сообщениями о допустимой конфигурации ведомого устройства
    // чтения и основной конфигурации записи. Нулевой код MemberID означает клиентское
    // неспецифическое устройство. Номер/тип версии продукта следует использовать в сочетании
    // с "кодом идентификатора участника", который идентифицирует производителя устройства.
    //=======================================================================================

    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SConfigSMemberIDcode, 0)); // 0xFFFF
    if (!ot.isValidResponse(response)) {
      return;
    }

    vars.parameters.slaveMemberIdCode = response >> 0 & 0xFF;
    ot.sendRequest(ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MConfigMMemberIDcode, vars.parameters.slaveMemberIdCode));
  }

  void updateMasterParameters() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MasterVersion, 0x013F));
    if (!ot.isValidResponse(response)) {
      return;
    }

    vars.parameters.masterType = (response & 0xFFFF) >> 8;
    vars.parameters.masterVersion = response & 0xFF;
  }

  void updateSlaveParameters() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SlaveVersion, 0));
    if (!ot.isValidResponse(response)) {
      return;
    }

    vars.parameters.slaveType = (response & 0xFFFF) >> 8;
    vars.parameters.slaveVersion = response & 0xFF;
  }

  void updateMinMaxDhwTemp() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSetUBTdhwSetLB, 0));

    if (!ot.isValidResponse(response)) {
      return;
    }

    byte minTemp = response & 0xFF;
    byte maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.parameters.dhwMinTemp = minTemp;
      vars.parameters.dhwMaxTemp = maxTemp;
    }
  }

  void updateMinMaxHeatingTemp() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxTSetUBMaxTSetLB, 0));

    if (!ot.isValidResponse(response)) {
      return;
    }

    byte minTemp = response & 0xFF;
    byte maxTemp = (response & 0xFFFF) >> 8;

    if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.parameters.heatingMinTemp = minTemp;
      vars.parameters.heatingMaxTemp = maxTemp;
    }
  }

  void updateOutsideTemp() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0));

    if (ot.isValidResponse(response)) {
      vars.temperatures.outdoor = ot.getFloat(response);
    }
  }

  void updateHeatingTemp() {
    unsigned long response = ot.sendRequest(ot.buildGetBoilerTemperatureRequest());

    if (ot.isValidResponse(response)) {
      vars.temperatures.heating = ot.getFloat(response);
    }
  }


  void updateDHWTemp() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Tdhw, 0));

    if (ot.isValidResponse(response)) {
      vars.temperatures.dhw = ot.getFloat(response);
    }
  }

  void updateFaultCode() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0));

    if (ot.isValidResponse(response)) {
      vars.states.faultCode = response & 0xFF;
    }
  }

  void updateModulationLevel() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0));

    if (ot.isValidResponse(response)) {
      vars.sensors.modulation = ot.getFloat(response);
    }
  }

  void updatePressure() {
    unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0));

    if (ot.isValidResponse(response)) {
      vars.sensors.pressure = ot.getFloat(response);
    }
  }
};
