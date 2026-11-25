#include <app.h>

#include <algorithm>

HA::Discovery haDiscovery;

bool publishHaEntity(HaEntity entityId, bool remove) {
  std::unique_ptr<HA::Component> entity;
  const UnitSystem unit = settings.system.unitSystem;
  const uint32_t stateExpiration = std::max<uint32_t>(60u, 2u * settings.mqtt.interval);

  switch (entityId) {
    case HA_ENTITY_HEATING_TURBO: {
      auto toggle = std::make_unique<HA::Switch>("heating_turbo", "Turbo heating");
      if (!remove) {
        toggle->stateTopic = "~/settings/heating/turbo";
        toggle->commandTopic = toggle->stateTopic + "/set";
        toggle->category = HA::Category::CONFIG;
        toggle->payloadOn = S_TRUE;
        toggle->payloadOff = S_FALSE;
        toggle->icon = "mdi:rocket-launch-outline";
        toggle->enabledByDefault = false;
      }
      entity = std::move(toggle);
      break;
    }

    case HA_ENTITY_HEATING_HYSTERESIS_ENABLED: {
      auto toggle = std::make_unique<HA::Switch>("heating_hysteresis", "Use heating hysteresis");
      if (!remove) {
        toggle->stateTopic = "~/settings/heating/hysteresis/enabled";
        toggle->commandTopic = toggle->stateTopic + "/set";
        toggle->category = HA::Category::CONFIG;
        toggle->payloadOn = S_TRUE;
        toggle->payloadOff = S_FALSE;
        toggle->icon = "mdi:altimeter";
        toggle->enabledByDefault = true;
      }
      entity = std::move(toggle);
      break;
    }

    case HA_ENTITY_HEATING_HYSTERESIS_VALUE: {
      auto number = std::make_unique<HA::Number>("heating_hysteresis", "Heating hysteresis");
      if (!remove) {
        number->stateTopic = "~/settings/heating/hysteresis/value";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 0;
        number->max = 15;
        number->step = 0.01f;
        number->icon = "mdi:altimeter";
        number->enabledByDefault = true;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_HEATING_TURBO_FACTOR: {
      auto number = std::make_unique<HA::Number>("heating_turbo_factor", "Heating turbo factor");
      if (!remove) {
        number->stateTopic = "~/settings/heating/turbo_factor";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 1.5f;
        number->max = 10;
        number->step = 0.01f;
        number->icon = "mdi:multiplication-box";
        number->enabledByDefault = false;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_HEATING_MIN_TEMP: {
      auto number = std::make_unique<HA::Number>("heating_min_temp", "Heating min temp");
      if (!remove) {
        number->stateTopic = "~/settings/heating/min_temp";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = convertTemp(vars.slave.heating.minTemp, UnitSystem::METRIC, unit);
        number->max = convertTemp(vars.slave.heating.maxTemp - 1.0f, UnitSystem::METRIC, unit);
        if (unit == UnitSystem::METRIC) {
          number->unit = "°C";
          number->step = 1.0f;

        } else if (unit == UnitSystem::IMPERIAL) {
          number->unit = "°F";
          number->step = 0.1f;
        }
        number->icon = "mdi:thermometer-chevron-down";
        number->deviceClass = S_TEMPERATURE;
        number->enabledByDefault = true;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_HEATING_MAX_TEMP: {
      auto number = std::make_unique<HA::Number>("heating_max_temp", "Heating max temp");
      if (!remove) {
        number->stateTopic = "~/settings/heating/max_temp";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = convertTemp(vars.slave.heating.minTemp + 1.0f, UnitSystem::METRIC, unit);
        number->max = convertTemp(vars.slave.heating.maxTemp, UnitSystem::METRIC, unit);
        if (unit == UnitSystem::METRIC) {
          number->unit = "°C";
          number->step = 1.0f;

        } else if (unit == UnitSystem::IMPERIAL) {
          number->unit = "°F";
          number->step = 0.1f;
        }
        number->icon = "mdi:thermometer-chevron-up";
        number->deviceClass = S_TEMPERATURE;
        number->enabledByDefault = true;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_PID_ENABLED: {
      auto toggle = std::make_unique<HA::Switch>("pid", "PID");
      if (!remove) {
        toggle->stateTopic = "~/settings/pid/enabled";
        toggle->commandTopic = toggle->stateTopic + "/set";
        toggle->category = HA::Category::CONFIG;
        toggle->payloadOn = S_TRUE;
        toggle->payloadOff = S_FALSE;
        toggle->icon = "mdi:chart-bar-stacked";
        toggle->enabledByDefault = true;
      }
      entity = std::move(toggle);
      break;
    }

    case HA_ENTITY_PID_P_FACTOR: {
      auto number = std::make_unique<HA::Number>("pid_p_factor", "PID factor P");
      if (!remove) {
        number->stateTopic = "~/settings/pid/p_factor";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 0.001f;
        number->max = 1000;
        number->step = 0.001f;
        number->icon = "mdi:alpha-p-circle-outline";
        number->enabledByDefault = false;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_PID_I_FACTOR: {
      auto number = std::make_unique<HA::Number>("pid_i_factor", "PID factor I");
      if (!remove) {
        number->stateTopic = "~/settings/pid/i_factor";
        number->commandTopic = number->stateTopic + "/set";
        // Home Assistant requires a Number step of at least 0.001.
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 0;
        number->max = 100;
        number->step = 0.001f;
        number->icon = "mdi:alpha-i-circle-outline";
        number->enabledByDefault = false;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_PID_D_FACTOR: {
      auto number = std::make_unique<HA::Number>("pid_d_factor", "PID factor D");
      if (!remove) {
        number->stateTopic = "~/settings/pid/d_factor";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 0;
        number->max = 100000;
        number->step = 0.1f;
        number->icon = "mdi:alpha-d-circle-outline";
        number->enabledByDefault = false;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_PID_DT: {
      auto number = std::make_unique<HA::Number>("pid_dt", "PID DT");
      if (!remove) {
        number->stateTopic = "~/settings/pid/dt";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 30;
        number->max = 1800;
        number->step = 1;
        number->icon = "mdi:timer-cog-outline";
        number->enabledByDefault = false;
        number->deviceClass = "duration";
        number->unit = "s";
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_PID_MIN_TEMP: {
      auto number = std::make_unique<HA::Number>("pid_min_temp", "PID min temp");
      if (!remove) {
        number->stateTopic = "~/settings/pid/min_temp";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = settings.equitherm.enabled
          ? -99.0f
          : convertTemp(0, UnitSystem::METRIC, unit);
        number->max = settings.equitherm.enabled
          ? 99.0f
          : convertTemp(99, UnitSystem::METRIC, unit);
        if (unit == UnitSystem::METRIC) {
          number->unit = "°C";
          number->step = 1.0f;

        } else if (unit == UnitSystem::IMPERIAL) {
          number->unit = "°F";
          number->step = settings.equitherm.enabled ? 1.0f : 0.1f;
        }
        number->icon = "mdi:thermometer-chevron-down";
        number->enabledByDefault = false;
        number->deviceClass = S_TEMPERATURE;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_PID_MAX_TEMP: {
      auto number = std::make_unique<HA::Number>("pid_max_temp", "PID max temp");
      if (!remove) {
        number->stateTopic = "~/settings/pid/max_temp";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = settings.equitherm.enabled
          ? 1.0f
          : convertTemp(1, UnitSystem::METRIC, unit);
        number->max = settings.equitherm.enabled
          ? 99.0f
          : convertTemp(99, UnitSystem::METRIC, unit);
        if (unit == UnitSystem::METRIC) {
          number->unit = "°C";
          number->step = 1.0f;

        } else if (unit == UnitSystem::IMPERIAL) {
          number->unit = "°F";
          number->step = settings.equitherm.enabled ? 1.0f : 0.1f;
        }
        number->icon = "mdi:thermometer-chevron-up";
        number->enabledByDefault = false;
        number->deviceClass = S_TEMPERATURE;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_EQUITHERM_ENABLED: {
      auto toggle = std::make_unique<HA::Switch>("equitherm", "Equitherm");
      if (!remove) {
        toggle->stateTopic = "~/settings/equitherm/enabled";
        toggle->commandTopic = toggle->stateTopic + "/set";
        toggle->category = HA::Category::CONFIG;
        toggle->payloadOn = S_TRUE;
        toggle->payloadOff = S_FALSE;
        toggle->icon = "mdi:sun-snowflake-variant";
        toggle->enabledByDefault = true;
      }
      entity = std::move(toggle);
      break;
    }

    case HA_ENTITY_EQUITHERM_SLOPE: {
      auto number = std::make_unique<HA::Number>("equitherm_slope", "Equitherm slope");
      if (!remove) {
        number->stateTopic = "~/settings/equitherm/slope";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 0.001f;
        number->max = 10;
        number->step = 0.001f;
        number->icon = "mdi:slope-uphill";
        number->enabledByDefault = false;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_EQUITHERM_EXPONENT: {
      auto number = std::make_unique<HA::Number>("equitherm_exponent", "Equitherm exponent");
      if (!remove) {
        number->stateTopic = "~/settings/equitherm/exponent";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 0.011f;
        number->max = 2;
        number->step = 0.001f;
        number->icon = "mdi:exponent";
        number->enabledByDefault = false;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_EQUITHERM_SHIFT: {
      auto number = std::make_unique<HA::Number>("equitherm_shift", "Equitherm shift");
      if (!remove) {
        number->stateTopic = "~/settings/equitherm/shift";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = -15;
        number->max = 15;
        number->step = 0.01f;
        number->icon = "mdi:chart-areaspline";
        number->enabledByDefault = false;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_EQUITHERM_TARGET_DIFF_FACTOR: {
      auto number = std::make_unique<HA::Number>("equitherm_target_diff_factor", "Equitherm target diff factor");
      if (!remove) {
        number->stateTopic = "~/settings/equitherm/target_diff_factor";
        number->commandTopic = number->stateTopic + "/set";
        number->availabilityTopic = "~/settings/pid/enabled";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = 0;
        number->max = 10;
        number->step = 0.001f;
        number->icon = "mdi:chart-timeline-variant-shimmer";
        number->enabledByDefault = false;
        number->payloadAvailable = S_FALSE;
        number->payloadNotAvailable = S_TRUE;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_STATUS: {
      auto state = std::make_unique<HA::State>("status", "Status");
      if (!remove) {
        state->stateTopic = "~/status";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = "ON";
        state->payloadOff = "OFF";
        state->deviceClass = "problem";
        state->icon = "mdi:list-status";
        state->useDeviceAvailability = false;
        state->valueTemplate = "{{ iif(value == 'online', 'OFF', 'ON') }}";
        state->expireAfter = stateExpiration;
        state->enabledByDefault = true;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_EMERGENCY: {
      auto state = std::make_unique<HA::State>("emergency", "Emergency");
      if (!remove) {
        state->stateTopic = "~/state/master/emergency/state";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "problem";
        state->icon = "mdi:alert-rhombus-outline";
        state->enabledByDefault = true;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_OPENTHERM_STATUS: {
      auto state = std::make_unique<HA::State>("ot_status", "Opentherm status");
      if (!remove) {
        state->stateTopic = "~/state/slave/connected";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "connectivity";
        state->icon = "mdi:list-status";
        state->enabledByDefault = true;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_HEATING: {
      auto state = std::make_unique<HA::State>("heating", "Heating");
      if (!remove) {
        state->stateTopic = "~/state/slave/heating/active";
        state->availabilityTopic = "~/state/slave/connected";
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "running";
        state->icon = "mdi:radiator";
        state->enabledByDefault = true;
        state->expireAfter = stateExpiration;
        state->payloadAvailable = S_TRUE;
        state->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_FLAME: {
      auto state = std::make_unique<HA::State>("flame", "Flame");
      if (!remove) {
        state->stateTopic = "~/state/slave/flame";
        state->availabilityTopic = "~/state/slave/connected";
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "running";
        state->icon = "mdi:gas-burner";
        state->enabledByDefault = true;
        state->expireAfter = stateExpiration;
        state->payloadAvailable = S_TRUE;
        state->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_FAULT: {
      auto state = std::make_unique<HA::State>("fault", "Fault");
      if (!remove) {
        state->stateTopic = "~/state/slave/fault/active";
        state->availabilityTopic = "~/state/slave/connected";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "problem";
        state->icon = "mdi:alert-remove-outline";
        state->enabledByDefault = true;
        state->expireAfter = stateExpiration;
        state->payloadAvailable = S_TRUE;
        state->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_DIAGNOSTIC: {
      auto state = std::make_unique<HA::State>("diagnostic", "Diagnostic");
      if (!remove) {
        state->stateTopic = "~/state/slave/diag/active";
        state->availabilityTopic = "~/state/slave/connected";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "problem";
        state->icon = "mdi:account-wrench";
        state->enabledByDefault = true;
        state->expireAfter = stateExpiration;
        state->payloadAvailable = S_TRUE;
        state->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_EXTERNAL_PUMP: {
      auto state = std::make_unique<HA::State>("ext_pump", "External pump");
      if (!remove) {
        state->stateTopic = "~/state/master/ext_pump/state";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "running";
        state->icon = "mdi:pump";
        state->enabledByDefault = false;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_FAULT_CODE: {
      auto value = std::make_unique<HA::Value>("fault_code", "Fault code");
      if (!remove) {
        value->stateTopic = "~/state/slave/fault/code";
        value->availabilityTopic = "~/state/slave/fault/available";
        value->category = HA::Category::DIAGNOSTIC;
        value->icon = "mdi:cog-box";
        value->valueTemplate = "{{ '%02d (0x%02X)' | format(value | int, value | int) }}";
        value->enabledByDefault = true;
        value->expireAfter = stateExpiration;
        value->payloadAvailable = S_TRUE;
        value->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(value);
      break;
    }

    case HA_ENTITY_DIAGNOSTIC_CODE: {
      auto value = std::make_unique<HA::Value>("diagnostic_code", "Diagnostic code");
      if (!remove) {
        value->stateTopic = "~/state/slave/diag/code";
        value->availabilityTopic = "~/state/slave/diag/available";
        value->category = HA::Category::DIAGNOSTIC;
        value->icon = "mdi:information-box";
        value->valueTemplate = "{{ '%02d (0x%02X)' | format(value | int, value | int) }}";
        value->enabledByDefault = true;
        value->expireAfter = stateExpiration;
        value->payloadAvailable = S_TRUE;
        value->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(value);
      break;
    }

    case HA_ENTITY_RSSI: {
      auto gauge = std::make_unique<HA::Gauge>("rssi", "RSSI");
      if (!remove) {
        gauge->stateTopic = "~/state/master/network/rssi";
        gauge->availabilityTopic = "~/state/master/network/connection_type";
        gauge->availabilityTemplate = "{{ 'online' if value == 'wifi' else 'offline' }}";
        gauge->category = HA::Category::DIAGNOSTIC;
        gauge->deviceClass = "signal_strength";
        gauge->icon = "mdi:signal";
        gauge->unit = "dBm";
        gauge->enabledByDefault = false;
        gauge->expireAfter = stateExpiration;
      }
      entity = std::move(gauge);
      break;
    }

    case HA_ENTITY_UPTIME: {
      auto counter = std::make_unique<HA::Counter>("uptime", "Uptime");
      if (!remove) {
        counter->stateTopic = "~/state/master/uptime";
        counter->category = HA::Category::DIAGNOSTIC;
        counter->deviceClass = "duration";
        counter->icon = "mdi:clock-start";
        counter->unit = "s";
        counter->enabledByDefault = false;
        counter->expireAfter = stateExpiration;
      }
      entity = std::move(counter);
      break;
    }

    case HA_ENTITY_RESTART: {
      auto button = std::make_unique<HA::Button>("restart", "Restart");
      if (!remove) {
        button->commandTopic = "~/actions/restart/set";
        button->category = HA::Category::CONFIG;
        button->deviceClass = "restart";
        button->payloadPress = S_TRUE;
        button->enabledByDefault = false;
      }
      entity = std::move(button);
      break;
    }

    case HA_ENTITY_RESET_FAULT: {
      auto button = std::make_unique<HA::Button>("reset_fault", "Reset fault");
      if (!remove) {
        button->commandTopic = "~/actions/reset_fault/set";
        button->availabilityTopic = "~/state/slave/fault/active";
        button->category = HA::Category::CONFIG;
        button->deviceClass = "restart";
        button->payloadPress = S_TRUE;
        button->enabledByDefault = true;
        button->payloadAvailable = S_TRUE;
        button->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(button);
      break;
    }

    case HA_ENTITY_RESET_DIAGNOSTIC: {
      auto button = std::make_unique<HA::Button>("reset_diagnostic", "Reset diagnostic");
      if (!remove) {
        button->commandTopic = "~/actions/reset_diagnostic/set";
        button->availabilityTopic = "~/state/slave/diag/active";
        button->category = HA::Category::CONFIG;
        button->deviceClass = "restart";
        button->payloadPress = S_TRUE;
        button->enabledByDefault = true;
        button->payloadAvailable = S_TRUE;
        button->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(button);
      break;
    }

    case HA_ENTITY_HEATING_CLIMATE: {
      auto climate = std::make_unique<HA::Climate>("heating", "Heating");
      if (!remove) {
        climate->icon = "mdi:radiator";
        climate->enabledByDefault = true;
        climate->currentTemperatureTopic = vars.master.heating.smartControl
          ? "~/state/master/heating/indoor_temp"
          : "~/state/master/heating/current_temp";
        climate->temperatureCommandTopic = "~/settings/heating/target/set";
        climate->temperatureStateTopic = "~/settings/heating/target";
        climate->modeCommandTopic = "~/settings/heating/enabled/set";
        climate->modeCommandTemplate = "{% if value == 'heat' %}true{% elif value == 'off' %}false{% endif %}";
        climate->modeStateTopic = "~/settings/heating/enabled";
        climate->modeStateTemplate = "{{ 'heat' if value == 'true' else 'off' }}";
        climate->modes = {"off", "heat"};
        climate->actionTopic = "~/state/slave/heating/active";
        climate->actionTemplate = "{{ 'heating' if value == 'true' else 'idle' }}";
        climate->presetModeCommandTopic = "~/settings/heating/turbo/set";
        climate->presetModeCommandTemplate = "{% if value == 'boost' %}true{% elif value == 'none' %}false{% endif %}";
        climate->presetModeStateTopic = "~/settings/heating/turbo";
        climate->presetModeValueTemplate = "{{ 'boost' if value == 'true' else 'none' }}";
        // Home Assistant adds the reserved reset preset "none" itself.
        climate->presetModes = {"boost"};
        if (unit == UnitSystem::METRIC) {
          climate->temperatureUnit = "C";

        } else if (unit == UnitSystem::IMPERIAL) {
          climate->temperatureUnit = "F";
        }
        climate->minTemp = convertTemp(vars.master.heating.minTemp, UnitSystem::METRIC, unit);
        climate->maxTemp = convertTemp(vars.master.heating.maxTemp, UnitSystem::METRIC, unit);
        climate->tempStep = 0.1f;
      }
      entity = std::move(climate);
      break;
    }

    case HA_ENTITY_DHW_MIN_TEMP: {
      remove = remove || !settings.opentherm.options.dhwSupport;
      auto number = std::make_unique<HA::Number>("dhw_min_temp", "DHW min temp");
      if (!remove) {
        number->stateTopic = "~/settings/dhw/min_temp";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = convertTemp(vars.slave.dhw.minTemp, UnitSystem::METRIC, unit);
        number->max = convertTemp(vars.slave.dhw.maxTemp - 1.0f, UnitSystem::METRIC, unit);
        if (unit == UnitSystem::METRIC) {
          number->unit = "°C";
          number->step = 1.0f;

        } else if (unit == UnitSystem::IMPERIAL) {
          number->unit = "°F";
          number->step = 0.1f;
        }
        number->icon = "mdi:thermometer-chevron-down";
        number->enabledByDefault = true;
        number->deviceClass = S_TEMPERATURE;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_DHW_MAX_TEMP: {
      remove = remove || !settings.opentherm.options.dhwSupport;
      auto number = std::make_unique<HA::Number>("dhw_max_temp", "DHW max temp");
      if (!remove) {
        number->stateTopic = "~/settings/dhw/max_temp";
        number->commandTopic = number->stateTopic + "/set";
        number->category = HA::Category::CONFIG;
        number->mode = HA::NumberMode::BOX;
        number->min = convertTemp(vars.slave.dhw.minTemp + 1.0f, UnitSystem::METRIC, unit);
        number->max = convertTemp(vars.slave.dhw.maxTemp, UnitSystem::METRIC, unit);
        if (unit == UnitSystem::METRIC) {
          number->unit = "°C";
          number->step = 1.0f;

        } else if (unit == UnitSystem::IMPERIAL) {
          number->unit = "°F";
          number->step = 0.1f;
        }
        number->icon = "mdi:thermometer-chevron-up";
        number->enabledByDefault = true;
        number->deviceClass = S_TEMPERATURE;
      }
      entity = std::move(number);
      break;
    }

    case HA_ENTITY_DHW: {
      remove = remove || !settings.opentherm.options.dhwSupport;
      auto state = std::make_unique<HA::State>("dhw", "DHW");
      if (!remove) {
        state->stateTopic = "~/state/slave/dhw/active";
        state->availabilityTopic = "~/state/slave/connected";
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "running";
        state->icon = "mdi:faucet";
        state->enabledByDefault = true;
        state->expireAfter = stateExpiration;
        state->payloadAvailable = S_TRUE;
        state->payloadNotAvailable = S_FALSE;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_DHW_CLIMATE: {
      remove = remove || !settings.opentherm.options.dhwSupport;
      auto climate = std::make_unique<HA::Climate>("dhw", "DHW");
      if (!remove) {
        climate->icon = "mdi:faucet";
        climate->enabledByDefault = true;
        climate->currentTemperatureTopic = "~/state/master/dhw/current_temp";
        climate->temperatureCommandTopic = "~/settings/dhw/target/set";
        climate->temperatureStateTopic = "~/settings/dhw/target";
        climate->modeCommandTopic = "~/settings/dhw/enabled/set";
        climate->modeCommandTemplate = "{% if value == 'heat' %}true{% elif value == 'off' %}false{% endif %}";
        climate->modeStateTopic = "~/settings/dhw/enabled";
        climate->modeStateTemplate = "{{ 'heat' if value == 'true' else 'off' }}";
        climate->modes = {"off", "heat"};
        climate->actionTopic = "~/state/slave/dhw/active";
        climate->actionTemplate = "{{ 'heating' if value == 'true' else 'idle' }}";
        if (unit == UnitSystem::METRIC) {
          climate->temperatureUnit = "C";

        } else if (unit == UnitSystem::IMPERIAL) {
          climate->temperatureUnit = "F";
        }
        climate->minTemp = convertTemp(settings.dhw.minTemp, UnitSystem::METRIC, unit);
        climate->maxTemp = convertTemp(settings.dhw.maxTemp, UnitSystem::METRIC, unit);
        climate->tempStep = 0.1f;
      }
      entity = std::move(climate);
      break;
    }

    case HA_ENTITY_HEATING_BLOCKING: {
      auto state = std::make_unique<HA::State>("heating_blocking", "Heating blocking");
      if (!remove) {
        state->stateTopic = "~/state/master/heating/blocking";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->icon = "mdi:radiator-off";
        state->enabledByDefault = false;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_HEATING_OVERHEAT: {
      auto state = std::make_unique<HA::State>("heating_overheat", "Heating overheat");
      if (!remove) {
        state->stateTopic = "~/state/master/heating/overheat";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "problem";
        state->icon = "mdi:thermometer-alert";
        state->enabledByDefault = false;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_HEATING_FREEZING: {
      auto state = std::make_unique<HA::State>("heating_freezing", "Heating freezing");
      if (!remove) {
        state->stateTopic = "~/state/master/heating/freezing";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->icon = "mdi:snowflake";
        state->enabledByDefault = false;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_DHW_OVERHEAT: {
      remove = remove || !settings.opentherm.options.dhwSupport;
      auto state = std::make_unique<HA::State>("dhw_overheat", "DHW overheat");
      if (!remove) {
        state->stateTopic = "~/state/master/dhw/overheat";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->deviceClass = "problem";
        state->icon = "mdi:thermometer-alert";
        state->enabledByDefault = false;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_CASCADE_CONTROL_INPUT: {
      auto state = std::make_unique<HA::State>("cascade_control_input", "Cascade control input");
      if (!remove) {
        state->stateTopic = "~/state/master/cascade_control/input";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->icon = "mdi:import";
        state->enabledByDefault = false;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    case HA_ENTITY_CASCADE_CONTROL_OUTPUT: {
      auto state = std::make_unique<HA::State>("cascade_control_output", "Cascade control output");
      if (!remove) {
        state->stateTopic = "~/state/master/cascade_control/output";
        state->category = HA::Category::DIAGNOSTIC;
        state->payloadOn = S_TRUE;
        state->payloadOff = S_FALSE;
        state->icon = "mdi:export";
        state->enabledByDefault = false;
        state->expireAfter = stateExpiration;
      }
      entity = std::move(state);
      break;
    }

    default: {
      return false;
    }
  }

  if (remove) {
    return haDiscovery.unpublish(std::move(entity));

  } else {
    return haDiscovery.publish(std::move(entity));
  }
}

static std::string makeDynamicSensorId(const Sensors::Settings& sensor, Sensors::ValueType valueType) {
  std::string id = sensor.slug;
  if (id.empty()) {
    return {};
  }
  const bool multiple = sensor.type == Sensors::Type::BLUETOOTH || sensor.type == Sensors::Type::DHT11 || sensor.type == Sensors::Type::DHT22;
  if (!multiple) {
    return valueType == Sensors::ValueType::PRIMARY ? id : std::string();
  }

  switch (valueType) {
    case Sensors::ValueType::TEMPERATURE:
      return id + "_temp";
    case Sensors::ValueType::HUMIDITY:
      return id + "_humidity";
    case Sensors::ValueType::BATTERY:
      return id + "_battery";
    case Sensors::ValueType::RSSI:
      return id + "_rssi";
    default:
      return {};
  }
}

static const char* getDynamicSensorIcon(Sensors::Purpose purpose) {
  switch (purpose) {
    case Sensors::Purpose::OUTDOOR_TEMP:
      return "mdi:home-thermometer-outline";
    case Sensors::Purpose::INDOOR_TEMP:
      return "mdi:home-thermometer";
    case Sensors::Purpose::HEATING_TEMP:
      return "mdi:radiator";
    case Sensors::Purpose::HEATING_RETURN_TEMP:
    case Sensors::Purpose::DHW_RETURN_TEMP:
      return "mdi:heating-coil";
    case Sensors::Purpose::DHW_TEMP:
    case Sensors::Purpose::DHW_FLOW_RATE:
      return "mdi:faucet";
    case Sensors::Purpose::EXHAUST_TEMP:
      return "mdi:smoke";
    case Sensors::Purpose::TEMPERATURE:
      return "mdi:thermometer-lines";
    case Sensors::Purpose::MODULATION_LEVEL:
      return "mdi:fire-circle";
    case Sensors::Purpose::POWER_FACTOR:
    case Sensors::Purpose::POWER:
      return "mdi:chart-bar";
    case Sensors::Purpose::FAN_SPEED:
      return "mdi:fan";
    case Sensors::Purpose::PRESSURE:
      return "mdi:gauge";
    case Sensors::Purpose::HUMIDITY:
      return "mdi:water-percent";
    default:
      return nullptr;
  }
}

void publishDynamicSensor(const Sensors::Settings& sensor, Sensors::ValueType valueType, bool enabledByDefault) {
  #if !USE_BLE
  if (sensor.type == Sensors::Type::BLUETOOTH) {
    return;
  }
  #endif

  auto id = makeDynamicSensorId(sensor, valueType);
  if (id.empty()) {
    return;
  }

  const UnitSystem unit = settings.system.unitSystem;
  auto entity = std::make_unique<HA::Gauge>(std::move(id), sensor.name);
  entity->icon = getDynamicSensorIcon(sensor.purpose);
  entity->availabilityTopic = mqttTopic(S_SENSORS, sensor.slug, S_CONNECTED);
  entity->payloadAvailable = S_TRUE;
  entity->payloadNotAvailable = S_FALSE;
  entity->expireAfter = 2u * MQTT_SENSOR_REPUBLISH_INTERVAL / 1000u;
  entity->enabledByDefault = enabledByDefault;

  switch (sensor.purpose) {
    case Sensors::Purpose::OUTDOOR_TEMP:
    case Sensors::Purpose::INDOOR_TEMP:
    case Sensors::Purpose::HEATING_TEMP:
    case Sensors::Purpose::HEATING_RETURN_TEMP:
    case Sensors::Purpose::DHW_TEMP:
    case Sensors::Purpose::DHW_RETURN_TEMP:
    case Sensors::Purpose::EXHAUST_TEMP:
    case Sensors::Purpose::TEMPERATURE:
      entity->deviceClass = S_TEMPERATURE;
      if (unit == UnitSystem::METRIC) {
        entity->unit = "°C";

      } else if (unit == UnitSystem::IMPERIAL) {
        entity->unit = "°F";
      }
      break;

    case Sensors::Purpose::DHW_FLOW_RATE:
      entity->deviceClass = "volume_flow_rate";
      if (unit == UnitSystem::METRIC) {
        entity->unit = "L/min";

      } else if (unit == UnitSystem::IMPERIAL) {
        entity->unit = "gal/min";
      }
      break;

    case Sensors::Purpose::MODULATION_LEVEL:
    case Sensors::Purpose::POWER_FACTOR:
      entity->deviceClass = "power_factor";
      entity->unit = "%";
      break;

    case Sensors::Purpose::POWER:
      entity->deviceClass = "power";
      entity->unit = "kW";
      break;

    case Sensors::Purpose::FAN_SPEED:
      entity->unit = "RPM";
      break;

    case Sensors::Purpose::CO2:
      entity->deviceClass = "carbon_dioxide";
      entity->unit = "ppm";
      break;

    case Sensors::Purpose::PRESSURE:
      entity->deviceClass = "pressure";
      if (unit == UnitSystem::METRIC) {
        entity->unit = "bar";

      } else if (unit == UnitSystem::IMPERIAL) {
        entity->unit = "psi";
      }
      break;

    case Sensors::Purpose::HUMIDITY:
      entity->deviceClass = S_HUMIDITY;
      entity->unit = "%";
      break;

    default:
      break;
  }

  if (sensor.type == Sensors::Type::BLUETOOTH || sensor.type == Sensors::Type::DHT11 || sensor.type == Sensors::Type::DHT22) {
    switch (valueType) {
      case Sensors::ValueType::TEMPERATURE:
        entity->name += " ";
        entity->name += S_TEMPERATURE;
        entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_TEMPERATURE);
        entity->deviceClass = S_TEMPERATURE;
        if (unit == UnitSystem::METRIC) {
          entity->unit = "°C";

        } else if (unit == UnitSystem::IMPERIAL) {
          entity->unit = "°F";
        }
        break;

      case Sensors::ValueType::HUMIDITY:
        entity->name += " ";
        entity->name += S_HUMIDITY;
        entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_HUMIDITY);
        entity->deviceClass = S_HUMIDITY;
        entity->unit = "%";
        break;

      case Sensors::ValueType::BATTERY:
        entity->name += " ";
        entity->name += S_BATTERY;
        entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_BATTERY);
        entity->deviceClass = S_BATTERY;
        entity->unit = "%";
        entity->category = HA::Category::DIAGNOSTIC;
        break;

      case Sensors::ValueType::RSSI:
        entity->name += " ";
        entity->name += S_RSSI;
        entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_RSSI);
        entity->deviceClass = "signal_strength";
        entity->unit = "dBm";
        entity->category = HA::Category::DIAGNOSTIC;
        break;

      default:
        return;
    }

  } else {
    entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_VALUE);
  }

  haDiscovery.publish(std::move(entity));
}

void publishDynamicManualSensor(const Sensors::Settings& sensor, bool enabledByDefault) {
  auto id = makeDynamicSensorId(sensor, Sensors::ValueType::PRIMARY);
  if (id.empty()) {
    return;
  }

  const UnitSystem unit = settings.system.unitSystem;
  auto entity = std::make_unique<HA::Number>(std::move(id), sensor.name);
  entity->icon = getDynamicSensorIcon(sensor.purpose);
  entity->category = HA::Category::CONFIG;
  entity->enabledByDefault = enabledByDefault;
  entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_VALUE);
  entity->commandTopic = entity->stateTopic + "/set";
  entity->mode = HA::NumberMode::BOX;
  entity->step = 0.01f;
  entity->min.reset();
  entity->max.reset();

  switch (sensor.purpose) {
    case Sensors::Purpose::OUTDOOR_TEMP:
    case Sensors::Purpose::INDOOR_TEMP:
    case Sensors::Purpose::HEATING_TEMP:
    case Sensors::Purpose::HEATING_RETURN_TEMP:
    case Sensors::Purpose::DHW_TEMP:
    case Sensors::Purpose::DHW_RETURN_TEMP:
    case Sensors::Purpose::EXHAUST_TEMP:
    case Sensors::Purpose::TEMPERATURE:
      entity->deviceClass = S_TEMPERATURE;
      if (unit == UnitSystem::METRIC) {
        entity->unit = "°C";
        entity->min = -99;
        entity->max = 99;

      } else if (unit == UnitSystem::IMPERIAL) {
        entity->unit = "°F";
        entity->min = -147;
        entity->max = 211;
      }
      break;

    case Sensors::Purpose::DHW_FLOW_RATE:
      entity->deviceClass = "volume_flow_rate";
      if (unit == UnitSystem::METRIC) {
        entity->unit = "L/min";

      } else if (unit == UnitSystem::IMPERIAL) {
        entity->unit = "gal/min";
      }
      break;

    case Sensors::Purpose::MODULATION_LEVEL:
    case Sensors::Purpose::POWER_FACTOR:
      entity->deviceClass = "power_factor";
      entity->unit = "%";
      break;

    case Sensors::Purpose::POWER:
      entity->deviceClass = "power";
      entity->unit = "kW";
      break;

    case Sensors::Purpose::FAN_SPEED:
      entity->unit = "RPM";
      break;

    case Sensors::Purpose::CO2:
      entity->deviceClass = "carbon_dioxide";
      entity->unit = "ppm";
      break;

    case Sensors::Purpose::PRESSURE:
      entity->deviceClass = "pressure";
      if (unit == UnitSystem::METRIC) {
        entity->unit = "bar";

      } else if (unit == UnitSystem::IMPERIAL) {
        entity->unit = "psi";
      }
      break;

    case Sensors::Purpose::HUMIDITY:
      entity->deviceClass = S_HUMIDITY;
      entity->unit = "%";
      break;

    default:
      break;
  }

  haDiscovery.publish(std::move(entity));
}

void deleteDynamicSensor(const Sensors::Settings& sensor, Sensors::ValueType valueType) {
  auto id = makeDynamicSensorId(sensor, valueType);
  if (id.empty()) {
    return;
  }

  if (sensor.type == Sensors::Type::MANUAL) {
    haDiscovery.unpublish<HA::Number>(std::move(id), "");
    return;
  }

  haDiscovery.unpublish<HA::Gauge>(std::move(id), "");
}

void publishConnectionDynamicSensor(const Sensors::Settings& sensor, bool enabledByDefault) {
  auto entity = std::make_unique<HA::State>(
    std::string(sensor.slug) + "_connected", std::string(sensor.name) + " connected"
  );
  entity->category = HA::Category::DIAGNOSTIC;
  entity->deviceClass = "connectivity";
  entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_CONNECTED);
  entity->payloadOn = S_TRUE;
  entity->payloadOff = S_FALSE;
  entity->enabledByDefault = enabledByDefault;
  entity->expireAfter = 2u * MQTT_SENSOR_REPUBLISH_INTERVAL / 1000u;
  haDiscovery.publish(std::move(entity));
}

void deleteConnectionDynamicSensor(const Sensors::Settings& sensor) {
  haDiscovery.unpublish<HA::State>(std::string(sensor.slug) + "_connected", "");
}

void publishSignalQualityDynamicSensor(const Sensors::Settings& sensor, bool enabledByDefault) {
  auto entity = std::make_unique<HA::Gauge>(
    std::string(sensor.slug) + "_signal_quality", std::string(sensor.name) + " signal quality"
  );
  entity->category = HA::Category::DIAGNOSTIC;
  entity->stateTopic = mqttTopic(S_SENSORS, sensor.slug, S_SIGNAL_QUALITY);
  entity->unit = "%";
  entity->icon = "mdi:signal";
  entity->enabledByDefault = enabledByDefault;
  entity->expireAfter = 2u * MQTT_SENSOR_REPUBLISH_INTERVAL / 1000u;
  haDiscovery.publish(std::move(entity));
}

void deleteSignalQualityDynamicSensor(const Sensors::Settings& sensor) {
  haDiscovery.unpublish<HA::Gauge>(std::string(sensor.slug) + "_signal_quality", "");
}
