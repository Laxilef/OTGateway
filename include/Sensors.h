#pragma once

#include <Arduino.h>
#include <esp_log.h>
#include <errno.h>
#include <cmath>
#include <functional>
#include <math.h>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "utils.h"
#include "strings_shared.h"

class Sensors {
protected:
  static uint8_t maxSensors;

public:
  enum class Type : uint8_t {
    OT_OUTDOOR_TEMP         = 0,
    OT_HEATING_TEMP         = 1,
    OT_HEATING_RETURN_TEMP  = 2,
    OT_DHW_TEMP             = 3,
    OT_DHW_TEMP2            = 4,
    OT_DHW_FLOW_RATE        = 5,
    OT_CH2_TEMP             = 6,
    OT_EXHAUST_TEMP         = 7,
    OT_HEAT_EXCHANGER_TEMP  = 8,
    OT_PRESSURE             = 9,
    OT_MODULATION_LEVEL     = 10,
    OT_CURRENT_POWER        = 11,
    OT_EXHAUST_CO2          = 12,
    OT_EXHAUST_FAN_SPEED    = 13,
    OT_SUPPLY_FAN_SPEED     = 14,
    OT_SOLAR_STORAGE_TEMP   = 15,
    OT_SOLAR_COLLECTOR_TEMP = 16,
    OT_FAN_SPEED_SETPOINT   = 17,
    OT_FAN_SPEED_CURRENT    = 18,

    OT_BURNER_STARTS        = 19,
    OT_DHW_BURNER_STARTS    = 20,
    OT_HEATING_PUMP_STARTS  = 21,
    OT_DHW_PUMP_STARTS      = 22,
    OT_BURNER_HOURS         = 23,
    OT_DHW_BURNER_HOURS     = 24,
    OT_HEATING_PUMP_HOURS   = 25,
    OT_DHW_PUMP_HOURS       = 26,
    OT_COOLING_HOURS        = 27,

    NTC_10K_TEMP            = 50,
    DALLAS_TEMP             = 51,
    BLUETOOTH               = 52,
    DHT11                   = 53,
    DHT22                   = 54,

    HEATING_SETPOINT_TEMP   = 253,
    MANUAL                  = 254,
    NOT_CONFIGURED          = 255
  };

  enum class Purpose : uint8_t {
    OUTDOOR_TEMP          = 0,
    INDOOR_TEMP           = 1,
    HEATING_TEMP          = 2,
    HEATING_RETURN_TEMP   = 3,
    DHW_TEMP              = 4,
    DHW_RETURN_TEMP       = 5,
    DHW_FLOW_RATE         = 6,
    EXHAUST_TEMP          = 7,
    MODULATION_LEVEL      = 8,

    NUMBER                = 247,
    POWER_FACTOR          = 248,
    POWER                 = 249,
    FAN_SPEED             = 250,
    CO2                   = 251,
    PRESSURE              = 252,
    HUMIDITY              = 253,
    TEMPERATURE           = 254,
    NOT_CONFIGURED        = 255
  };

  enum class ValueType : uint8_t {
    PRIMARY     = 0,
    TEMPERATURE = 0,
    HUMIDITY    = 1,
    BATTERY     = 2,
    RSSI        = 3
  };

  enum class AverageType : uint8_t {
    MEAN    = 0,
    MINIMUM = 1,
    MAXIMUM = 2
  };

  typedef struct {
    bool enabled = false;
    char name[33];
    char slug[33] = {};
    Purpose purpose = Purpose::NOT_CONFIGURED;
    Type type = Type::NOT_CONFIGURED;
    uint8_t gpio = GPIO_IS_NOT_CONFIGURED;
    uint8_t address[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    float offset = 0.0f;
    float factor = 1.0f;
    bool filtering = false;
    float filteringFactor = 0.15f;
  } Settings;

  typedef struct {
    bool connected = false;
    unsigned long activityAt = 0;
    unsigned long updatedAt = 0;
    uint8_t signalQuality = 100;
    //float raw[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  } Result;

  struct Snapshot {
    Settings settings;
    Result result;
  };

  // Set the backing pointers before starting workers.
  // Methods lock internally; hold mutex when accessing the arrays directly.
  static Settings* settings;
  static Result* results;
  static std::mutex mutex;

  static std::optional<Settings> getSettings(uint8_t sensorId) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr || !isValidSensorId(sensorId)) {
      return std::nullopt;
    }
    return settings[sensorId];
  }

