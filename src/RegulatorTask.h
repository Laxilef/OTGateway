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
    if (!settings.pid.enable && fabs(pidRegulator.integral) > 0.01f) {
      pidRegulator.integral = 0.0f;

      Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
    }

    if (settings.heating.turbo) {
      if (!settings.heating.enable || vars.states.emergency || !vars.sensors.indoor.connected) {
        settings.heating.turbo = false;

      } else if (!settings.pid.enable && !settings.equitherm.enable) {
        settings.heating.turbo = false;

      } else if (fabs(settings.heating.target - vars.temperatures.indoor) <= 1.0f) {
        settings.heating.turbo = false;
      }

      if (!settings.heating.turbo) {
        Log.sinfoln(FPSTR(L_REGULATOR), F("Turbo mode auto disabled"));
      }
    }


    float newTemp = vars.states.emergency
      ? settings.emergency.target
      : this->getNormalModeTemp();

    // Limits
    newTemp = constrain(
      newTemp,
      !settings.opentherm.nativeHeatingControl ? settings.heating.minTemp : THERMOSTAT_INDOOR_MIN_TEMP,
      !settings.opentherm.nativeHeatingControl ? settings.heating.maxTemp : THERMOSTAT_INDOOR_MAX_TEMP
    );

    if (fabs(vars.parameters.heatingSetpoint - newTemp) > 0.09f) {
      vars.parameters.heatingSetpoint = newTemp;
    }
  }


  float getNormalModeTemp() {
    float newTemp = 0;

    if (fabs(prevHeatingTarget - settings.heating.target) > 0.0001f) {
      prevHeatingTarget = settings.heating.target;
      Log.sinfoln(FPSTR(L_REGULATOR), F("New target: %.2f"), settings.heating.target);

      /*if (settings.pid.enable) {
        pidRegulator.integral = 0.0f;
        Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
      }*/
    }

    // if use equitherm
    if (settings.equitherm.enable) {
      unsigned short minTemp = settings.heating.minTemp;
      unsigned short maxTemp = settings.heating.maxTemp;
      float targetTemp = settings.heating.target;
      float indoorTemp = vars.temperatures.indoor;
      float outdoorTemp = vars.temperatures.outdoor;

      if (settings.system.unitSystem == UnitSystem::IMPERIAL) {
        minTemp = f2c(minTemp);
        maxTemp = f2c(maxTemp);
        targetTemp = f2c(targetTemp);
        indoorTemp = f2c(indoorTemp);
        outdoorTemp = f2c(outdoorTemp);
      }

      if (!vars.sensors.indoor.connected || settings.pid.enable) {
        etRegulator.Kt = 0.0f;
        etRegulator.indoorTemp = 0.0f;

      } else {
        etRegulator.Kt = settings.heating.turbo ? 0.0f : settings.equitherm.t_factor;
        etRegulator.indoorTemp = indoorTemp;
      }

      etRegulator.setLimits(minTemp, maxTemp);
      etRegulator.Kn = settings.equitherm.n_factor;
      etRegulator.Kk = settings.equitherm.k_factor;
      etRegulator.targetTemp = targetTemp;
      etRegulator.outdoorTemp = outdoorTemp;
      float etResult = etRegulator.getResult();

      if (settings.system.unitSystem == UnitSystem::IMPERIAL) {
        etResult = c2f(etResult);
      }

      if (fabs(prevEtResult - etResult) > 0.09f) {
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
      if (settings.heating.enable && vars.sensors.indoor.connected) {
        pidRegulator.Kp = settings.heating.turbo ? 0.0f : settings.pid.p_factor;
        pidRegulator.Kd = settings.pid.d_factor;

        pidRegulator.setLimits(settings.pid.minTemp, settings.pid.maxTemp);
        pidRegulator.setDt(settings.pid.dt * 1000u);
        pidRegulator.input = vars.temperatures.indoor;
        pidRegulator.setpoint = settings.heating.target;

        if (fabs(pidRegulator.Ki - settings.pid.i_factor) >= 0.0001f) {
          pidRegulator.Ki = settings.pid.i_factor;
          pidRegulator.integral = 0.0f;
          pidRegulator.getResultNow();

          Log.sinfoln(FPSTR(L_REGULATOR_PID), F("Integral sum has been reset"));
        }

        float pidResult = pidRegulator.getResultTimer();
        if (fabs(prevPidResult - pidResult) > 0.09f) {
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
    if (settings.heating.turbo && (settings.equitherm.enable || settings.pid.enable)) {
      newTemp += constrain(
        settings.heating.target - vars.temperatures.indoor,
        -3.0f,
        3.0f
      ) * settings.heating.turboFactor;
    }

    // default temp, manual mode
    if (!settings.equitherm.enable && !settings.pid.enable) {
      newTemp = settings.heating.target;
    }

    return newTemp;
  }
};
