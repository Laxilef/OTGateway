#include "lib/Equitherm.h"
#include <GyverPID.h>
#include <PIDtuner.h>

Equitherm etRegulator;
GyverPID pidRegulator(0, 0, 0, 10000);
PIDtuner pidTuner;

class RegulatorTask : public LeanTask {
public:
  RegulatorTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  bool tunerInit = false;
  byte tunerState = 0;
  float prevHeatingTarget = 0;
  float prevEtResult = 0;
  float prevPidResult = 0;


  void setup() {}
  void loop() {
    byte newTemp;

    if (vars.states.emergency) {
      newTemp = getEmergencyModeTemp();
    } else {
      newTemp = getNormalModeTemp();
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
    byte newTemp = vars.parameters.heatingSetpoint;

    // if use equitherm
    if (settings.emergency.useEquitherm && settings.outdoorTempSource != 1) {
      etRegulator.Kn = settings.equitherm.n_factor;
      etRegulator.Kk = settings.equitherm.k_factor;
      etRegulator.Kt = 0;
      etRegulator.indoorTemp = 0;
      etRegulator.outdoorTemp = vars.temperatures.outdoor;

      etRegulator.setLimits(vars.parameters.heatingMinTemp, vars.parameters.heatingMaxTemp);
      etRegulator.targetTemp = settings.emergency.target;

      float etResult = etRegulator.getResult();
      if (fabs(prevEtResult - etResult) + 0.0001 >= 1) {
        prevEtResult = etResult;
        newTemp = round(etResult);

        INFO_F("New emergency equitherm result: %u (%f) \n", newTemp, etResult);
      }

    } else {
      // default temp, manual mode
      newTemp = round(settings.emergency.target);
    }

    return newTemp;
  }

  byte getNormalModeTemp() {
    bool updateIntegral = false;
    byte newTemp = vars.parameters.heatingSetpoint;

    if (fabs(prevHeatingTarget - settings.heating.target) > 0.0001) {
      prevHeatingTarget = settings.heating.target;
      updateIntegral = true;

      INFO_F("New heating target: %f \n", settings.heating.target);
    }

    // if use equitherm
    if (settings.equitherm.enable) {
      if (vars.tuning.enable && vars.tuning.regulator == 0) {
        if (settings.pid.enable) {
          settings.pid.enable = false;
        }

        etRegulator.Kn = tuneEquithermN(etRegulator.Kn, vars.temperatures.indoor, settings.heating.target, 300, 1800, 0.01, 1);
      } else {
        etRegulator.Kn = settings.equitherm.n_factor;
      }

      if (settings.pid.enable) {
        etRegulator.Kt = 0;
        etRegulator.indoorTemp = round(vars.temperatures.indoor);
        etRegulator.outdoorTemp = round(vars.temperatures.outdoor);
      } else {
        etRegulator.Kt = settings.equitherm.t_factor;
        etRegulator.indoorTemp = vars.temperatures.indoor;
        etRegulator.outdoorTemp = vars.temperatures.outdoor;
      }

      etRegulator.setLimits(vars.parameters.heatingMinTemp, vars.parameters.heatingMaxTemp);
      etRegulator.Kk = settings.equitherm.k_factor;
      etRegulator.targetTemp = settings.heating.target;

      float etResult = etRegulator.getResult();
      if (fabs(prevEtResult - etResult) + 0.0001 >= 1) {
        prevEtResult = etResult;
        updateIntegral = true;
        newTemp = round(etResult);

        INFO_F("New equitherm result: %u (%f) \n", newTemp, etResult);

      } else {
        updateIntegral = false;
      }
    }

    // if use pid
    if (settings.pid.enable && tunerInit && (!vars.tuning.enable || vars.tuning.regulator != 1)) {
      pidTuner.reset();
      tunerState = 0;
      tunerInit = false;
      INFO(F("Tuning stopped"));

    } else if (settings.pid.enable && vars.tuning.enable && vars.tuning.regulator == 1) {
      if (tunerInit && pidTuner.getState() == 3) {
        INFO(F("Tuning finished"));
        pidTuner.debugText(&INFO_STREAM);

        if (pidTuner.getAccuracy() < 90) {
          WARN(F("Tuning bad result, restart..."));

        } else {
          settings.pid.p_factor = pidTuner.getPID_p();
          settings.pid.i_factor = pidTuner.getPID_i();
          settings.pid.d_factor = pidTuner.getPID_d();
          vars.tuning.enable = false;
        }

        pidTuner.reset();
        tunerState = 0;
        tunerInit = false;

      } else {
        if (!tunerInit) {
          INFO(F("Tuning start"));

          float step;
          if (vars.temperatures.indoor - vars.temperatures.outdoor > 10) {
            step = ceil(vars.parameters.heatingSetpoint / vars.temperatures.indoor * 2);
          } else {
            step = 5.0f;
          }

          float startTemp = newTemp + step;
          if (startTemp >= vars.parameters.heatingMaxTemp) {
            startTemp = vars.parameters.heatingMaxTemp - 10;
          }

          INFO_F("Tuning started. Start temp: %f, step: %f \n", startTemp, step);
          pidTuner.setParameters(NORMAL, startTemp, step, 20 * 60 * 1000, 0.15, 60 * 1000, 10000);
          tunerInit = true;
        }

        pidTuner.setInput(vars.temperatures.indoor);
        pidTuner.compute();

        if (tunerState > 0 && pidTuner.getState() != tunerState) {
          INFO(F("Tuning log:"));
          pidTuner.debugText(&INFO_STREAM);
          tunerState = pidTuner.getState();
        }

        newTemp = round(pidTuner.getOutput());
      }
    }

    if (settings.pid.enable && (!vars.tuning.enable || vars.tuning.enable && vars.tuning.regulator != 1)) {
      if (updateIntegral) {
        pidRegulator.integral = settings.heating.target;
      }

      pidRegulator.Kp = settings.pid.p_factor;
      pidRegulator.Ki = settings.pid.i_factor;
      pidRegulator.Kd = settings.pid.d_factor;

      pidRegulator.setLimits(vars.parameters.heatingMinTemp, vars.parameters.heatingMaxTemp);
      pidRegulator.input = vars.temperatures.indoor;
      pidRegulator.setpoint = settings.heating.target;

      float pidResult = pidRegulator.getResultTimer();
      if (abs(prevPidResult - pidResult) >= 0.5) {
        prevPidResult = pidResult;
        newTemp = round(pidResult);

        INFO_F("New PID result: %u (%f) \n", newTemp, pidResult);
      }
    }

    // default temp, manual mode
    if (!settings.equitherm.enable && !settings.pid.enable) {
      newTemp = round(settings.heating.target);
    }

    return newTemp;
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
