#include <Equitherm.h>
#include <GyverPID.h>

Equitherm etRegulator;
GyverPID pidRegulator(0, 0, 0);


class RegulatorTask : public LeanTask {
public:
  RegulatorTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  float prevHeatingTarget = 0;
  float prevEtResult = 0;
  float prevPidResult = 0;

  #if defined(ARDUINO_ARCH_ESP32)
  const char* getTaskName() override {
    return "Regulator";
  }
  
  /*BaseType_t getTaskCore() override {
    return 1;
  }*/

  int getTaskPriority() override {
    return 4;
  }
  #endif
  
  void loop() {
    float newTemp = vars.parameters.heatingSetpoint;

    if (vars.states.emergency) {
      if (settings.heating.turbo) {
        settings.heating.turbo = false;

        Log.sinfoln(FPSTR(L_REGULATOR), F("Turbo mode auto disabled"));
      }

      newTemp = this->getEmergencyModeTemp();

    } else {
      if (settings.heating.turbo && (fabs(settings.heating.target - vars.temperatures.indoor) < 1 || !settings.heating.enable || (settings.equitherm.enable && settings.pid.enable))) {
        settings.heating.turbo = false;

        Log.sinfoln(FPSTR(L_REGULATOR), F("Turbo mode auto disabled"));
      }

      newTemp = this->getNormalModeTemp();
    }

    // Limits
    newTemp = constrain(
      newTemp,
      !settings.opentherm.nativeHeatingControl ? settings.heating.minTemp : THERMOSTAT_INDOOR_MIN_TEMP,
      !settings.opentherm.nativeHeatingControl ? settings.heating.maxTemp : THERMOSTAT_INDOOR_MAX_TEMP
    );

    if (fabs(vars.parameters.heatingSetpoint - newTemp) > 0.4999f) {
      vars.parameters.heatingSetpoint = newTemp;
    }
  }


  float getEmergencyModeTemp() {
    float newTemp = 0;

    // if use equitherm
    if (settings.emergency.useEquitherm) {
      float etResult = getEquithermTemp(settings.heating.minTemp, settings.heating.maxTemp);

      if (fabs(prevEtResult - etResult) > 0.4999f) {
        prevEtResult = etResult;
        newTemp += etResult;

        Log.sinfoln(FPSTR(L_REGULATOR_EQUITHERM), F("New emergency result: %.2f"), etResult);

      } else {
        newTemp += prevEtResult;
      }

    } else if(settings.emergency.usePid) {
      if (vars.parameters.heatingEnabled) {
        float pidResult = getPidTemp(
          settings.heating.minTemp,
          settings.heating.maxTemp
        );

        if (fabs(prevPidResult - pidResult) > 0.4999f) {
          prevPidResult = pidResult;
          newTemp += pidResult;

          Log.sinfoln(FPSTR(L_REGULATOR_PID), F("New emergency result: %.2f"), pidResult);

        } else {
          newTemp += prevPidResult;
        }

      } else if (!vars.parameters.heatingEnabled && prevPidResult != 0) {
        newTemp += prevPidResult;
      }

    } else {
      // default temp, manual mode
      newTemp = settings.emergency.target;
    }

    return newTemp;
  }

  float getNormalModeTemp() {
    float newTemp = 0;

    if (fabs(prevHeatingTarget - settings.heating.target) > 0.0001f) {
      prevHeatingTarget = settings.heating.target;
      Log.sinfoln(FPSTR(L_REGULATOR), F("New target: %.2f"), settings.heating.target);

      if (/*settings.equitherm.enable && */settings.pid.enable) {
        pidRegulator.integral = 0;
        Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
      }
    }

    // if use equitherm
    if (settings.equitherm.enable) {
      float etResult = getEquithermTemp(settings.heating.minTemp, settings.heating.maxTemp);

      if (fabs(prevEtResult - etResult) > 0.4999f) {
        prevEtResult = etResult;
        newTemp += etResult;

        Log.sinfoln(FPSTR(L_REGULATOR_EQUITHERM), F("New result: %.2f"), etResult);

      } else {
        newTemp += prevEtResult;
      }
    }

    // if use pid
    if (settings.pid.enable) {
      //if (vars.parameters.heatingEnabled) {
      if (settings.heating.enable) {
        float pidResult = getPidTemp(
          settings.equitherm.enable ? (settings.pid.maxTemp * -1) : settings.pid.minTemp,
          settings.pid.maxTemp
        );

        if (fabs(prevPidResult - pidResult) > 0.4999f) {
          prevPidResult = pidResult;
          newTemp += pidResult;

          Log.sinfoln(FPSTR(L_REGULATOR_PID), F("New result: %.2f"), pidResult);
          Log.straceln(FPSTR(L_REGULATOR_PID), F("Integral: %.2f"), pidRegulator.integral);

        } else {
          newTemp += prevPidResult;
        }
      } else {
        newTemp += prevPidResult;
      }

    } else if (fabs(pidRegulator.integral) > 0.0001f) {
      pidRegulator.integral = 0;
      Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
    }

    // default temp, manual mode
    if (!settings.equitherm.enable && !settings.pid.enable) {
      newTemp = settings.heating.target;
    }

    return newTemp;
  }