  static std::optional<Snapshot> getSnapshot(uint8_t sensorId) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr || results == nullptr || !isValidSensorId(sensorId)) {
      return std::nullopt;
    }
    return Snapshot{settings[sensorId], results[sensorId]};
  }

  static inline void setMaxSensors(uint8_t value) {
    // The slot count and backing arrays are configured before starting workers.
    maxSensors = value;
  }

  static inline uint8_t getMaxSensors() {
    return maxSensors;
  }

  static uint8_t getMaxSensorId() {
    uint8_t maxSensors = getMaxSensors();
    return maxSensors > 1 ? (maxSensors - 1) : 0;
  }

  template <class T>
  static bool isValidSensorId(const T id) {
    return id >= 0 && id <= getMaxSensorId();
  }

  static std::optional<uint8_t> parseId(const char* value) {
    if (!isDigit(value)) {
      return std::nullopt;
    }

    errno = 0;
    const unsigned long parsed = strtoul(value, nullptr, 10);
    if (errno == ERANGE || !isValidSensorId(parsed)) {
      return std::nullopt;
    }

    return static_cast<uint8_t>(parsed);
  }

  static inline bool isValidValueId(const uint8_t id) {
    return id >= (uint8_t) ValueType::TEMPERATURE && id <= (uint8_t) ValueType::RSSI;
  }

  static bool isDifferent(const Settings& previous, const Settings& current) {
    return previous.enabled != current.enabled
      || strcmp(previous.name, current.name) != 0
      || strcmp(previous.slug, current.slug) != 0
      || previous.purpose != current.purpose
      || previous.type != current.type
      || previous.gpio != current.gpio
      || memcmp(previous.address, current.address, sizeof(previous.address)) != 0
      || isFloatDifferent(previous.offset, current.offset, 0.0001f)
      || isFloatDifferent(previous.factor, current.factor, 0.0001f)
      || previous.filtering != current.filtering
      || isFloatDifferent(previous.filteringFactor, current.filteringFactor, 0.0001f);
  }

  static bool hasEnabledAndValid(const Settings& sensor) {
    return sensor.enabled
        && sensor.slug[0] != '\0'
        && sensor.type != Type::NOT_CONFIGURED
        && sensor.purpose != Purpose::NOT_CONFIGURED;
  }

  static bool hasEnabledAndValid(const uint8_t id) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr || !isValidSensorId(id)) {
      return false;
    }
    return hasEnabledAndValid(settings[id]);
  }

  static uint8_t getAmountByType(Type type, bool onlyEnabled = false) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr) {
      return 0;
    }

    uint8_t amount = 0;
    for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
      if (settings[id].type == type && (!onlyEnabled || settings[id].enabled)) {
        amount++;
      }
    }

    return amount;
  }

  static std::optional<uint8_t> getIdByName(const char* name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr) {
      return std::nullopt;
    }

    for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
      if (strcmp(settings[id].name, name) == 0) {
        return id;
      }
    }

    return std::nullopt;
  }

  static std::optional<uint8_t> getIdBySlug(const char* slug) {
    if (slug == nullptr || !slug[0]) {
      return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr) {
      return std::nullopt;
    }

    for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
      if (strcmp(settings[id].slug, slug) == 0) {
        return id;
      }
    }

    return std::nullopt;
  }

  static float convertValue(const Settings& sensor, const ValueType valueType, float value, const UnitSystem unitFrom, const UnitSystem unitTo) {
    if (valueType != ValueType::PRIMARY || unitFrom == unitTo) {
      return value;
    }

    if (sensor.type == Type::HEATING_SETPOINT_TEMP && value == 0) {
      return 0;
    }

    Purpose purpose = sensor.purpose;
    if (sensor.type != Type::MANUAL) {
      switch (sensor.type) {
        case Type::OT_OUTDOOR_TEMP:
        case Type::OT_HEATING_TEMP:
        case Type::OT_HEATING_RETURN_TEMP:
        case Type::OT_DHW_TEMP:
        case Type::OT_DHW_TEMP2:
        case Type::OT_CH2_TEMP:
        case Type::OT_EXHAUST_TEMP:
        case Type::OT_HEAT_EXCHANGER_TEMP:
        case Type::OT_SOLAR_STORAGE_TEMP:
        case Type::OT_SOLAR_COLLECTOR_TEMP:
        case Type::NTC_10K_TEMP:
        case Type::DALLAS_TEMP:
        case Type::BLUETOOTH:
        case Type::DHT11:
        case Type::DHT22:
        case Type::HEATING_SETPOINT_TEMP:
          purpose = Purpose::TEMPERATURE;
          break;

        case Type::OT_PRESSURE:
          purpose = Purpose::PRESSURE;
          break;

        case Type::OT_DHW_FLOW_RATE:
          purpose = Purpose::DHW_FLOW_RATE;
          break;

        default:
          return value;
      }
    }

    switch (purpose) {
      case Purpose::OUTDOOR_TEMP:
      case Purpose::INDOOR_TEMP:
      case Purpose::HEATING_TEMP:
      case Purpose::HEATING_RETURN_TEMP:
      case Purpose::DHW_TEMP:
      case Purpose::DHW_RETURN_TEMP:
      case Purpose::EXHAUST_TEMP:
      case Purpose::TEMPERATURE:
        return convertTemp(value, unitFrom, unitTo);

      case Purpose::PRESSURE:
        return convertPressure(value, unitFrom, unitTo);

      case Purpose::DHW_FLOW_RATE:
        return convertVolume(value, unitFrom, unitTo);

      default:
        return value;
    }
  }

  static bool setValueById(const uint8_t sensorId, float value, const ValueType valueType, const bool updateActivityAt = false, const bool markConnected = false) {
    const uint8_t valueId = static_cast<uint8_t>(valueType);
    char name[sizeof(Settings::name)];
    float compensatedValue = value;
    float loggedValue;
    bool connectedChanged;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (settings == nullptr || results == nullptr || !isValidSensorId(sensorId) || !isValidValueId(valueId)) {
        return false;
      }
      const auto& sensor = settings[sensorId];
      auto& result = results[sensorId];
      if (sensor.type == Type::HEATING_SETPOINT_TEMP || sensor.type == Type::MANUAL) {
        result.values[valueId] = compensatedValue;

      } else {
        if (valueType == ValueType::PRIMARY) {
          if (fabsf(sensor.factor) > 0.001f) {
            compensatedValue *= sensor.factor;
          }
          if (fabsf(sensor.offset) > 0.001f) {
            compensatedValue += sensor.offset;
          }

        } else if (valueType == ValueType::RSSI) {
          if (sensor.type == Type::BLUETOOTH) {
            result.signalQuality = Sensors::bluetoothRssiToQuality(value);
          }
        }

        if (sensor.filtering && fabsf(result.values[valueId]) >= 0.1f) {
          result.values[valueId] += (compensatedValue - result.values[valueId]) * sensor.filteringFactor;

        } else {
          result.values[valueId] = compensatedValue;
        }
      }

      const auto now = millis();
      result.updatedAt = now;
      if (updateActivityAt) {
        result.activityAt = now;
      }
      connectedChanged = markConnected && !result.connected;
      if (connectedChanged) {
        result.connected = true;
      }
      memcpy(name, sensor.name, sizeof(name));
      loggedValue = result.values[valueId];
    }
    if (connectedChanged) {
      logConnectionStatus(sensorId, name, true);
    }
    ESP_LOGI(
      L_SENSORS, "#%hhu '%s' new value %hhu: %.2f, compensated: %.2f, raw: %.2f",
      sensorId, name, valueId, loggedValue, compensatedValue, value
    );
    return true;
  }

  static bool setManualValueById(uint8_t sensorId, float value, const UnitSystem srcUnit) {
    char name[sizeof(Settings::name)];
    bool connectedChanged;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (settings == nullptr || results == nullptr || !isValidSensorId(sensorId)) {
        return false;
      }

      const auto& sensor = settings[sensorId];
      if (!sensor.enabled || sensor.type != Type::MANUAL) {
        return false;
      }

      value = convertValue(sensor, ValueType::PRIMARY, value, srcUnit, UnitSystem::METRIC);
      if (!std::isfinite(value)) {
        return false;
      }

      results[sensorId].values[static_cast<uint8_t>(ValueType::PRIMARY)] = value;
      connectedChanged = applyConnectionStatus(sensorId, true, true);
      memcpy(name, sensor.name, sizeof(name));
    }
    if (connectedChanged) {
      logConnectionStatus(sensorId, name, true);
    }
    ESP_LOGI(
      L_SENSORS, "#%hhu '%s' new value %hhu: %.2f, compensated: %.2f, raw: %.2f",
      sensorId, name, static_cast<uint8_t>(ValueType::PRIMARY), value, value, value
    );
    return true;
  }

  static std::optional<uint8_t> setManualValueBySlug(std::string_view slug, float value, const UnitSystem srcUnit) {
    if (slug.empty()) {
      return std::nullopt;
    }

    std::optional<uint8_t> sensorId;
    char name[sizeof(Settings::name)];
    bool connectedChanged = false;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (settings == nullptr || results == nullptr) {
        return std::nullopt;
      }

      for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
        const auto& sensor = settings[id];
        if (!sensor.enabled || sensor.type != Type::MANUAL) {
          continue;
        }

        if (slug != sensor.slug) {
          continue;
        }

        value = convertValue(sensor, ValueType::PRIMARY, value, srcUnit, UnitSystem::METRIC);
        if (!std::isfinite(value)) {
          return std::nullopt;
        }

        sensorId = id;
        results[id].values[static_cast<uint8_t>(ValueType::PRIMARY)] = value;
        connectedChanged = applyConnectionStatus(id, true, true);
        memcpy(name, sensor.name, sizeof(name));
        break;
      }
    }
    if (!sensorId) {
      return std::nullopt;
    }
    if (connectedChanged) {
      logConnectionStatus(*sensorId, name, true);
    }
    ESP_LOGI(
      L_SENSORS, "#%hhu '%s' new value %hhu: %.2f, compensated: %.2f, raw: %.2f",
      *sensorId, name, static_cast<uint8_t>(ValueType::PRIMARY), value, value, value
    );
    return sensorId;
  }

  static uint8_t setValueByType(Type type, float value, const ValueType valueType, const bool updateActivityAt = false, const bool markConnected = false) {
    uint8_t updated = 0;
    for (uint8_t sensorId = 0; sensorId <= getMaxSensorId(); sensorId++) {
      const auto sensor = getSettings(sensorId);
      if (!sensor || !sensor->enabled || sensor->type != type) {
        continue;
      }
      if (setValueById(sensorId, value, valueType, updateActivityAt, markConnected)) {
        updated++;
      }
    }
    return updated;
  }

  static bool getConnectionStatusById(const uint8_t sensorId) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr || results == nullptr || !isValidSensorId(sensorId)) {
      return false;
    }
    return results[sensorId].connected;
  }

  static bool setConnectionStatusById(const uint8_t sensorId, const bool status, const bool updateActivityAt = true) {
    char name[sizeof(Settings::name)];
    bool changed;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (settings == nullptr || results == nullptr || !isValidSensorId(sensorId)) {
        return false;
      }
      changed = applyConnectionStatus(sensorId, status, updateActivityAt);
      memcpy(name, settings[sensorId].name, sizeof(name));
    }
    if (changed) {
      logConnectionStatus(sensorId, name, status);
    }
    return true;
  }

  static uint8_t setConnectionStatusByType(Type type, const bool status, const bool updateActivityAt = true) {
    uint8_t updated = 0;
    for (uint8_t sensorId = 0; sensorId <= getMaxSensorId(); sensorId++) {
      char name[sizeof(Settings::name)];
      bool changed;
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (settings == nullptr || results == nullptr) {
          return updated;
        }
        const auto& sensor = settings[sensorId];
        if (!sensor.enabled || sensor.type != type) {
          continue;
        }
        changed = applyConnectionStatus(sensorId, status, updateActivityAt);
        memcpy(name, sensor.name, sizeof(name));
      }
      if (changed) {
        logConnectionStatus(sensorId, name, status);
      }
      updated++;
    }
    return updated;
  }

  static bool adjustSignalQuality(uint8_t sensorId, int8_t delta) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr || results == nullptr || !isValidSensorId(sensorId)) {
      return false;
    }

    int value = constrain(results[sensorId].signalQuality + delta, 0, 100);
    if (value != results[sensorId].signalQuality) {
      results[sensorId].signalQuality = static_cast<uint8_t>(value);
      results[sensorId].updatedAt = millis();
    }
    return true;
  }

  static bool setAddressById(uint8_t sensorId, const uint8_t* address, const std::function<void()>& onUpdated = nullptr) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (settings == nullptr || address == nullptr || !isValidSensorId(sensorId)) {
        return false;
      }

      if (memcmp(settings[sensorId].address, address, sizeof(Settings::address)) == 0) {
        return true;
      }

      memcpy(settings[sensorId].address, address, sizeof(Settings::address));
    }

    if (onUpdated) {
      onUpdated();
    }

    return true;
  }

  static bool updateConnectionStatusById(uint8_t sensorId, unsigned long wiredTimeout, unsigned long wirelessTimeout) {
    char name[sizeof(Settings::name)];
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (settings == nullptr || results == nullptr || !isValidSensorId(sensorId) || !results[sensorId].connected) {
        return false;
      }

      const auto& sensor = settings[sensorId];
      const auto& result = results[sensorId];
      bool disconnected = !hasEnabledAndValid(sensor);
      if (!disconnected) {
        const auto timeout = sensor.type == Type::MANUAL || sensor.type == Type::BLUETOOTH
          ? wirelessTimeout
          : wiredTimeout;
        disconnected = millis() - result.activityAt > timeout;
      }
      if (!disconnected) {
        return false;
      }
      applyConnectionStatus(sensorId, false, false);
      memcpy(name, sensor.name, sizeof(name));
    }
    logConnectionStatus(sensorId, name, false);
    return true;
  }

  static float getMeanValueByPurpose(Purpose purpose, const ValueType valueType, const AverageType avgType = AverageType::MEAN, const bool onlyConnected = true, const float defaultValue = NAN) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr || results == nullptr) {
      return defaultValue;
    }

    uint8_t valueId = (uint8_t) valueType;
    if (!isValidValueId(valueId)) {
      return defaultValue;
    }

    float value = 0.0f;
    uint8_t amount = 0;

    if (avgType == AverageType::MEAN) {
      float sum = 0.0f;
      for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
        auto& sSensor = settings[id];
        auto& rSensor = results[id];

        if (sSensor.purpose == purpose && (!onlyConnected || rSensor.connected)) {
          sum += rSensor.values[valueId];
          amount++;
        }
      }

      value = amount == 1 ? sum : (sum / amount);

    } else if (avgType == AverageType::MINIMUM) {
      for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
        auto& sSensor = settings[id];
        auto& rSensor = results[id];

        if (sSensor.purpose == purpose && (!onlyConnected || rSensor.connected)) {
          if (amount == 0 || rSensor.values[valueId] < value) {
            value = rSensor.values[valueId];
            amount++;
          }
        }
      }

    } else if (avgType == AverageType::MAXIMUM) {
      for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
        auto& sSensor = settings[id];
        auto& rSensor = results[id];

        if (sSensor.purpose == purpose && (!onlyConnected || rSensor.connected)) {
          if (amount == 0 || rSensor.values[valueId] > value) {
            value = rSensor.values[valueId];
            amount++;
          }
        }
      }
    }

    return amount > 0 ? value : defaultValue;
  }

  static bool existsConnectedSensorsByPurpose(Purpose purpose) {
    std::lock_guard<std::mutex> lock(mutex);
    if (settings == nullptr || results == nullptr) {
      return false;
    }

    for (uint8_t id = 0; id <= getMaxSensorId(); id++) {
      if (settings[id].purpose == purpose && results[id].connected) {
        return true;
      }
    }

    return false;
  }

  static std::string& cleanName(std::string& value, char space = ' ') {
    for (char& symbol : value) {
      if (symbol >= '0' && symbol <= '9') {
        continue;
      }
      if (symbol >= 'A' && symbol <= 'Z') {
        continue;
      }
      if (symbol >= 'a' && symbol <= 'z') {
        continue;
      }
      if (symbol == '_' || symbol == '-' || symbol == space) {
        continue;
      }
      symbol = space;
    }

    // trim
    const auto first = value.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string::npos) {
      value.clear();
      return value;
    }
    value.erase(value.find_last_not_of(" \t\r\n\f\v") + 1);
    value.erase(0, first);

    return value;
  }

  static std::string cleanName(std::string_view value, char space = ' ') {
    std::string result(value);
    cleanName(result, space);
    return result;
  }

  static std::string& makeSlug(std::string& result, std::string_view value, char separator = '_') {
    result.assign(value);
    cleanName(result);
    for (char& symbol : result) {
      if (symbol >= 'A' && symbol <= 'Z') {
        symbol += 'a' - 'A';
      }
      if (symbol == ' ') {
        symbol = separator;
      }
    }
    return result;
  }

  static std::string makeSlug(std::string_view value, char separator = '_') {
    std::string result;
    makeSlug(result, value, separator);
    return result;
  }

  static std::string makeSlugWithSuffix(std::string_view value, std::string_view suffix, char separator = '_') {
    std::string result;
    result.reserve(value.size() + 1 + suffix.size());
    makeSlug(result, value, separator);
    result += separator;
    result.append(suffix);
    return result;
  }

  static std::string& makeSlugWithSuffix(std::string& result, std::string_view value, std::string_view suffix, char separator = '_') {
    result = makeSlugWithSuffix(value, suffix, separator);
    return result;
  }

  static std::string makeSlugWithPrefix(std::string_view value, std::string_view prefix, char separator = '_') {
    std::string result;
    result.reserve(prefix.size() + 1 + value.size());
    makeSlug(result, value, separator);
    result.insert(0, 1, separator);
    result.insert(0, prefix);
    return result;
  }

  static std::string& makeSlugWithPrefix(std::string& result, std::string_view value, std::string_view prefix, char separator = '_') {
    result = makeSlugWithPrefix(value, prefix, separator);
    return result;
  }

  static uint8_t bluetoothRssiToQuality(int rssi) {
    return constrain(map(rssi, -110, -50, 0, 100), 0, 100);;
  }

private:
  // The caller holds mutex; the status log is emitted after it is released.
  static bool applyConnectionStatus(uint8_t sensorId, bool status, bool updateActivityAt) {
    auto& result = results[sensorId];
    const bool changed = result.connected != status;
    result.connected = status;
    if (changed || updateActivityAt) {
      const auto now = millis();
      result.updatedAt = now;
      if (updateActivityAt) {
        result.activityAt = now;
      }
    }
    return changed;
  }

  static void logConnectionStatus(uint8_t sensorId, const char* name, bool status) {
    ESP_LOGI(
      L_SENSORS, "#%hhu '%s' new status: %s",
      sensorId, name, status ? "CONNECTED" : "DISCONNECTED"
    );
  }

  static constexpr bool isDigit(const char* value) {
    if (value == nullptr || *value == '\0') {
      return false;
    }

    while (*value != '\0') {
      if (*value < '0' || *value > '9') {
        return false;
      }
      value++;
    }

    return true;
  }
};

inline uint8_t Sensors::maxSensors = 0;
inline Sensors::Settings* Sensors::settings = nullptr;
inline Sensors::Result* Sensors::results = nullptr;
inline std::mutex Sensors::mutex;
