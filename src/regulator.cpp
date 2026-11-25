#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>

#include <cmath>
#include <GyverPID.h>

static GyverPID pidRegulator(0, 0, 0);

static float prevHeatingTarget = 0.0f;
static float equithermResult = 0.0f;
static float pidResult = 0.0f;
static bool indoorSensorsConnected = false;

static Task equithermTask("Equitherm", Task::Type::ONCE, [](void*) {
  float etResult = settings.heating.target + settings.equitherm.shift;

  if (settings.heating.maxTemp > settings.heating.target) {
    float tempDelta = settings.heating.target - vars.master.heating.outdoorTemp;
    float maxPoint = settings.heating.target - (
      settings.heating.maxTemp - settings.heating.target
    ) / settings.equitherm.slope;

    float sf = (settings.heating.maxTemp - settings.heating.target) / pow(
      settings.heating.target - maxPoint,
      1.0f / settings.equitherm.exponent
    );
    etResult = settings.heating.target + settings.equitherm.shift + sf * (
      tempDelta >= 0
        ? pow(tempDelta, 1.0f / settings.equitherm.exponent)
        : -(pow(-(tempDelta), 1.0f / settings.equitherm.exponent))
    );
  }

  if (indoorSensorsConnected && !settings.pid.enabled && !settings.heating.turbo) {
    etResult += constrain(
      settings.heating.target - vars.master.heating.indoorTemp,
      -3.0f,
      3.0f
    ) * settings.equitherm.targetDiffFactor;
  }

  if (!std::isfinite(etResult)) {
    etResult = settings.heating.target + settings.equitherm.shift;
  }

  etResult = constrain(etResult, settings.heating.minTemp, settings.heating.maxTemp);

  if (isFloatDifferent(equithermResult, etResult, 0.09f)) {
    equithermResult = etResult;
    ESP_LOGI(L_REGULATOR_EQUITHERM, "New result: %.2f", etResult);
  }
});

static Task pidTask("PID", Task::Type::FOREVER, [](void*) {
  pidRegulator.Kp = settings.heating.turbo ? 0.0f : settings.pid.p_factor;
  pidRegulator.Ki = settings.pid.i_factor;
  pidRegulator.Kd = settings.pid.d_factor;

  pidRegulator.setLimits(settings.pid.minTemp, settings.pid.maxTemp);
  // Use the same period for scheduling and the integral/derivative terms.
  pidRegulator.setDt(pidTask.interval());
  pidRegulator.input = vars.master.heating.indoorTemp;
  pidRegulator.setpoint = settings.heating.target;

  float error = pidRegulator.setpoint - pidRegulator.input;
  bool hasDeadband = settings.pid.deadband.enabled &&
                     (error > -(settings.pid.deadband.thresholdHigh)) &&
                     (error < settings.pid.deadband.thresholdLow);

  if (hasDeadband) {
    pidRegulator.Kp *= settings.pid.deadband.p_multiplier;
    pidRegulator.Ki *= settings.pid.deadband.i_multiplier;
    pidRegulator.Kd *= settings.pid.deadband.d_multiplier;
  }

  float result = pidRegulator.getResult();
  if (isFloatDifferent(pidResult, result, 0.09f)) {
    pidResult = result;
    ESP_LOGI(L_REGULATOR_PID, "New result: %.2f", result);
    ESP_LOGD(L_REGULATOR_PID, "Integral: %.2f", pidRegulator.integral);
  }
});

static void turbo() {
  if (settings.heating.turbo) {
    if (!settings.heating.enabled || vars.emergency.state || !indoorSensorsConnected) {
      settings.heating.turbo = false;

    } else if (!settings.pid.enabled && !settings.equitherm.enabled) {
      settings.heating.turbo = false;

    } else if (fabsf(settings.heating.target - vars.master.heating.indoorTemp) <= 1.0f) {
      settings.heating.turbo = false;
    }

    if (!settings.heating.turbo) {
      ESP_LOGI(L_REGULATOR, "Turbo mode auto disabled");
    }
  }
}

static void hysteresis() {
  // Hysteresis also clears blocking when it is no longer applicable.
  const bool useHysteresis =
      settings.heating.hysteresis.enabled && indoorSensorsConnected &&
      (settings.equitherm.enabled || settings.pid.enabled || settings.opentherm.options.nativeOTC);

  if (useHysteresis) {
    if (!vars.master.heating.blocking && vars.master.heating.indoorTemp - settings.heating.target + 0.0001f >= settings.heating.hysteresis.value) {
      vars.master.heating.blocking = true;

    } else if (vars.master.heating.blocking && vars.master.heating.indoorTemp - settings.heating.target - 0.0001f <= -(settings.heating.hysteresis.value)) {
      vars.master.heating.blocking = false;
    }

  } else if (vars.master.heating.blocking) {
    vars.master.heating.blocking = false;
  }
}

