#pragma once

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
    
    NTC_10K_TEMP            = 50,
    DALLAS_TEMP             = 51,
    BLUETOOTH               = 52,

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

  typedef struct {
    bool enabled = false;
    char name[33];
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
    unsigned long activityTime = 0;
    uint8_t signalQuality = 100;
    //float raw[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  } Result;


  static Settings* settings;
  static Result* results;

  static inline void setMaxSensors(uint8_t value) {
    maxSensors = value;
  }

  static inline uint8_t getMaxSensors() {
    return maxSensors;
  }

  static uint8_t getMaxSensorId() {
    uint8_t maxSensors = getMaxSensors();
    return maxSensors > 1 ? (maxSensors - 1) : 0;
  }

  static inline bool isValidSensorId(const uint8_t id) {
    return id >= 0 && id <= getMaxSensorId();
  }

  static inline bool isValidValueId(const uint8_t id) {
    return id >= (uint8_t) ValueType::TEMPERATURE && id <= (uint8_t) ValueType::RSSI;
  }

  static bool hasEnabledAndValid(const uint8_t id) {
    if (!isValidSensorId(id) || !settings[id].enabled) {
      return false;
    }

    if (settings[id].type == Type::NOT_CONFIGURED || settings[id].purpose == Purpose::NOT_CONFIGURED) {
      return false;
    }

    return true;
  }

  static uint8_t getAmountByType(Type type, bool onlyEnabled = false) {
    if (settings == nullptr) {
      return 0;
    }

    uint8_t amount = 0;
    for (uint8_t id = 0; id < getMaxSensorId(); id++) {
      if (settings[id].type == type && (!onlyEnabled || settings[id].enabled)) {
        amount++;
      }
    }

    return amount;
  }

  static int16_t getIdByName(const char* name) {
    if (settings == nullptr) {
      return 0;
    }

    for (uint8_t id = 0; id < getMaxSensorId(); id++) {
      if (strcmp(settings[id].name, name) == 0) {
        return id;
      }
    }

    return -1;
  }

  static int16_t getIdByObjectId(const char* objectId) {
    if (settings == nullptr) {
      return 0;
    }

    String refObjectId;
    for (uint8_t id = 0; id < getMaxSensorId(); id++) {
      Sensors::makeObjectId(refObjectId, settings[id].name);
      if (refObjectId.equals(objectId)) {
        return id;
      }
    }

    return -1;
  }

  static bool setValueById(const uint8_t sensorId, float value, const ValueType valueType, const bool updateActivityTime = false, const bool markConnected = false) {
    if (settings == nullptr || results == nullptr) {
      return false;
    }

    uint8_t valueId = (uint8_t) valueType;
    if (!isValidSensorId(sensorId) || !isValidValueId(valueId)) {
      return false;
    }

    auto& sSensor = settings[sensorId];
    auto& rSensor = results[sensorId];
    
    float compensatedValue = value;
    if (sSensor.type == Type::HEATING_SETPOINT_TEMP || sSensor.type == Type::MANUAL) {
      rSensor.values[valueId] = compensatedValue;

    } else {
      if (valueType == ValueType::PRIMARY) {
        if (fabsf(sSensor.factor) > 0.001f) {
          compensatedValue *= sSensor.factor;
        }

        if (fabsf(sSensor.offset) > 0.001f) {
          compensatedValue += sSensor.offset;
        }

      } else if (valueType == ValueType::RSSI) {
        if (sSensor.type == Type::BLUETOOTH) {
          rSensor.signalQuality = Sensors::bluetoothRssiToQuality(value);
        }
      }

      if (sSensor.filtering && fabsf(rSensor.values[valueId]) >= 0.1f) {
        rSensor.values[valueId] += (compensatedValue - rSensor.values[valueId]) * sSensor.filteringFactor;
        
      } else {
        rSensor.values[valueId] = compensatedValue;
      }
    }

    if (updateActivityTime) {
      rSensor.activityTime = millis();
    }

    if (markConnected && !rSensor.connected) {
      rSensor.connected = true;

      Log.snoticeln(
        FPSTR(L_SENSORS), F("#%hhu '%s' new status: CONNECTED"),
        sensorId, sSensor.name
      );
    }

    Log.snoticeln(
      FPSTR(L_SENSORS), F("#%hhu '%s' new value %hhu: %.2f, compensated: %.2f, raw: %.2f"),
      sensorId, sSensor.name, valueId, rSensor.values[valueId], compensatedValue, value
    );

    return true;
  }

  static uint8_t setValueByType(Type type, float value, const ValueType valueType, const bool updateActivityTime = false, const bool markConnected = false) {
    if (settings == nullptr) {
      return 0;
    }

    uint8_t updated = 0;
    
    // read sensors data for current instance
    for (uint8_t sensorId = 0; sensorId < getMaxSensorId(); sensorId++) {
      auto& sSensor = settings[sensorId];
      
      // only target & valid sensors
      if (!sSensor.enabled || sSensor.type != type) {
        continue;
      }

      if (setValueById(sensorId, value, valueType, updateActivityTime, markConnected)) {
        updated++;
      }
    }

    return updated;
  }

  static bool getConnectionStatusById(const uint8_t sensorId) {
    if (settings == nullptr || results == nullptr) {
      return false;
    }

    if (!isValidSensorId(sensorId)) {
      return false;
    }

    return results[sensorId].connected;
  }

  static bool setConnectionStatusById(const uint8_t sensorId, const bool status, const bool updateActivityTime = true) {
    if (settings == nullptr || results == nullptr) {
      return false;
    }

    if (!isValidSensorId(sensorId)) {
      return false;
    }

    auto& sSensor = settings[sensorId];
    auto& rSensor = results[sensorId];

    if (rSensor.connected != status) {
      Log.snoticeln(
        FPSTR(L_SENSORS), F("#%hhu '%s' new status: %s"),
        sensorId, sSensor.name, status ? F("CONNECTED") : F("DISCONNECTED")
      );
      
      rSensor.connected = status;
    }

    if (updateActivityTime) {
      rSensor.activityTime = millis();
    }

    return true;
  }

  static uint8_t setConnectionStatusByType(Type type, const bool status, const bool updateActivityTime = true) {
    if (settings == nullptr) {
      return 0;
    }

    uint8_t updated = 0;
    
    // read sensors data for current instance
    for (uint8_t sensorId = 0; sensorId < getMaxSensorId(); sensorId++) {
      auto& sSensor = settings[sensorId];
      
      // only target & valid sensors
      if (!sSensor.enabled || sSensor.type != type) {
        continue;
      }

      if (setConnectionStatusById(sensorId, status, updateActivityTime)) {
        updated++;
      }
    }

    return updated;
  }

  static float getMeanValueByPurpose(Purpose purpose, const ValueType valueType, bool onlyConnected = true) {
    if (settings == nullptr || results == nullptr) {
      return 0;
    }

    uint8_t valueId = (uint8_t) valueType;
    if (!isValidValueId(valueId)) {
      return false;
    }
    
    float value = 0.0f;
    uint8_t amount = 0;

    for (uint8_t id = 0; id < getMaxSensorId(); id++) {
      auto& sSensor = settings[id];
      auto& rSensor = results[id];

      if (sSensor.purpose == purpose && (!onlyConnected || rSensor.connected)) {
        value += rSensor.values[valueId];
        amount++;
      }
    }

    if (!amount) {
      return 0.0f;
      
    } else if (amount == 1) {
      return value;

    } else {
      return value / amount;
    }
  }

  static bool existsConnectedSensorsByPurpose(Purpose purpose) {
    if (settings == nullptr || results == nullptr) {
      return 0;
    }

    for (uint8_t id = 0; id < getMaxSensorId(); id++) {
      if (settings[id].purpose == purpose && results[id].connected) {
        return true;
      }
    }

    return false;
  }

  static String& cleanName(String& value, char space = ' ') {
    // only valid symbols
    for (uint8_t pos = 0; pos < value.length(); pos++) {
      char symbol = value.charAt(pos);

      // 0..9
      if (symbol >= 48 && symbol <= 57) {
        continue;
      }

      // A..Z
      if (symbol >= 65 && symbol <= 90) {
        continue;
      }

      // a..z
      if (symbol >= 97 && symbol <= 122) {
        continue;
      }

      // _-
      if (symbol == 95 || symbol == 45 || symbol == space) {
        continue;
      }

      value.setCharAt(pos, space);
    }

    value.trim();

    return value;
  }

  template <class T> 
  static String cleanName(T value, char space = ' ') {
    String res = value;
    return cleanName(res, space);
  }

  template <class T> 
  static String& makeObjectId(String& res, T value, char separator = '_') {
    res = value;
    cleanName(res);
    res.toLowerCase();
    res.replace(' ', separator);

    return res;
  }

  template <class T> 
  static String makeObjectId(T value, char separator = '_') {
    String res;
    makeObjectId(res, value, separator);

    return res;
  }

  template <class TV, class TS> 
  static String& makeObjectIdWithSuffix(String& res, TV value, TS suffix, char separator = '_') {
    res.clear();
    makeObjectId(res, value, separator);
    res += separator;
    res += suffix;

    return res;
  }

  template <class TV, class TS> 
  static String makeObjectIdWithSuffix(TV value, TS suffix, char separator = '_') {
    String res;
    makeObjectIdWithSuffix(res, value, suffix, separator);
    return res;
  }

  template <class TV, class TP> 
  static String& makeObjectIdWithPrefix(String& res, TV value, TP prefix, char separator = '_') {
    res = prefix;
    res += separator;
    res += makeObjectId(value, separator).c_str();

    return res;
  }

  template <class TV, class TP> 
  static String makeObjectIdWithPrefix(TV value, TP prefix, char separator = '_') {
    String res;
    return makeObjectIdWithPrefix(res, value, prefix, separator);
  }

  static uint8_t bluetoothRssiToQuality(int rssi) {
    return constrain(map(rssi, -110, -50, 0, 100), 0, 100);;
  }
};

uint8_t Sensors::maxSensors = 0;
Sensors::Settings* Sensors::settings = nullptr;
Sensors::Result* Sensors::results = nullptr;