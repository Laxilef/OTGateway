#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>

#include <MycilaMQTT.h>
#include <algorithm>
#include <atomic>
#include <bitset>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <list>
#include <optional>
#include <string>
#include <string_view>

std::mutex mqttMutex;

static MQTT* mqttClient = nullptr;

// Keep the applied prefix separate from settings until MQTT is reconfigured.
static std::string mqttPrefix;  // NOLINT(runtime/string)
static std::atomic<unsigned long> mqttConnectedAt{0};
static unsigned long mqttPublishedSensorAt[SENSORS_AMOUNT] = {};

static std::atomic<bool> mqttHaRefreshRequested{false};
// Discovery and its synchronous publisher run only on Core.
static std::bitset<HA_ENTITY_COUNT> mqttHaPendingEntities;
static std::bitset<SENSORS_AMOUNT> mqttHaPendingSensors;

Task* mqttPublishSettingsTask = nullptr;
Task* mqttPublishStateTask = nullptr;
Task* mqttPublishSensorsTask = nullptr;
static Task* mqttReconfigureSensorsTask = nullptr;
static Task* mqttPublishHaTask = nullptr;

// Protect the MQTT change queue and publication timestamps across HTTP/native callbacks.
static std::mutex mqttSensorsMutex;

struct MqttSensorChange {
  uint8_t sensorId;
  std::optional<Sensors::Settings> previousSettings;
};
static std::list<MqttSensorChange> mqttSensorChanges;

// The caller holds mqttSensorsMutex.
static bool isSensorReconfigurationPending(uint8_t sensorId) {
  return std::any_of(mqttSensorChanges.begin(), mqttSensorChanges.end(), [sensorId](const auto& change) {
    return change.sensorId == sensorId;
  });
}

static bool isMqttBlocked() {
  return vars.states.upgrading || restartTask.scheduled() || restartTask.active();
}

static bool isMqttReady() {
  return settings.mqtt.enabled && isNetworkConnected() && !isMqttBlocked()
    && !mqttConfigureTask.scheduled() && isMqttConnected();
}

static bool isMqttReadyForPublish() {
  const unsigned long connectedAt = mqttConnectedAt.load();
  return isMqttReady() && connectedAt && millis() - connectedAt > 30000u;
}

std::string mqttTopic(std::initializer_list<std::string_view> parts) {
  size_t length = mqttPrefix.size();
  for (const auto part : parts) {
    length += 1 + part.size();
  }

  std::string topic;
  topic.reserve(length);
  topic.append(mqttPrefix);
  for (const auto part : parts) {
    topic += '/';
    topic.append(part);
  }
  return topic;
}

static bool publishValue(const std::string& topic, std::string_view value) {
  return mqttClient->publish(topic.c_str(), value, true);
}

static bool publishValue(const std::string& topic, const char* value) {
  return publishValue(topic, std::string_view(value));
}

static bool publishValue(const std::string& topic, bool value) {
  return publishValue(topic, value ? S_TRUE : S_FALSE);
}

static bool publishValue(const std::string& topic, float value, uint8_t decimals = 2) {
  if (!std::isfinite(value)) {
    // Remove an obsolete retained reading instead of publishing nan/inf.
    return publishValue(topic, "");
  }

  char payload[64];
  const int length = snprintf(payload, sizeof(payload), "%.*f", decimals, value);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(payload)) {
    return false;
  }

  if (decimals) {
    char* end = payload + length - 1;
    while (*end == '0') {
      *end-- = '\0';
    }
    if (*end == '.') {
      *end = '\0';
    }
  }

  return publishValue(topic, payload);
}

