#include <Equitherm.h>
#include <GyverPID.h>
#include <PIDtuner.h>

Equitherm etRegulator;
GyverPID pidRegulator(0, 0, 0);
PIDtuner pidTuner;

class RegulatorTask : public LeanTask {
public:
  RegulatorTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  const char* taskName = "Regulator task";
  const int taskCore = 2;

  bool tunerInit = false;
  byte tunerState = 0;
  byte tunerRegulator = 0;
  float prevHeatingTarget = 0;
  float prevEtResult = 0;
  float prevPidResult = 0;


  void setup() {}
  void loop() {
    byte newTemp = vars.parameters.heatingSetpoint;

    if (vars.states.emergency) {
      if (settings.heating.turbo) {
        settings.heating.turbo = false;

        INFO("[REGULATOR] Turbo mode auto disabled");
      }

      newTemp = getEmergencyModeTemp();

    } else {
      if (vars.tuning.enable || tunerInit) {
        if (settings.heating.turbo) {
          settings.heating.turbo = false;

          INFO("[REGULATOR] Turbo mode auto disabled");
        }

        newTemp = getTuningModeTemp();

        if (newTemp == 0) {
          vars.tuning.enable = false;
        }
      }

      if (!vars.tuning.enable) {
        if (settings.heating.turbo && (fabs(settings.heating.target - vars.temperatures.indoor) < 1 || (settings.equitherm.enable && settings.pid.enable))) {
          settings.heating.turbo = false;

          INFO("[REGULATOR] Turbo mode auto disabled");
        }

        newTemp = getNormalModeTemp();
      }
    }

    // Ограничиваем, если до этого не ограничило
    if (newTemp < vars.parameters.heatingMinTemp || newTemp > vars.parameters.heatingMaxTemp) {
      newTemp = constrain(newTemp, vars.parameters.heatingMinTemp, vars.parameters.heatingMaxTemp);
    }

    if (abs(vars.parameters.heatingSetpoint - newTemp) + 0.0001 >= 1) {
      vars.parameters.heatingSetpoint = newTemp;
    }
  }


  byte getEmergencyModeTemp() {
    float newTemp = 0;

    // if use equitherm
    if (settings.emergency.useEquitherm && settings.sensors.outdoor.type != 1) {
      float etResult = getEquithermTemp(vars.parameters.heatingMinTemp, vars.parameters.heatingMaxTemp);

      if (fabs(prevEtResult - etResult) + 0.0001 >= 0.5) {
        prevEtResult = etResult;
        newTemp += etResult;

        INFO_F("[REGULATOR][EQUITHERM] New emergency result: %u (%f) \n", (int)round(etResult), etResult);

      } else {
        newTemp += prevEtResult;
      }

    } else {
      // default temp, manual mode
      newTemp = settings.emergency.target;
    }

    return round(newTemp);
  }

  byte getNormalModeTemp() {
    float newTemp = 0;

    if (fabs(prevHeatingTarget - settings.heating.target) > 0.0001) {
      prevHeatingTarget = settings.heating.target;
      INFO_F("[REGULATOR] New target: %f \n", settings.heating.target);

      if (settings.equitherm.enable && settings.pid.enable) {
        pidRegulator.integral = 0;
        INFO_F("[REGULATOR][PID] Integral sum has been reset");
      }
    }

    // if use equitherm
    if (settings.equitherm.enable) {
      float etResult = getEquithermTemp(vars.parameters.heatingMinTemp, vars.parameters.heatingMaxTemp);

      if (fabs(prevEtResult - etResult) + 0.0001 >= 0.5) {
        prevEtResult = etResult;
        newTemp += etResult;

        INFO_F("[REGULATOR][EQUITHERM] New result: %u (%f) \n", (int)round(etResult), etResult);

      } else {
        newTemp += prevEtResult;
      }
    }

    // if use pid
    if (settings.pid.enable && vars.parameters.heatingEnabled) {
      float pidResult = getPidTemp(
        settings.equitherm.enable ? (settings.pid.maxTemp * -1) : settings.pid.minTemp,
        settings.equitherm.enable ? settings.pid.maxTemp : settings.pid.maxTemp
      );

      if (fabs(prevPidResult - pidResult) + 0.0001 >= 0.5) {
        prevPidResult = pidResult;
        newTemp += pidResult;

        INFO_F("[REGULATOR][PID] New result: %d (%f) \n", (int)round(pidResult), pidResult);

      } else {
        newTemp += prevPidResult;
      }

    } else if (settings.pid.enable && !vars.parameters.heatingEnabled && prevPidResult != 0) {
      newTemp += prevPidResult;
    }

    // default temp, manual mode
    if (!settings.equitherm.enable && !settings.pid.enable) {
      newTemp = settings.heating.target;
    }

    newTemp = round(newTemp);
    newTemp = constrain(newTemp, 0, 100);
    return newTemp;
  }

