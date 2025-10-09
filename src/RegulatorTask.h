#include <GyverPID.h>

GyverPID pidRegulator(0, 0, 0);


class RegulatorTask : public LeanTask {
public:
  RegulatorTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  float prevHeatingTarget = 0.0f;
  float prevEtResult = 0.0f;
  float prevPidResult = 0.0f;

  bool indoorSensorsConnected = false;
  //bool outdoorSensorsConnected = false;

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
    if (vars.states.restarting || vars.states.upgrading) {
      return;
    }

    this->indoorSensorsConnected = Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::INDOOR_TEMP);
    //this->outdoorSensorsConnected = Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::OUTDOOR_TEMP);

    if (settings.equitherm.enabled || settings.pid.enabled || settings.opentherm.options.nativeHeatingControl) {
      vars.master.heating.indoorTempControl = true;
      vars.master.heating.minTemp = THERMOSTAT_INDOOR_MIN_TEMP;
      vars.master.heating.maxTemp = THERMOSTAT_INDOOR_MAX_TEMP;

    } else {
      vars.master.heating.indoorTempControl = false;
      vars.master.heating.minTemp = settings.heating.minTemp;
      vars.master.heating.maxTemp = settings.heating.maxTemp;
    }

    if (!settings.pid.enabled && fabsf(pidRegulator.integral) > 0.01f) {
      pidRegulator.integral = 0.0f;

      Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
    }

    this->turbo();
    this->hysteresis();

    vars.master.heating.targetTemp = settings.heating.target;
    vars.master.heating.setpointTemp = roundf(constrain(
      this->getHeatingSetpointTemp(),
      this->getHeatingMinSetpointTemp(),
      this->getHeatingMaxSetpointTemp()
    ), 0);

    Sensors::setValueByType(
      Sensors::Type::HEATING_SETPOINT_TEMP, vars.master.heating.setpointTemp,
      Sensors::ValueType::PRIMARY, true, true
    );
  }

  void turbo() {
    if (settings.heating.turbo) {
      if (!settings.heating.enabled || vars.emergency.state || !this->indoorSensorsConnected) {
        settings.heating.turbo = false;

      } else if (!settings.pid.enabled && !settings.equitherm.enabled) {
        settings.heating.turbo = false;

      } else if (fabsf(settings.heating.target - vars.master.heating.indoorTemp) <= 1.0f) {
        settings.heating.turbo = false;
      }

      if (!settings.heating.turbo) {
        Log.sinfoln(FPSTR(L_REGULATOR), F("Turbo mode auto disabled"));
      }
    }
  }

  void hysteresis() {
    bool useHyst = false;
    if (settings.heating.hysteresis > 0.01f && this->indoorSensorsConnected) {
      useHyst = settings.equitherm.enabled || settings.pid.enabled || settings.opentherm.options.nativeHeatingControl;
    }

    if (useHyst) {
      if (!vars.master.heating.blocking && vars.master.heating.indoorTemp - settings.heating.target + 0.0001f >= settings.heating.hysteresis) {
        vars.master.heating.blocking = true;

      } else if (vars.master.heating.blocking && vars.master.heating.indoorTemp - settings.heating.target - 0.0001f <= -(settings.heating.hysteresis)) {
        vars.master.heating.blocking = false;
      }

    } else if (vars.master.heating.blocking) {
      vars.master.heating.blocking = false;
    }
  }

  inline float getHeatingMinSetpointTemp() {
    return settings.opentherm.options.nativeHeatingControl
      ? vars.master.heating.minTemp
      : settings.heating.minTemp;
  }

  inline float getHeatingMaxSetpointTemp() {
    return settings.opentherm.options.nativeHeatingControl
      ? vars.master.heating.maxTemp
      : settings.heating.maxTemp;
  }

  float getHeatingSetpointTemp() {
    float newTemp = 0;

    if (fabsf(prevHeatingTarget - settings.heating.target) > 0.0001f) {
      prevHeatingTarget = settings.heating.target;
      Log.sinfoln(FPSTR(L_REGULATOR), F("New target: %.2f"), settings.heating.target);

      /*if (settings.pid.enabled) {
        pidRegulator.integral = 0.0f;
        Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
      }*/
    }

    if (vars.emergency.state) {
      return settings.emergency.target;

    } else if (settings.opentherm.options.nativeHeatingControl) {
      return settings.heating.target;

    } else if (!settings.equitherm.enabled && !settings.pid.enabled) {
      return settings.heating.target;
    }

    // if use equitherm
    if (settings.equitherm.enabled) {
      float tempDelta = settings.heating.target - vars.master.heating.outdoorTemp;
      float maxPoint = settings.heating.target - (
        settings.heating.maxTemp - settings.heating.target
      ) / settings.equitherm.slope;

      float sf = (settings.heating.maxTemp - settings.heating.target) / pow(
        settings.heating.target - maxPoint,
        1.0f / settings.equitherm.exponent
      );
      float etResult = settings.heating.target + settings.equitherm.shift + sf * (
        tempDelta >= 0 
          ? pow(tempDelta, 1.0f / settings.equitherm.exponent) 
          : -(pow(-(tempDelta), 1.0f / settings.equitherm.exponent))
      );

      // add diff
      if (this->indoorSensorsConnected && !settings.pid.enabled && !settings.heating.turbo) {
        etResult += constrain(
          settings.heating.target - vars.master.heating.indoorTemp,
          -3.0f,
          3.0f
        ) * settings.equitherm.targetDiffFactor;
      }

      // limit
      etResult = constrain(etResult, settings.heating.minTemp, settings.heating.maxTemp);

      if (fabsf(prevEtResult - etResult) > 0.09f) {
        prevEtResult = etResult;
        newTemp += etResult;

        Log.sinfoln(FPSTR(L_REGULATOR_EQUITHERM), F("New result: %.2f"), etResult);

      } else {
        newTemp += prevEtResult;
      }
    }

    // if use pid
    if (settings.pid.enabled) {
      //if (vars.parameters.heatingEnabled) {
      if (settings.heating.enabled && this->indoorSensorsConnected) {
        pidRegulator.Kp = settings.heating.turbo ? 0.0f : settings.pid.p_factor;
        pidRegulator.Ki = settings.pid.i_factor;
        pidRegulator.Kd = settings.pid.d_factor;

        pidRegulator.setLimits(settings.pid.minTemp, settings.pid.maxTemp);
        pidRegulator.setDt(settings.pid.dt * 1000u);
        pidRegulator.input = vars.master.heating.indoorTemp;
        pidRegulator.setpoint = settings.heating.target;

        /*if (fabsf(pidRegulator.Ki - settings.pid.i_factor) >= 0.0001f) {
          pidRegulator.Ki = settings.pid.i_factor;
          pidRegulator.integral = 0.0f;
          pidRegulator.getResultNow();

          Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
        }*/

        float error = pidRegulator.setpoint - pidRegulator.input;
        bool hasDeadband = settings.pid.deadband.enabled 
          && (error > -(settings.pid.deadband.thresholdHigh))
          && (error < settings.pid.deadband.thresholdLow);

        if (hasDeadband) {
          pidRegulator.Kp *= settings.pid.deadband.p_multiplier;
          pidRegulator.Ki *= settings.pid.deadband.i_multiplier;
          pidRegulator.Kd *= settings.pid.deadband.d_multiplier;
        }

        float pidResult = pidRegulator.getResultTimer();
        if (fabsf(prevPidResult - pidResult) > 0.09f) {
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
    }

    // Turbo mode
    if (settings.heating.turbo && (settings.equitherm.enabled || settings.pid.enabled)) {
      newTemp += constrain(
        settings.heating.target - vars.master.heating.indoorTemp,
        -3.0f,
        3.0f
      ) * settings.heating.turboFactor;
    }

    return newTemp;
  }
};