static void publishHeatingSettings() {
  publishValue(mqttTopic("settings/heating/enabled"), settings.heating.enabled);
  publishValue(mqttTopic("settings/heating/turbo"), settings.heating.turbo);
  publishValue(mqttTopic("settings/heating/target"), convertTemp(settings.heating.target, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("settings/heating/hysteresis/enabled"), settings.heating.hysteresis.enabled);
  publishValue(mqttTopic("settings/heating/hysteresis/value"), settings.heating.hysteresis.value, 3);
  publishValue(mqttTopic("settings/heating/turbo_factor"), settings.heating.turboFactor, 3);
  publishValue(mqttTopic("settings/heating/min_temp"), convertTemp(settings.heating.minTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("settings/heating/max_temp"), convertTemp(settings.heating.maxTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("settings/heating/max_modulation"), std::to_string(settings.heating.maxModulation));
  publishValue(mqttTopic("settings/heating/indoor_temp_avg_type"), std::to_string(static_cast<uint8_t>(settings.heating.indoorTempAvgType)));
  publishValue(mqttTopic("settings/heating/outdoor_temp_avg_type"), std::to_string(static_cast<uint8_t>(settings.heating.outdoorTempAvgType)));
  publishValue(
    mqttTopic("settings/heating/overheat_protection/high_temp"),
    settings.heating.overheatProtection.highTemp == 0
      ? 0
      : convertTemp(settings.heating.overheatProtection.highTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(
    mqttTopic("settings/heating/overheat_protection/low_temp"),
    settings.heating.overheatProtection.lowTemp == 0
      ? 0
      : convertTemp(settings.heating.overheatProtection.lowTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(mqttTopic("settings/heating/freeze_protection/high_temp"), convertTemp(settings.heating.freezeProtection.highTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("settings/heating/freeze_protection/low_temp"), convertTemp(settings.heating.freezeProtection.lowTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
}

static void publishDhwSettings() {
  publishValue(mqttTopic("settings/dhw/enabled"), settings.dhw.enabled);
  publishValue(mqttTopic("settings/dhw/target"), convertTemp(settings.dhw.target, UnitSystem::METRIC, settings.system.unitSystem), 1);
  publishValue(mqttTopic("settings/dhw/min_temp"), convertTemp(settings.dhw.minTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("settings/dhw/max_temp"), convertTemp(settings.dhw.maxTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("settings/dhw/max_modulation"), std::to_string(settings.dhw.maxModulation));
  publishValue(
    mqttTopic("settings/dhw/overheat_protection/high_temp"),
    settings.dhw.overheatProtection.highTemp == 0
      ? 0
      : convertTemp(settings.dhw.overheatProtection.highTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(
    mqttTopic("settings/dhw/overheat_protection/low_temp"),
    settings.dhw.overheatProtection.lowTemp == 0
      ? 0
      : convertTemp(settings.dhw.overheatProtection.lowTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
}

static void publishPidSettings() {
  publishValue(mqttTopic("settings/pid/enabled"), settings.pid.enabled);
  publishValue(mqttTopic("settings/pid/p_factor"), settings.pid.p_factor, 3);
  publishValue(mqttTopic("settings/pid/i_factor"), settings.pid.i_factor, 4);
  publishValue(mqttTopic("settings/pid/d_factor"), settings.pid.d_factor, 1);
  publishValue(mqttTopic("settings/pid/dt"), std::to_string(settings.pid.dt));
  publishValue(
    mqttTopic("settings/pid/min_temp"),
    settings.equitherm.enabled
      ? settings.pid.minTemp
      : convertTemp(settings.pid.minTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(
    mqttTopic("settings/pid/max_temp"),
    settings.equitherm.enabled
      ? settings.pid.maxTemp
      : convertTemp(settings.pid.maxTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(mqttTopic("settings/pid/deadband/enabled"), settings.pid.deadband.enabled);
  publishValue(mqttTopic("settings/pid/deadband/p_multiplier"), settings.pid.deadband.p_multiplier, 3);
  publishValue(mqttTopic("settings/pid/deadband/i_multiplier"), settings.pid.deadband.i_multiplier, 3);
  publishValue(mqttTopic("settings/pid/deadband/d_multiplier"), settings.pid.deadband.d_multiplier, 3);
  publishValue(mqttTopic("settings/pid/deadband/threshold_high"), settings.pid.deadband.thresholdHigh);
  publishValue(mqttTopic("settings/pid/deadband/threshold_low"), settings.pid.deadband.thresholdLow);
}

static void publishEquithermSettings() {
  publishValue(mqttTopic("settings/equitherm/enabled"), settings.equitherm.enabled);
  publishValue(mqttTopic("settings/equitherm/slope"), settings.equitherm.slope, 3);
  publishValue(mqttTopic("settings/equitherm/exponent"), settings.equitherm.exponent, 3);
  publishValue(mqttTopic("settings/equitherm/shift"), settings.equitherm.shift, 2);
  publishValue(mqttTopic("settings/equitherm/target_diff_factor"), settings.equitherm.targetDiffFactor, 3);
}

static void publishSettings() {
  publishValue(mqttTopic("settings/emergency/target"), convertTemp(settings.emergency.target, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishHeatingSettings();
  publishDhwSettings();
  publishPidSettings();
  publishEquithermSettings();
}

static void publishSlaveState() {
  publishValue(mqttTopic("state/slave/member_id"), std::to_string(vars.slave.memberId));
  publishValue(mqttTopic("state/slave/flags"), std::to_string(vars.slave.flags));
  publishValue(mqttTopic("state/slave/type"), std::to_string(vars.slave.type));
  publishValue(mqttTopic("state/slave/app_version"), std::to_string(vars.slave.appVersion));
  publishValue(mqttTopic("state/slave/protocol_version"), vars.slave.protocolVersion);
  publishValue(mqttTopic("state/slave/connected"), vars.slave.connected);
  publishValue(mqttTopic("state/slave/flame"), vars.slave.flame);
  publishValue(mqttTopic("state/slave/cooling/active"), vars.slave.cooling.active);
  publishValue(mqttTopic("state/slave/cooling/setpoint"), std::to_string(vars.slave.cooling.setpoint));
  publishValue(mqttTopic("state/slave/modulation/min"), std::to_string(vars.slave.modulation.min));
  publishValue(mqttTopic("state/slave/modulation/max"), std::to_string(vars.slave.modulation.max));
  publishValue(mqttTopic("state/slave/power/min"), vars.slave.power.min, 2);
  publishValue(mqttTopic("state/slave/power/max"), vars.slave.power.max, 2);
  publishValue(mqttTopic("state/slave/heating/active"), vars.slave.heating.active);
  publishValue(mqttTopic("state/slave/heating/min_temp"), convertTemp(vars.slave.heating.minTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/slave/heating/max_temp"), convertTemp(vars.slave.heating.maxTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/slave/dhw/active"), vars.slave.dhw.active);
  publishValue(mqttTopic("state/slave/dhw/min_temp"), convertTemp(vars.slave.dhw.minTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/slave/dhw/max_temp"), convertTemp(vars.slave.dhw.maxTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/slave/fault/active"), vars.slave.fault.active);
  publishValue(mqttTopic("state/slave/fault/code"), std::to_string(vars.slave.fault.code));
  publishValue(mqttTopic("state/slave/fault/available"), vars.slave.connected && vars.slave.fault.active);
  publishValue(mqttTopic("state/slave/diag/active"), vars.slave.diag.active);
  publishValue(mqttTopic("state/slave/diag/code"), std::to_string(vars.slave.diag.code));
  // Preserve the diagnostic-code availability condition from async.
  publishValue(mqttTopic("state/slave/diag/available"), (vars.slave.connected && vars.slave.fault.active) || vars.slave.diag.active);
}

static void publishMasterState() {
  publishValue(mqttTopic("state/master/heating/enabled"), vars.master.heating.enabled);
  publishValue(mqttTopic("state/master/heating/blocking"), vars.master.heating.blocking);
  publishValue(mqttTopic("state/master/heating/smart_control"), vars.master.heating.smartControl);
  publishValue(mqttTopic("state/master/heating/overheat"), vars.master.heating.overheat);
  publishValue(mqttTopic("state/master/heating/freezing"), vars.master.heating.freezing);
  publishValue(
    mqttTopic("state/master/heating/setpoint_temp"),
    vars.master.heating.setpointTemp == 0
      ? 0
      : convertTemp(vars.master.heating.setpointTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(
    mqttTopic("state/master/heating/target_temp"),
    vars.master.heating.targetTemp == 0
      ? 0
      : convertTemp(vars.master.heating.targetTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(mqttTopic("state/master/heating/current_temp"), convertTemp(vars.master.heating.currentTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/heating/return_temp"), convertTemp(vars.master.heating.returnTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/heating/indoor_temp"), convertTemp(vars.master.heating.indoorTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/heating/outdoor_temp"), convertTemp(vars.master.heating.outdoorTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/heating/min_temp"), convertTemp(vars.master.heating.minTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/heating/max_temp"), convertTemp(vars.master.heating.maxTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/dhw/enabled"), vars.master.dhw.enabled);
  publishValue(mqttTopic("state/master/dhw/overheat"), vars.master.dhw.overheat);
  publishValue(
    mqttTopic("state/master/dhw/target_temp"),
    vars.master.dhw.targetTemp == 0
      ? 0
      : convertTemp(vars.master.dhw.targetTemp, UnitSystem::METRIC, settings.system.unitSystem),
    2
  );
  publishValue(mqttTopic("state/master/dhw/current_temp"), convertTemp(vars.master.dhw.currentTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/dhw/return_temp"), convertTemp(vars.master.dhw.returnTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/dhw/min_temp"), convertTemp(settings.dhw.minTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/dhw/max_temp"), convertTemp(settings.dhw.maxTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  publishValue(mqttTopic("state/master/network/connected"), isNetworkConnected());
  publishValue(mqttTopic("state/master/network/rssi"), std::to_string(vars.network.rssi));
  publishValue(mqttTopic("state/master/network/connection_type"), getNetworkConnectionType());
  publishValue(mqttTopic("state/master/emergency/state"), vars.emergency.state);
  publishValue(mqttTopic("state/master/ext_pump/state"), vars.externalPump.state);
  publishValue(mqttTopic("state/master/cascade_control/input"), vars.cascadeControl.input);
  publishValue(mqttTopic("state/master/cascade_control/output"), vars.cascadeControl.output);
  publishValue(mqttTopic("state/master/uptime"), std::to_string(millis() / 1000u));
}

static void publishState() {
  publishValue(mqttTopic("status"), "online");
  publishSlaveState();
  publishMasterState();
}

static std::string_view trimPayload(std::string_view payload) {
  const auto first = payload.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return {};
  }
  return payload.substr(first, payload.find_last_not_of(" \t\r\n") - first + 1);
}

static std::optional<float> parseNumber(std::string_view payload, uint8_t bufferSize = 17) {
  if (payload.empty() || bufferSize < 2) {
    return std::nullopt;
  }

  // Long input is intentionally truncated to leave room for the terminating NUL.
  payload = payload.substr(0, bufferSize - 1);

  // strtof also accepts leading whitespace, '+' and hexadecimal numbers.
  const char first = payload.front();
  if ((first < '0' || first > '9') && first != '-' && first != '.') {
    return std::nullopt;
  }

  if (payload.find_first_of("xX") != std::string_view::npos) {
    return std::nullopt;
  }

  char buffer[bufferSize];
  memcpy(buffer, payload.data(), payload.size());
  buffer[payload.size()] = '\0';

  char* end;
  errno = 0;
  const float parsed = std::strtof(buffer, &end);
  if (end != buffer + payload.size() || !std::isfinite(parsed) || (errno == ERANGE && parsed == 0.0f)) {
    return std::nullopt;
  }

  return parsed;
}

static bool isCommandValid(const std::string& topic, std::string_view payload) {
  if (payload.empty()) {
    return false;
  }

  // These bounds keep commands within RX 512: MycilaMQTT does not expose fragment metadata.
  if (topic.size() > 128 || payload.size() > 64 || topic.find('\0') != std::string::npos) {
    ESP_LOGW(L_MQTT_MSG, "Invalid scalar command size or topic");
    return false;
  }

  return true;
}

static bool publishSensor(const Sensors::Settings& sensor, const Sensors::Result& result) {
  if (!sensor.slug[0]) {
    return false;
  }

  if (!publishValue(mqttTopic(S_SENSORS, sensor.slug, S_CONNECTED), result.connected)) {
    return false;
  }

  if (sensor.type == Sensors::Type::BLUETOOTH || sensor.type == Sensors::Type::DALLAS_TEMP
      || sensor.type == Sensors::Type::DHT11 || sensor.type == Sensors::Type::DHT22) {
    if (!publishValue(mqttTopic(S_SENSORS, sensor.slug, S_SIGNAL_QUALITY), std::to_string(result.signalQuality))) {
      return false;
    }
  }

  if (sensor.type == Sensors::Type::BLUETOOTH || sensor.type == Sensors::Type::DHT11 || sensor.type == Sensors::Type::DHT22) {
    const bool published = publishValue(
      mqttTopic(S_SENSORS, sensor.slug, S_TEMPERATURE),
      convertTemp(
        result.values[static_cast<uint8_t>(Sensors::ValueType::TEMPERATURE)],
        UnitSystem::METRIC, settings.system.unitSystem
      ),
      3
    );

    if (!published) {
      return false;
    }

    if (!publishValue(mqttTopic(S_SENSORS, sensor.slug, S_HUMIDITY), result.values[static_cast<uint8_t>(Sensors::ValueType::HUMIDITY)], 3)) {
      return false;
    }

    if (sensor.type == Sensors::Type::BLUETOOTH) {
      if (!publishValue(mqttTopic(S_SENSORS, sensor.slug, S_BATTERY), result.values[static_cast<uint8_t>(Sensors::ValueType::BATTERY)], 1)) {
        return false;
      }

      if (!publishValue(mqttTopic(S_SENSORS, sensor.slug, S_RSSI), result.values[static_cast<uint8_t>(Sensors::ValueType::RSSI)], 0)) {
        return false;
      }
    }

  } else {
    const bool published = publishValue(
      mqttTopic(S_SENSORS, sensor.slug, S_VALUE),
      Sensors::convertValue(
        sensor, Sensors::ValueType::PRIMARY,
        result.values[static_cast<uint8_t>(Sensors::ValueType::PRIMARY)],
        UnitSystem::METRIC, settings.system.unitSystem
      ),
      3
    );

    if (!published) {
      return false;
    }
  }

  return true;
}

static bool purgeSensor(const char* slug) {
  if (slug == nullptr || !slug[0]) {
    return true;
  }

  const char* fields[] = {S_CONNECTED, S_SIGNAL_QUALITY, S_VALUE, S_TEMPERATURE, S_HUMIDITY, S_BATTERY, S_RSSI};
  for (const auto field : fields) {
    const std::string topic = mqttTopic(S_SENSORS, slug, field);

    if (!mqttClient->publish(topic.c_str(), {}, true)) {
      return false;
    }
  }

  return true;
}

void requestReconfigureMqttSensor(uint8_t sensorId, std::optional<Sensors::Settings> previousSettings) {
  if (!Sensors::isValidSensorId(sensorId)) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mqttSensorsMutex);
    const auto pending = std::find_if(mqttSensorChanges.begin(), mqttSensorChanges.end(), [sensorId](const auto& change) {
      return change.sensorId == sensorId;
    });

    if (pending == mqttSensorChanges.end()) {
      mqttSensorChanges.push_back({sensorId, std::move(previousSettings)});

    } else if (!pending->previousSettings && previousSettings) {
      // Keep the first settings whose old MQTT identity may need cleanup.
      pending->previousSettings = std::move(previousSettings);
    }
  }

  // If SDK stop holds the lock, pending changes stay queued for a periodic run.
  std::unique_lock<std::mutex> taskLock(mqttMutex, std::try_to_lock);
  if (taskLock.owns_lock() && mqttReconfigureSensorsTask) {
    mqttReconfigureSensorsTask->requestEarlyRun();
  }
}

void requestPublishMqttSensor(uint8_t sensorId) {
  if (!Sensors::isValidSensorId(sensorId)) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mqttSensorsMutex);
    mqttPublishedSensorAt[sensorId] = 0;
  }

  // stopMqtt holds this lock while waiting for the MQTT callback to finish.
  // If busy, the cleared timestamp still triggers the next periodic publication.
  std::unique_lock<std::mutex> taskLock(mqttMutex, std::try_to_lock);
  if (taskLock.owns_lock() && mqttPublishSensorsTask) {
    mqttPublishSensorsTask->requestEarlyRun();
  }
}

void requestReconfigureMqtt() {
  mqttConfigureTask.setInterval(0).resume();
}

bool isMqttConnected() {
  // Do not wait for SDK stop or access a client being destroyed.
  std::unique_lock<std::mutex> lock(mqttMutex, std::try_to_lock);
  return lock.owns_lock() && mqttClient && mqttClient->isConnected();
}

void stopMqtt(bool destroy = false) {
  std::lock_guard<std::mutex> lock(mqttMutex);
  if (mqttClient) {
    mqttClient->end();
  }
  mqttConnectedAt.store(0);
  haDiscovery.end();
  mqttHaPendingEntities.reset();
  mqttHaPendingSensors.reset();

  if (!destroy) {
    return;
  }

  if (mqttPublishSettingsTask) {
    coreTaskManager.removeTask(*mqttPublishSettingsTask);
    delete mqttPublishSettingsTask;
    mqttPublishSettingsTask = nullptr;
  }

  if (mqttPublishStateTask) {
    coreTaskManager.removeTask(*mqttPublishStateTask);
    delete mqttPublishStateTask;
    mqttPublishStateTask = nullptr;
  }

  if (mqttPublishSensorsTask) {
    coreTaskManager.removeTask(*mqttPublishSensorsTask);
    delete mqttPublishSensorsTask;
    mqttPublishSensorsTask = nullptr;
  }

  if (mqttReconfigureSensorsTask) {
    coreTaskManager.removeTask(*mqttReconfigureSensorsTask);
    delete mqttReconfigureSensorsTask;
    mqttReconfigureSensorsTask = nullptr;
  }

  if (mqttPublishHaTask) {
    coreTaskManager.removeTask(*mqttPublishHaTask);
    delete mqttPublishHaTask;
    mqttPublishHaTask = nullptr;
  }

  delete mqttClient;
  mqttClient = nullptr;

  std::string().swap(mqttPrefix);
}

static float mqttValueToMetric(std::string_view path, float value) {
  if (settings.equitherm.enabled && path.starts_with("pid/")) {
    return value;
  }

  if (value == 0 && path.find(S_OVERHEAT_PROTECTION) != std::string_view::npos) {
    return value;
  }

  const auto name = path.substr(path.find_last_of('/') + 1);
  if (name == S_TARGET || name == S_MIN_TEMP || name == S_MAX_TEMP
      || name == S_HIGH_TEMP || name == S_LOW_TEMP) {
    return convertTemp(value, settings.system.unitSystem, UnitSystem::METRIC);
  }

  return value;
}

static void subscribe() {
  mqttClient->subscribe("homeassistant/status", [](const std::string&, const std::string_view& message) {
    if (message == "online") {
      mqttHaRefreshRequested.store(true);
    }
  });

  const auto onSettingCommand = [](const std::string& topic, const std::string_view& message) {
    if (isMqttBlocked() || !settings.mqtt.enabled || mqttConfigureTask.scheduled() || mqttConfigureTask.active()) {
      return;
    }

    auto path = std::string_view(topic).substr(mqttPrefix.size());
    if (!path.starts_with("/settings/")) {
      return;
    }

    path.remove_prefix(sizeof("/settings/") - 1);
    if (!path.ends_with("/set")) {
      return;
    }

    path.remove_suffix(sizeof("/set") - 1);
    if (!isCommandValid(topic, message)) {
      return;
    }

    mqttClient->publish(topic.c_str(), {}, true);
    const auto payload = trimPayload(message);

    SettingResult result;
    if (payload == S_TRUE || payload == S_FALSE) {
      result = applySetting(settings, path, payload == S_TRUE);

    } else {
      const auto parsed = parseNumber(payload);
      if (parsed) {
        result = applySetting(settings, path, mqttValueToMetric(path, *parsed));
      }
    }

    if (!result.accepted) {
      ESP_LOGW(L_MQTT_MSG, "Unsupported or invalid setting command");
      if (!result.changed) {
        return;
      }
    }

    result.changed = normalizeSettings(settings) || result.changed;
    if (result.changed) {
      requestSaveSettings();
    }

    mqttPublishSettingsTask->requestEarlyRun();
  };

  mqttClient->subscribe(mqttTopic(S_SETTINGS, "+/+/set"), onSettingCommand);
  mqttClient->subscribe(mqttTopic(S_SETTINGS, "+/+/+/set"), onSettingCommand);

  mqttClient->subscribe(mqttTopic(S_ACTIONS, "+", "set"), [](const std::string& topic, const std::string_view& message) {
    if (isMqttBlocked() || !settings.mqtt.enabled || mqttConfigureTask.scheduled() || mqttConfigureTask.active()) {
      return;
    }

    if (!isCommandValid(topic, message)) {
      return;
    }

    mqttClient->publish(topic.c_str(), {}, true);

    const auto payload = trimPayload(message);
    if (payload != S_TRUE && payload != S_FALSE) {
      ESP_LOGW(L_MQTT_MSG, "Invalid action value");
      return;
    }

    auto action = std::string_view(topic).substr(mqttPrefix.size() + sizeof("/actions/") - 1);
    action.remove_suffix(sizeof("/set") - 1);


    if (!applyAction(action, payload == S_TRUE)) {
      ESP_LOGW(L_MQTT_MSG, "Unsupported or blocked action");
    }
  });

  mqttClient->subscribe(mqttTopic(S_SENSORS, "+", S_VALUE, "set"), [](const std::string& topic, const std::string_view& message) {
    if (isMqttBlocked() || !settings.mqtt.enabled || mqttConfigureTask.scheduled() || mqttConfigureTask.active()) {
      return;
    }

    if (!isCommandValid(topic, message)) {
      return;
    }

    mqttClient->publish(topic.c_str(), {}, true);

    const auto parsed = parseNumber(trimPayload(message));
    if (!parsed) {
      ESP_LOGW(L_MQTT_MSG, "Invalid manual sensor value");
      return;
    }

    const auto slugStart = mqttPrefix.size() + strlen(S_SENSORS) + 2;
    const std::string slug = topic.substr(slugStart, topic.find('/', slugStart) - slugStart);
    if (slug.empty()) {
      return;
    }

    const auto sensorId = Sensors::setManualValueBySlug(slug, *parsed, settings.system.unitSystem);
    if (!sensorId) {
      ESP_LOGW(L_MQTT_MSG, "Unknown or disabled manual sensor");
      return;
    }

    requestPublishMqttSensor(*sensorId);
  });
}

static void connect() {
  if (!mqttClient) {
    std::lock_guard<std::mutex> lock(mqttMutex);

    mqttClient = new MQTT();
    mqttClient->setAsync(true);
    mqttClient->setConfigHook([](esp_mqtt_client_config_t& config) {
      config.session.last_will.qos = 1;
      config.session.disable_clean_session = false;
      config.network.timeout_ms = 3000;
      config.network.reconnect_timeout_ms = MQTT_RECONNECT_INTERVAL;
      config.buffer.size = 512;
      config.buffer.out_size = 512;
      config.outbox.limit = 8192;
      config.task.stack_size = 4096;
      config.task.priority = 2;
    });

    mqttClient->onConnect([] {
      mqttConnectedAt.store(millis());
      mqttHaRefreshRequested.store(true);
      {
        std::lock_guard<std::mutex> lock(mqttSensorsMutex);
        memset(mqttPublishedSensorAt, 0, sizeof(mqttPublishedSensorAt));
      }
      mqttPublishSettingsTask->requestEarlyRun();
      mqttPublishStateTask->requestEarlyRun();
      mqttPublishSensorsTask->requestEarlyRun();
      mqttReconfigureSensorsTask->requestEarlyRun();
      ESP_LOGI(L_MQTT, "Connected");
    });

    mqttPrefix = settings.mqtt.prefix;
    subscribe();

  } else if (mqttPrefix != settings.mqtt.prefix) {
    for (const auto& topic : mqttClient->getSubscribedTopics()) {
      mqttClient->unsubscribe(topic.c_str());
    }

    mqttPrefix = settings.mqtt.prefix;
    subscribe();
  }

  haDiscovery.setWillTopic("~/status");
  haDiscovery.begin(
    {
      .id = mqttPrefix,
      .name = PROJECT_NAME,
      .version = BUILD_VERSION,
      .model = PROJECT_NAME,
      .manufacturer = PROJECT_REPO,
      .url = std::string("http://") + getNetworkIp().toString().c_str() + "/",
    },
    mqttPrefix,
    [](const char* topic, const std::string& payload) {
      mqttClient->publish(topic, payload, true);
    }
  );
  mqttClient->begin({
    .server = settings.mqtt.server,
    .port = settings.mqtt.port,
    .keepAlive = static_cast<uint16_t>(settings.mqtt.interval * 10u),
    .username = settings.mqtt.user,
    .password = settings.mqtt.password,
    .clientId = settings.mqtt.prefix,
    .willTopic = mqttTopic("status"),
  });
}

Task mqttConfigureTask("Configure MQTT", Task::Type::ONCE, [](void*) {
  if (!settings.mqtt.enabled) {
    stopMqtt(true);
    return;
  }

  // Publish complete settings while MQTT is enabled.
  if (!mqttPublishSettingsTask) {
    auto* task = new Task("Publish MQTT settings", [](void*) {
      publishSettings();
    });

    task->setEnabledWhen(isMqttReadyForPublish);
    coreTaskManager.addTask(*task);

    std::lock_guard<std::mutex> lock(mqttMutex);
    mqttPublishSettingsTask = task;
  }

  // Publish the current application and slave state.
  if (!mqttPublishStateTask) {
    auto* task = new Task("Publish MQTT state", [](void*) {
      publishState();
    });

    task->setEnabledWhen(isMqttReadyForPublish);
    coreTaskManager.addTask(*task);

    std::lock_guard<std::mutex> lock(mqttMutex);
    mqttPublishStateTask = task;
  }

  // Publish one discovery config per tick, without retaining Component objects.
  if (!mqttPublishHaTask) {
    auto* task = new Task("Publish HA discovery", [](void*) {
      static bool discoveryEnabled = settings.mqtt.homeAssistantDiscovery;
      static UnitSystem unitSystem = settings.system.unitSystem;
      static bool equithermEnabled = settings.equitherm.enabled;
      static unsigned short interval = settings.mqtt.interval;
      static uint8_t heatingMinTemp = vars.slave.heating.minTemp;
      static uint8_t heatingMaxTemp = vars.slave.heating.maxTemp;
      static bool dhwSupport = settings.opentherm.options.dhwSupport;
      static bool smartControl = vars.master.heating.smartControl;
      static float heatingMinSetpointTemp = vars.master.heating.minTemp;
      static float heatingMaxSetpointTemp = vars.master.heating.maxTemp;
      static uint8_t dhwMinTemp = settings.dhw.minTemp;
      static uint8_t dhwMaxTemp = settings.dhw.maxTemp;
      static uint8_t dhwBoilerMinTemp = vars.slave.dhw.minTemp;
      static uint8_t dhwBoilerMaxTemp = vars.slave.dhw.maxTemp;

      // Observe metadata dependencies regardless of the settings entry point.
      if (discoveryEnabled != settings.mqtt.homeAssistantDiscovery) {
        discoveryEnabled = settings.mqtt.homeAssistantDiscovery;
        mqttHaRefreshRequested.store(true);
      }

      if (unitSystem != settings.system.unitSystem) {
        unitSystem = settings.system.unitSystem;
        mqttHaPendingEntities
          .set(HA_ENTITY_HEATING_MIN_TEMP)
          .set(HA_ENTITY_HEATING_MAX_TEMP)
          .set(HA_ENTITY_PID_MIN_TEMP)
          .set(HA_ENTITY_PID_MAX_TEMP)
          .set(HA_ENTITY_HEATING_CLIMATE)
          .set(HA_ENTITY_DHW_MIN_TEMP)
          .set(HA_ENTITY_DHW_MAX_TEMP)
          .set(HA_ENTITY_DHW_CLIMATE);
        mqttHaPendingSensors.set();
      }

      if (equithermEnabled != settings.equitherm.enabled) {
        equithermEnabled = settings.equitherm.enabled;
        mqttHaPendingEntities.set(HA_ENTITY_PID_MIN_TEMP);
        mqttHaPendingEntities.set(HA_ENTITY_PID_MAX_TEMP);
      }

      if (interval != settings.mqtt.interval) {
        const uint32_t previousExpiration = std::max<uint32_t>(60u, 2u * interval);
        interval = settings.mqtt.interval;
        const uint32_t expiration = std::max<uint32_t>(60u, 2u * interval);
        if (previousExpiration != expiration) {
          mqttHaPendingEntities.set();
        }
      }

      if (heatingMinTemp != vars.slave.heating.minTemp) {
        heatingMinTemp = vars.slave.heating.minTemp;
        mqttHaPendingEntities.set(HA_ENTITY_HEATING_MIN_TEMP);
        mqttHaPendingEntities.set(HA_ENTITY_HEATING_MAX_TEMP);
      }

      if (heatingMaxTemp != vars.slave.heating.maxTemp) {
        heatingMaxTemp = vars.slave.heating.maxTemp;
        mqttHaPendingEntities.set(HA_ENTITY_HEATING_MIN_TEMP);
        mqttHaPendingEntities.set(HA_ENTITY_HEATING_MAX_TEMP);
      }

      if (dhwSupport != settings.opentherm.options.dhwSupport) {
        dhwSupport = settings.opentherm.options.dhwSupport;
        mqttHaPendingEntities
          .set(HA_ENTITY_DHW_MIN_TEMP)
          .set(HA_ENTITY_DHW_MAX_TEMP)
          .set(HA_ENTITY_DHW)
          .set(HA_ENTITY_DHW_CLIMATE)
          .set(HA_ENTITY_DHW_OVERHEAT);
      }

      if (smartControl != vars.master.heating.smartControl) {
        smartControl = vars.master.heating.smartControl;
        mqttHaPendingEntities.set(HA_ENTITY_HEATING_CLIMATE);
      }

      if (heatingMinSetpointTemp != vars.master.heating.minTemp) {
        heatingMinSetpointTemp = vars.master.heating.minTemp;
        mqttHaPendingEntities.set(HA_ENTITY_HEATING_CLIMATE);
      }

      if (heatingMaxSetpointTemp != vars.master.heating.maxTemp) {
        heatingMaxSetpointTemp = vars.master.heating.maxTemp;
        mqttHaPendingEntities.set(HA_ENTITY_HEATING_CLIMATE);
      }

      if (dhwMinTemp != settings.dhw.minTemp) {
        dhwMinTemp = settings.dhw.minTemp;
        mqttHaPendingEntities.set(HA_ENTITY_DHW_CLIMATE);
      }

      if (dhwMaxTemp != settings.dhw.maxTemp) {
        dhwMaxTemp = settings.dhw.maxTemp;
        mqttHaPendingEntities.set(HA_ENTITY_DHW_CLIMATE);
      }

      if (dhwBoilerMinTemp != vars.slave.dhw.minTemp) {
        dhwBoilerMinTemp = vars.slave.dhw.minTemp;
        mqttHaPendingEntities.set(HA_ENTITY_DHW_MIN_TEMP);
        mqttHaPendingEntities.set(HA_ENTITY_DHW_MAX_TEMP);
      }

      if (dhwBoilerMaxTemp != vars.slave.dhw.maxTemp) {
        dhwBoilerMaxTemp = vars.slave.dhw.maxTemp;
        mqttHaPendingEntities.set(HA_ENTITY_DHW_MIN_TEMP);
        mqttHaPendingEntities.set(HA_ENTITY_DHW_MAX_TEMP);
      }

      if (mqttHaRefreshRequested.exchange(false)) {
        mqttHaPendingEntities.set();
        mqttHaPendingSensors.set();
      }

      for (size_t index = 0; index < mqttHaPendingEntities.size(); index++) {
        if (!mqttHaPendingEntities.test(index)) {
          continue;
        }

        mqttHaPendingEntities.reset(index);
        publishHaEntity(static_cast<HaEntity>(index), !settings.mqtt.homeAssistantDiscovery);
        return;
      }

      for (uint8_t sensorId = 0; sensorId < SENSORS_AMOUNT; sensorId++) {
        if (!mqttHaPendingSensors.test(sensorId)) {
          continue;
        }

        mqttHaPendingSensors.reset(sensorId);
        if (settings.mqtt.homeAssistantDiscovery) {
          requestReconfigureMqttSensor(sensorId);

        } else {
          auto sensor = Sensors::getSettings(sensorId);
          if (sensor) {
            requestReconfigureMqttSensor(sensorId, std::move(sensor));
          }
        }
        break;
      }
    });

    task->setInterval(250).setEnabledWhen(isMqttReadyForPublish);
    coreTaskManager.addTask(*task);

    std::lock_guard<std::mutex> lock(mqttMutex);
    mqttPublishHaTask = task;
  }

  // Rebuild sensor discovery and clear old readings in the same queue handler.
  if (!mqttReconfigureSensorsTask) {
    auto* task = new Task("Reconfigure MQTT sensors", [](void*) {
      std::unique_lock<std::mutex> lock(mqttSensorsMutex, std::try_to_lock);
      if (!lock.owns_lock() || mqttSensorChanges.empty()) {
        return;
      }

      MqttSensorChange change = mqttSensorChanges.front();
      const auto sensor = Sensors::getSettings(change.sensorId);
      mqttSensorChanges.pop_front();
      mqttPublishedSensorAt[change.sensorId] = 0;
      lock.unlock();

      const auto& [sensorId, prevSettings] = change;
      const bool metadataChanged = prevSettings && (
        !sensor
        || prevSettings->enabled != sensor->enabled
        || strcmp(prevSettings->name, sensor->name) != 0
        || strcmp(prevSettings->slug, sensor->slug) != 0
        || prevSettings->purpose != sensor->purpose
        || prevSettings->type != sensor->type
      );

      // Metadata changes replace entities; disabling discovery removes them.
      if (prevSettings && Sensors::hasEnabledAndValid(*prevSettings)
          && (metadataChanged || !settings.mqtt.homeAssistantDiscovery)) {
        deleteConnectionDynamicSensor(*prevSettings);
        deleteSignalQualityDynamicSensor(*prevSettings);

        switch (prevSettings->type) {
          case Sensors::Type::BLUETOOTH:
            deleteDynamicSensor(*prevSettings, Sensors::ValueType::TEMPERATURE);
            deleteDynamicSensor(*prevSettings, Sensors::ValueType::HUMIDITY);
            deleteDynamicSensor(*prevSettings, Sensors::ValueType::BATTERY);
            deleteDynamicSensor(*prevSettings, Sensors::ValueType::RSSI);
            break;

          case Sensors::Type::DHT11:
          case Sensors::Type::DHT22:
            deleteDynamicSensor(*prevSettings, Sensors::ValueType::TEMPERATURE);
            deleteDynamicSensor(*prevSettings, Sensors::ValueType::HUMIDITY);
            break;

          default:
            deleteDynamicSensor(*prevSettings);
            break;
        }
      }

      // Hardware, calibration and discovery-only refreshes keep retained readings.
      if (prevSettings && prevSettings->slug[0] && metadataChanged) {
        purgeSensor(prevSettings->slug);
      }

      if (settings.mqtt.homeAssistantDiscovery && sensor && Sensors::hasEnabledAndValid(*sensor)) {
        if (sensor->type == Sensors::Type::BLUETOOTH || sensor->type == Sensors::Type::DALLAS_TEMP ||
            sensor->type == Sensors::Type::DHT11 || sensor->type == Sensors::Type::DHT22) {
          publishConnectionDynamicSensor(*sensor);
          publishSignalQualityDynamicSensor(*sensor);

        } else if (sensor->type == Sensors::Type::NTC_10K_TEMP) {
          publishConnectionDynamicSensor(*sensor);
        }

        switch (sensor->type) {
          case Sensors::Type::BLUETOOTH:
            publishDynamicSensor(*sensor, Sensors::ValueType::TEMPERATURE);
            publishDynamicSensor(*sensor, Sensors::ValueType::HUMIDITY);
            publishDynamicSensor(*sensor, Sensors::ValueType::BATTERY);
            publishDynamicSensor(*sensor, Sensors::ValueType::RSSI, false);
            break;

          case Sensors::Type::DHT11:
          case Sensors::Type::DHT22:
            publishDynamicSensor(*sensor, Sensors::ValueType::TEMPERATURE);
            publishDynamicSensor(*sensor, Sensors::ValueType::HUMIDITY);
            break;

          case Sensors::Type::MANUAL:
            publishDynamicManualSensor(*sensor);
            break;

          default:
            publishDynamicSensor(*sensor);
            break;
        }
      }

      mqttPublishSensorsTask->requestEarlyRun();
    });

    task->setInterval(500).setEnabledWhen(isMqttReady);
    coreTaskManager.addTask(*task);

    std::lock_guard<std::mutex> lock(mqttMutex);
    mqttReconfigureSensorsTask = task;
  }

  // Publish sensor readings after startup and pending subscription changes.
  if (!mqttPublishSensorsTask) {
    auto* task = new Task("Publish MQTT sensors", [](void*) {
      static UnitSystem publishedUnitSystem = UnitSystem::METRIC;
      if (publishedUnitSystem != settings.system.unitSystem) {
        std::lock_guard<std::mutex> lock(mqttSensorsMutex);
        memset(mqttPublishedSensorAt, 0, sizeof(mqttPublishedSensorAt));
        publishedUnitSystem = settings.system.unitSystem;
      }

      const unsigned long changeDelay = mqttPublishSensorsTask->earlyRunRequested() ? 0u : 1000u;
      for (uint8_t sensorId = 0; sensorId < SENSORS_AMOUNT; sensorId++) {
        std::optional<Sensors::Snapshot> snapshot;
        {
          std::lock_guard<std::mutex> lock(mqttSensorsMutex);
          if (isSensorReconfigurationPending(sensorId)) {
            continue;
          }

          // Keep physical updates after this cutoff pending for the next publication.
          const unsigned long snapshotAt = millis();
          snapshot = Sensors::getSnapshot(sensorId);
          if (!snapshot || !Sensors::hasEnabledAndValid(snapshot->settings)) {
            continue;
          }

          const unsigned long publishedAt = mqttPublishedSensorAt[sensorId];
          if (publishedAt) {
            const auto now = millis();
            const auto elapsed = now - publishedAt;
            const bool changed = now - snapshot->result.updatedAt <= elapsed;

            if (elapsed <= MQTT_SENSOR_REPUBLISH_INTERVAL && (!changed || elapsed < changeDelay)) {
              continue;
            }
          }

          // A newer manual value can reset this timestamp while the snapshot is publishing.
          mqttPublishedSensorAt[sensorId] = snapshotAt;
        }

        if (!publishSensor(snapshot->settings, snapshot->result)) {
          {
            std::lock_guard<std::mutex> lock(mqttSensorsMutex);
            mqttPublishedSensorAt[sensorId] = 0;
          }
          ESP_LOGD(L_MQTT, "Sensor publication deferred");
        }
      }
    });

    task->setInterval(500).setEnabledWhen([] {
      if (!isMqttReadyForPublish()) {
        return false;
      }

      std::lock_guard<std::mutex> lock(mqttSensorsMutex);
      return mqttSensorChanges.empty();
    });
    coreTaskManager.addTask(*task);

    std::lock_guard<std::mutex> lock(mqttMutex);
    mqttPublishSensorsTask = task;
  }

  mqttPublishSettingsTask->setInterval(settings.mqtt.interval * 10000u);
  mqttPublishStateTask->setInterval(settings.mqtt.interval * 1000u);

  // Call end() to reset the connection
  stopMqtt(false);

  if (!isNetworkConnected()) {
    return;
  }

  // Call begin() to reconnect
  connect();

  if (!mqttClient->isEnabled()) {
    stopMqtt(false);

    // Preserve a newer reconfiguration request instead of replacing it with backoff.
    if (!mqttConfigureTask.scheduled()) {
      mqttConfigureTask.resume(MQTT_RECONNECT_INTERVAL);
    }

    ESP_LOGE(L_MQTT, "Failed to start MQTT client");
    return;
  }
});

void initMqtt() {
  // Keep only the configuration dispatcher when MQTT publications are disabled.
  mqttConfigureTask.pause().setEnabledWhen([] {
    return !vars.states.upgrading && !restartTask.scheduled() && !restartTask.active();
  });

  coreTaskManager.addTask(mqttConfigureTask);
  onNetworkEvent([](NetworkEvent event) {
    if (event == NetworkEvent::CONNECTED || event == NetworkEvent::DISCONNECTED || event == NetworkEvent::EVENTS_OVERFLOW) {
      requestReconfigureMqtt();
    }
  });
}