  byte getTuningModeTemp() {
    if (tunerInit && (!vars.tuning.enable || vars.tuning.regulator != tunerRegulator)) {
      if (tunerRegulator == 0) {
        pidTuner.reset();
      }

      tunerInit = false;
      tunerRegulator = 0;
      tunerState = 0;
      INFO(F("[REGULATOR][TUNING] Stopped"));
    }

    if (!vars.tuning.enable) {
      return 0;
    }


    if (vars.tuning.regulator == 0) {
      // @TODO дописать
      INFO(F("[REGULATOR][TUNING][EQUITHERM] Not implemented"));
      return 0;

    } else if (vars.tuning.regulator == 1) {
      // PID tuner
      float defaultTemp = settings.equitherm.enable
        ? getEquithermTemp(vars.parameters.heatingMinTemp, vars.parameters.heatingMaxTemp)
        : settings.heating.target;

      if (tunerInit && pidTuner.getState() == 3) {
        INFO(F("[REGULATOR][TUNING][PID] Finished"));
        pidTuner.debugText(&INFO_STREAM);

        pidTuner.reset();
        tunerInit = false;
        tunerRegulator = 0;
        tunerState = 0;

        if (pidTuner.getAccuracy() < 90) {
          WARN(F("[REGULATOR][TUNING][PID] Bad result, try again..."));

        } else {
          settings.pid.p_factor = pidTuner.getPID_p();
          settings.pid.i_factor = pidTuner.getPID_i();
          settings.pid.d_factor = pidTuner.getPID_d();

          return 0;
        }
      }

      if (!tunerInit) {
        INFO(F("[REGULATOR][TUNING][PID] Start..."));

        float step;
        if (vars.temperatures.indoor - vars.temperatures.outdoor > 10) {
          step = ceil(vars.parameters.heatingSetpoint / vars.temperatures.indoor * 2);
        } else {
          step = 5.0f;
        }

        float startTemp = step;
        INFO_F("[REGULATOR][TUNING][PID] Started. Start value: %f, step: %f \n", startTemp, step);
        pidTuner.setParameters(NORMAL, startTemp, step, 20 * 60 * 1000, 0.15, 60 * 1000, 10000);
        tunerInit = true;
        tunerRegulator = 1;
      }

      pidTuner.setInput(vars.temperatures.indoor);
      pidTuner.compute();

      if (tunerState > 0 && pidTuner.getState() != tunerState) {
        INFO(F("[REGULATOR][TUNING][PID] Log:"));
        pidTuner.debugText(&INFO_STREAM);
        tunerState = pidTuner.getState();
      }

      return round(defaultTemp + pidTuner.getOutput());

    } else {
      return 0;
    }
  }

  float getEquithermTemp(int minTemp, int maxTemp) {
    if (vars.states.emergency) {
      etRegulator.Kt = 0;
      etRegulator.indoorTemp = 0;
      etRegulator.outdoorTemp = vars.temperatures.outdoor;

    } else if (settings.pid.enable) {
      etRegulator.Kt = 0;
      etRegulator.indoorTemp = round(vars.temperatures.indoor);
      etRegulator.outdoorTemp = round(vars.temperatures.outdoor);

    } else {
      if (settings.heating.turbo) {
        etRegulator.Kt = 10;
      } else {
        etRegulator.Kt = settings.equitherm.t_factor;
      }
      etRegulator.indoorTemp = vars.temperatures.indoor;
      etRegulator.outdoorTemp = vars.temperatures.outdoor;
    }

    etRegulator.setLimits(minTemp, maxTemp);
    etRegulator.Kn = settings.equitherm.n_factor;
    // etRegulator.Kn = tuneEquithermN(etRegulator.Kn, vars.temperatures.indoor, settings.heating.target, 300, 1800, 0.01, 1);
    etRegulator.Kk = settings.equitherm.k_factor;
    etRegulator.targetTemp = vars.states.emergency ? settings.emergency.target : settings.heating.target;

    return etRegulator.getResult();
  }

  float getPidTemp(int minTemp, int maxTemp) {
    pidRegulator.Kp = settings.pid.p_factor;
    pidRegulator.Ki = settings.pid.i_factor;
    pidRegulator.Kd = settings.pid.d_factor;

    pidRegulator.setLimits(minTemp, maxTemp);
    pidRegulator.input = vars.temperatures.indoor;
    pidRegulator.setpoint = settings.heating.target;

    return pidRegulator.getResultNow();
  }

  float tuneEquithermN(float ratio, float currentTemp, float setTemp, unsigned int dirtyInterval = 60, unsigned int accurateInterval = 1800, float accurateStep = 0.01, float accurateStepAfter = 1) {
    static uint32_t _prevIteration = millis();

    if (abs(currentTemp - setTemp) < accurateStepAfter) {
      if (millis() - _prevIteration < (accurateInterval * 1000)) {
        return ratio;
      }

      if (currentTemp - setTemp > 0.1f) {
        ratio -= accurateStep;

      } else if (currentTemp - setTemp < -0.1f) {
        ratio += accurateStep;
      }

    } else {
      if (millis() - _prevIteration < (dirtyInterval * 1000)) {
        return ratio;
      }

      ratio = ratio * (setTemp / currentTemp);
    }

    _prevIteration = millis();
    return ratio;
  }

};