  /**
   * @brief Get the Equitherm Temp
   * Calculations in degrees C, conversion occurs when using F
   * 
   * @param minTemp 
   * @param maxTemp 
   * @return float 
   */
  float getEquithermTemp(int minTemp, int maxTemp) {
    float targetTemp = vars.states.emergency ? settings.emergency.target : settings.heating.target;
    float indoorTemp = vars.temperatures.indoor;
    float outdoorTemp = vars.temperatures.outdoor;

    if (settings.system.unitSystem == UnitSystem::IMPERIAL) {
      minTemp = f2c(minTemp);
      maxTemp = f2c(maxTemp);
      targetTemp = f2c(targetTemp);
      indoorTemp = f2c(indoorTemp);
      outdoorTemp = f2c(outdoorTemp);
    }

    if (vars.states.emergency) {
      if (settings.sensors.indoor.type == SensorType::MANUAL) {
        etRegulator.Kt = 0;
        etRegulator.indoorTemp = 0;

      } else if ((settings.sensors.indoor.type == SensorType::DS18B20 || settings.sensors.indoor.type == SensorType::BLUETOOTH) && !vars.sensors.indoor.connected) {
        etRegulator.Kt = 0;
        etRegulator.indoorTemp = 0;
        
      } else {
        etRegulator.Kt = settings.equitherm.t_factor;
        etRegulator.indoorTemp = indoorTemp;
      }
      
      etRegulator.outdoorTemp = outdoorTemp;

    } else if (settings.pid.enable) {
      etRegulator.Kt = 0;
      etRegulator.indoorTemp = round(indoorTemp);
      etRegulator.outdoorTemp = round(outdoorTemp);

    } else {
      if (settings.heating.turbo) {
        etRegulator.Kt = 10;
      } else {
        etRegulator.Kt = settings.equitherm.t_factor;
      }
      etRegulator.indoorTemp = indoorTemp;
      etRegulator.outdoorTemp = outdoorTemp;
    }

    etRegulator.setLimits(minTemp, maxTemp);
    etRegulator.Kn = settings.equitherm.n_factor;
    etRegulator.Kk = settings.equitherm.k_factor;
    etRegulator.targetTemp = targetTemp;
    float result = etRegulator.getResult();

    if (settings.system.unitSystem == UnitSystem::IMPERIAL) {
      result = c2f(result);
    }

    return result;
  }

  float getPidTemp(int minTemp, int maxTemp) {
    if (fabs(pidRegulator.Kp - settings.pid.p_factor) >= 0.0001f) {
      pidRegulator.Kp = settings.pid.p_factor;
      pidRegulator.integral = 0;
      Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
    }

    if (fabs(pidRegulator.Ki - settings.pid.i_factor) >= 0.0001f) {
      pidRegulator.Ki = settings.pid.i_factor;
      pidRegulator.integral = 0;
      Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
    }

    if (fabs(pidRegulator.Kd - settings.pid.d_factor) >= 0.0001f) {
      pidRegulator.Kd = settings.pid.d_factor;
      pidRegulator.integral = 0;
      Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
    }

    pidRegulator.setLimits(minTemp, maxTemp);
    pidRegulator.setDt(settings.pid.dt * 1000u);
    pidRegulator.input = vars.temperatures.indoor;
    pidRegulator.setpoint = vars.states.emergency ? settings.emergency.target : settings.heating.target;

    return pidRegulator.getResultTimer();
  }
};