static inline float getHeatingMinSetpointTemp() {
  return settings.opentherm.options.nativeOTC
    ? vars.master.heating.minTemp
    : settings.heating.minTemp;
}

static inline float getHeatingMaxSetpointTemp() {
  return settings.opentherm.options.nativeOTC
    ? vars.master.heating.maxTemp
    : settings.heating.maxTemp;
}

static float getHeatingSetpointTemp() {
  if (isFloatDifferent(prevHeatingTarget, settings.heating.target, 0.0001f)) {
    prevHeatingTarget = settings.heating.target;
    ESP_LOGI(L_REGULATOR, "New target: %.2f", settings.heating.target);
  }

  if (vars.emergency.state) {
    return settings.emergency.target;

  } else if (settings.opentherm.options.nativeOTC) {
    return settings.heating.target;
  }

  float setpoint = settings.heating.target;
  if (settings.equitherm.enabled || settings.pid.enabled) {
    setpoint = 0.0f;

    if (settings.equitherm.enabled) {
      equithermTask.forceRun();
      setpoint += equithermResult;
    }

    // Keep the last PID result when heating or the indoor sensor is unavailable.
    if (settings.pid.enabled) {
      setpoint += pidResult;
    }

    if (settings.heating.turbo) {
      setpoint += constrain(
        settings.heating.target - vars.master.heating.indoorTemp,
        -3.0f,
        3.0f
      ) * settings.heating.turboFactor;
    }
  }

  if (vars.master.heating.freezing) {
    setpoint = fmaxf(
      setpoint,
      static_cast<float>(settings.heating.freezeProtection.lowTemp)
    );
  }

  return setpoint;
}

static Task regulatorTask("Regulator", [](void*) {
  indoorSensorsConnected = Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::INDOOR_TEMP);

  if (pidTask.enabled()) {
    const uint32_t pidInterval = settings.pid.dt * 1000u;

    if (pidTask.interval() != pidInterval) {
      pidTask.setInterval(pidInterval);
    }
  }

  if (settings.equitherm.enabled || settings.pid.enabled || settings.opentherm.options.nativeOTC) {
    vars.master.heating.smartControl = true;
    vars.master.heating.minTemp = THERMOSTAT_INDOOR_MIN_TEMP;
    vars.master.heating.maxTemp = THERMOSTAT_INDOOR_MAX_TEMP;

  } else {
    vars.master.heating.smartControl = false;
    vars.master.heating.minTemp = settings.heating.minTemp;
    vars.master.heating.maxTemp = settings.heating.maxTemp;
  }

  // This must still run when the PID calculation task is disabled.
  if (!settings.pid.enabled && fabsf(pidRegulator.integral) > 0.01f) {
    pidRegulator.integral = 0.0f;
    ESP_LOGI(L_REGULATOR_PID, "Integral sum has been reset");
  }

  turbo();
  hysteresis();

  const float setpointTemp = getHeatingSetpointTemp();
  vars.master.heating.targetTemp = settings.opentherm.options.nativeOTC
    ? setpointTemp
    : settings.heating.target;
  vars.master.heating.setpointTemp = roundf(constrain(
    setpointTemp,
    getHeatingMinSetpointTemp(),
    getHeatingMaxSetpointTemp()
  ), 0);

  Sensors::setValueByType(
    Sensors::Type::HEATING_SETPOINT_TEMP, vars.master.heating.setpointTemp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

void initRegulator() {
  regulatorTask.setInterval(10000);

  // Sensor availability is refreshed by the regulator before checking PID eligibility.
  pidTask.setEnabledWhen([] {
    return settings.pid.enabled && settings.heating.enabled &&
           !vars.emergency.state && !settings.opentherm.options.nativeOTC &&
           indoorSensorsConnected;
  });
  pidTask.setInterval(10000);
  pidTask.resume(pidTask.interval());

  // Apply mode changes and turbo auto-disable before a PID calculation due on the same pass.
  coreTaskManager.addTask(regulatorTask);
  coreTaskManager.addTask(pidTask);
  coreTaskManager.addTask(equithermTask);
  equithermTask.pause();
}
