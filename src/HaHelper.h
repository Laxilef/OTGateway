#pragma once
#include <HomeAssistantHelper.h>

class HaHelper : public HomeAssistantHelper {
public:
  static const uint8_t TEMP_SOURCE_HEATING = 0;
  static const uint8_t TEMP_SOURCE_INDOOR = 1;
  static const char AVAILABILITY_OT_CONN[];
  static const char AVAILABILITY_SENSOR_CONN[];

  void updateCachedTopics() {
    this->statusTopic = this->getDeviceTopic(F("status"));
    this->stateTopic = this->getDeviceTopic(F("state"));
    this->setStateTopic = this->getDeviceTopic(F("state/set"));
    this->settingsTopic = this->getDeviceTopic(F("settings"));
    this->setSettingsTopic = this->getDeviceTopic(F("settings/set"));
  }

  void setExpireAfter(unsigned short value) {
    this->expireAfter = value;
  }

  auto getExpireAfter() {
    return this->expireAfter;
  }

  bool publishDynamicSensor(Sensors::Settings& sSensor, Sensors::ValueType vType = Sensors::ValueType::PRIMARY, UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;

    // set device class & unit of measurement
    switch (sSensor.purpose) {
      case Sensors::Purpose::OUTDOOR_TEMP:
      case Sensors::Purpose::INDOOR_TEMP:
      case Sensors::Purpose::HEATING_TEMP:
      case Sensors::Purpose::HEATING_RETURN_TEMP:
      case Sensors::Purpose::DHW_TEMP:
      case Sensors::Purpose::DHW_RETURN_TEMP:
      case Sensors::Purpose::EXHAUST_TEMP:
      case Sensors::Purpose::TEMPERATURE:
        doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);
        if (unit == UnitSystem::METRIC) {
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);

          if (sSensor.type == Sensors::Type::MANUAL) {
            doc[FPSTR(HA_MIN)] = -99;
            doc[FPSTR(HA_MAX)] = 99;
          }
          
        } else if (unit == UnitSystem::IMPERIAL) {
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);

          if (sSensor.type == Sensors::Type::MANUAL) {
            doc[FPSTR(HA_MIN)] = -147;
            doc[FPSTR(HA_MAX)] = 211;
          }
        }
        break;

      case Sensors::Purpose::DHW_FLOW_RATE:
        doc[FPSTR(HA_DEVICE_CLASS)] = F("volume_flow_rate");
        if (unit == UnitSystem::METRIC) {
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_L_MIN);
          
        } else if (unit == UnitSystem::IMPERIAL) {
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_GAL_MIN);
        }
        break;

      case Sensors::Purpose::MODULATION_LEVEL:
      case Sensors::Purpose::POWER_FACTOR:
        doc[FPSTR(HA_DEVICE_CLASS)] = F("power_factor");
        doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_PERCENT);
        break;

      case Sensors::Purpose::POWER:
        doc[FPSTR(HA_DEVICE_CLASS)] = F("power");
        doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("kW");
        break;

      case Sensors::Purpose::FAN_SPEED:
        doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("RPM");
        break;

      case Sensors::Purpose::CO2:
        doc[FPSTR(HA_DEVICE_CLASS)] = F("carbon_dioxide");
        doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("ppm");
        break;

      case Sensors::Purpose::PRESSURE:
        doc[FPSTR(HA_DEVICE_CLASS)] = F("pressure");
        if (unit == UnitSystem::METRIC) {
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("bar");

        } else if (unit == UnitSystem::IMPERIAL) {
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("psi");
        }
        break;

      case Sensors::Purpose::HUMIDITY:
        doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_HUMIDITY);
        doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_PERCENT);
        break;

      default:
        break;
    }

    // set icon
    switch (sSensor.purpose) {
      case Sensors::Purpose::OUTDOOR_TEMP:
        doc[FPSTR(HA_ICON)] = F("mdi:home-thermometer-outline");
        break;
        
      case Sensors::Purpose::INDOOR_TEMP:
        doc[FPSTR(HA_ICON)] = F("mdi:home-thermometer");
        break;
        
      case Sensors::Purpose::HEATING_TEMP:
        doc[FPSTR(HA_ICON)] = F("mdi:radiator");
        break;
        
      case Sensors::Purpose::HEATING_RETURN_TEMP:
        doc[FPSTR(HA_ICON)] = F("mdi:heating-coil");
        break;
        
      case Sensors::Purpose::DHW_TEMP:
        doc[FPSTR(HA_ICON)] = F("mdi:faucet");
        break;

      case Sensors::Purpose::DHW_RETURN_TEMP:
        doc[FPSTR(HA_ICON)] = F("mdi:heating-coil");
        break;
        
      case Sensors::Purpose::EXHAUST_TEMP:
        doc[FPSTR(HA_ICON)] = F("mdi:smoke");
        break;

      case Sensors::Purpose::TEMPERATURE:
        doc[FPSTR(HA_ICON)] = F("mdi:thermometer-lines");
        break;

      case Sensors::Purpose::DHW_FLOW_RATE:
        doc[FPSTR(HA_ICON)] = F("mdi:faucet");
        break;

      case Sensors::Purpose::MODULATION_LEVEL:
        doc[FPSTR(HA_ICON)] = F("mdi:fire-circle");
        break;

      case Sensors::Purpose::POWER_FACTOR:
      case Sensors::Purpose::POWER:
        doc[FPSTR(HA_ICON)] = F("mdi:chart-bar");
        break;

      case Sensors::Purpose::FAN_SPEED:
        doc[FPSTR(HA_ICON)] = F("mdi:fan");
        break;

      case Sensors::Purpose::PRESSURE:
        doc[FPSTR(HA_ICON)] = F("mdi:gauge");
        break;

      case Sensors::Purpose::HUMIDITY:
        doc[FPSTR(HA_ICON)] = F("mdi:water-percent");
        break;

      default:
        break;
    }

    String objId = Sensors::makeObjectId(sSensor.name);

    // state topic
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(
      F("sensors"),
      objId.c_str()
    );

    // set device class, name, value template for bluetooth sensors
    // or name & value template for another sensors
    if (sSensor.type == Sensors::Type::BLUETOOTH) {
      // available state topic
      doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = doc[FPSTR(HA_STATE_TOPIC)];
      doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = JsonString(AVAILABILITY_SENSOR_CONN, true);

      String sName = sSensor.name;
      switch (vType) {
        case Sensors::ValueType::TEMPERATURE:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, F("temp"));
          sName += F(" temperature");

          doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);
          if (unit == UnitSystem::METRIC) {
            doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
            
          } else if (unit == UnitSystem::IMPERIAL) {
            doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
          }
          doc[FPSTR(HA_NAME)] = sName;
          doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperature|float(0)|round(2) }}");
          break;

        case Sensors::ValueType::HUMIDITY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, FPSTR(S_HUMIDITY));
          sName += F(" humidity");

          doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_HUMIDITY);
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_PERCENT);
          doc[FPSTR(HA_NAME)] = sName;
          doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.humidity|float(0)|round(2) }}");
          break;

        case Sensors::ValueType::BATTERY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, FPSTR(S_BATTERY));
          sName += F(" battery");
          
          doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_BATTERY);
          doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_PERCENT);
          doc[FPSTR(HA_NAME)] = sName;
          doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.battery|float(0)|round(2) }}");
          break;

        case Sensors::ValueType::RSSI:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, FPSTR(S_RSSI));
          sName += F(" RSSI");
          
          doc[FPSTR(HA_DEVICE_CLASS)] = F("signal_strength");
          doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
          doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("dBm");
          doc[FPSTR(HA_NAME)] = sName;
          doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.rssi|float(0)|round(2) }}");
          break;

        default:
          return false;
      }

    } else if (sSensor.type == Sensors::Type::MANUAL) {
      doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
      doc[FPSTR(HA_STEP)] = 0.01f;
      doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);

      doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(
        F("sensors"),
        objId.c_str(),
        F("set")
      );
      doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"value\": {{ value }}}");

      doc[FPSTR(HA_NAME)] = sSensor.name;
      doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.value|float(0)|round(2) }}");

    } else {
      // available state topic
      doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = doc[FPSTR(HA_STATE_TOPIC)];
      doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = JsonString(AVAILABILITY_SENSOR_CONN, true);

      doc[FPSTR(HA_NAME)] = sSensor.name;
      doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.value|float(0)|round(2) }}");
    }

    // object id's
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(objId.c_str());
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];

    const String& configTopic = this->makeConfigTopic(
      sSensor.type == Sensors::Type::MANUAL ? FPSTR(HA_ENTITY_NUMBER) : FPSTR(HA_ENTITY_SENSOR),
      objId.c_str()
    );
    objId.clear();

    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_STATE_CLASS)] = FPSTR(HA_STATE_CLASS_MEASUREMENT);
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(configTopic.c_str(), doc);
  }

  bool deleteDynamicSensor(Sensors::Settings& sSensor, Sensors::ValueType vType = Sensors::ValueType::PRIMARY) {
    String objId;

    if (sSensor.type == Sensors::Type::BLUETOOTH) {
      switch (vType) {
        case Sensors::ValueType::TEMPERATURE:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, F("temp"));
          break;

        case Sensors::ValueType::HUMIDITY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, FPSTR(S_HUMIDITY));
          break;

        case Sensors::ValueType::BATTERY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, FPSTR(S_BATTERY));
          break;

        case Sensors::ValueType::RSSI:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, FPSTR(S_RSSI));
          break;

        default:
          return false;
      }

    } else {
      Sensors::makeObjectId(objId, sSensor.name);
    }

    const String& configTopic = this->makeConfigTopic(
      sSensor.type == Sensors::Type::MANUAL ? FPSTR(HA_ENTITY_NUMBER) : FPSTR(HA_ENTITY_SENSOR),
      objId.c_str()
    );
    objId.clear();

    return this->publish(configTopic.c_str());
  }

  bool publishConnectionDynamicSensor(Sensors::Settings& sSensor, bool enabledByDefault = true) {
    JsonDocument doc;
    String objId = Sensors::makeObjectIdWithSuffix(sSensor.name, F("connected"));

    // object id's
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(objId.c_str());
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];

    // state topic
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(
      F("sensors"),
      Sensors::makeObjectId(sSensor.name).c_str()
    );

    // sensor name
    {
      String sName = sSensor.name;
      sName.trim();
      sName += F(" connected");

      doc[FPSTR(HA_NAME)] = sName;
    }
    
    const String& configTopic = this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), objId.c_str());
    objId.clear();


    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("connectivity");
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.connected, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(configTopic.c_str(), doc);
  }

  bool deleteConnectionDynamicSensor(Sensors::Settings& sSensor) {
    const String& configTopic = this->makeConfigTopic(
      FPSTR(HA_ENTITY_BINARY_SENSOR),
      Sensors::makeObjectIdWithSuffix(sSensor.name, F("connected")).c_str()
    );

    return this->publish(configTopic.c_str());
  }

  bool publishSignalQualityDynamicSensor(Sensors::Settings& sSensor, bool enabledByDefault = true) {
    JsonDocument doc;
    String objId = Sensors::makeObjectIdWithSuffix(sSensor.name, F("signal_quality"));

    // object id's
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(objId.c_str());
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];

    // state topic
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(
      F("sensors"),
      Sensors::makeObjectId(sSensor.name).c_str()
    );

    // sensor name
    {
      String sName = sSensor.name;
      sName.trim();
      sName += F(" signal quality");

      doc[FPSTR(HA_NAME)] = sName;
    }
    
    const String& configTopic = this->makeConfigTopic(FPSTR(HA_ENTITY_SENSOR), objId.c_str());
    objId.clear();


    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    //doc[FPSTR(HA_DEVICE_CLASS)] = F("signal_strength");
    doc[FPSTR(HA_STATE_CLASS)] = FPSTR(HA_STATE_CLASS_MEASUREMENT);
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_PERCENT);
    doc[FPSTR(HA_ICON)] = F("mdi:signal");
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.signalQuality|float(0)|round(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(configTopic.c_str(), doc);
  }

  bool deleteSignalQualityDynamicSensor(Sensors::Settings& sSensor) {
    JsonDocument doc;
    const String& configTopic = this->makeConfigTopic(
      FPSTR(HA_ENTITY_SENSOR),
      Sensors::makeObjectIdWithSuffix(sSensor.name, F("signal_quality")).c_str()
    );

    return this->publish(configTopic.c_str());
  }


  bool publishSwitchHeatingTurbo(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("heating_turbo"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("Turbo heating");
    doc[FPSTR(HA_ICON)] = F("mdi:rocket-launch-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.turbo }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"heating\": {\"turbo\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"heating\": {\"turbo\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SWITCH), F("heating_turbo")).c_str(), doc);
  }

  bool publishInputHeatingHysteresis(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("heating_hysteresis"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Heating hysteresis");
    doc[FPSTR(HA_ICON)] = F("mdi:altimeter");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.hysteresis|float(0)|round(2) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"hysteresis\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 15;
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_hysteresis")).c_str(), doc);
  }

  bool publishInputHeatingTurboFactor(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("heating_turbo_factor"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("power_factor");
    doc[FPSTR(HA_NAME)] = F("Heating turbo factor");
    doc[FPSTR(HA_ICON)] = F("mdi:multiplication-box");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.turboFactor|float(0)|round(2) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"turboFactor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 1.5;
    doc[FPSTR(HA_MAX)] = 10;
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_turbo_factor")).c_str(), doc);
  }

  bool publishInputHeatingMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("heating_min_temp"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = 0;
      doc[FPSTR(HA_MAX)] = 99;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = 32;
      doc[FPSTR(HA_MAX)] = 211;
    }

    doc[FPSTR(HA_NAME)] = F("Heating min temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-down");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.minTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"minTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_min_temp")).c_str(), doc);
  }

  bool publishInputHeatingMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("heating_max_temp"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = 1;
      doc[FPSTR(HA_MAX)] = 100;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = 33;
      doc[FPSTR(HA_MAX)] = 212;
    }

    doc[FPSTR(HA_NAME)] = F("Heating max temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-up");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.maxTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"maxTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_max_temp")).c_str(), doc);
  }


  bool publishInputDhwMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("dhw_min_temp"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = 0;
      doc[FPSTR(HA_MAX)] = 99;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = 32;
      doc[FPSTR(HA_MAX)] = 211;
    }

    doc[FPSTR(HA_NAME)] = F("DHW min temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-down");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.dhw.minTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"dhw\": {\"minTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_min_temp")).c_str(), doc);
  }

  bool publishInputDhwMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("dhw_max_temp"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = 1;
      doc[FPSTR(HA_MAX)] = 100;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = 33;
      doc[FPSTR(HA_MAX)] = 212;
    }

    doc[FPSTR(HA_NAME)] = F("DHW max temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-up");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.dhw.maxTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"dhw\": {\"maxTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_max_temp")).c_str(), doc);
  }


  bool publishSwitchPid(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("pid"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("PID");
    doc[FPSTR(HA_ICON)] = F("mdi:chart-bar-stacked");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.enabled }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"pid\": {\"enabled\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"pid\": {\"enabled\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SWITCH), F("pid")).c_str(), doc);
  }

  bool publishInputPidFactorP(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("pid_p"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("PID factor P");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-p-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.p_factor|float(0)|round(3) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"p_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0.1f;
    doc[FPSTR(HA_MAX)] = 1000;
    doc[FPSTR(HA_STEP)] = 0.1f;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_p_factor")).c_str(), doc);
  }

  bool publishInputPidFactorI(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("pid_i"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("PID factor I");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-i-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.i_factor|float(0)|round(4) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"i_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 100;
    doc[FPSTR(HA_STEP)] = 0.001f;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_i_factor")).c_str(), doc);
  }

  bool publishInputPidFactorD(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("pid_d"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("PID factor D");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-d-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.d_factor|float(0)|round(3) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"d_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 100000;
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_d_factor")).c_str(), doc);
  }

  bool publishInputPidDt(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("pid_dt"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("duration");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("s");
    doc[FPSTR(HA_NAME)] = F("PID DT");
    doc[FPSTR(HA_ICON)] = F("mdi:timer-cog-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.dt|int(0) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"dt\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 30;
    doc[FPSTR(HA_MAX)] = 1800;
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_dt")).c_str(), doc);
  }

  bool publishInputPidMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("pid_min_temp"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = -99;
      doc[FPSTR(HA_MAX)] = 99;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = -146;
      doc[FPSTR(HA_MAX)] = 211;
    }

    doc[FPSTR(HA_NAME)] = F("PID min temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-down");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.minTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"minTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_min_temp")).c_str(), doc);
  }

  bool publishInputPidMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("pid_max_temp"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_TEMPERATURE);

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = 1;
      doc[FPSTR(HA_MAX)] = 100;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = 1;
      doc[FPSTR(HA_MAX)] = 212;
    }

    doc[FPSTR(HA_NAME)] = F("PID max temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-up");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.maxTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"maxTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_max_temp")).c_str(), doc);
  }


  bool publishSwitchEquitherm(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("equitherm"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("Equitherm");
    doc[FPSTR(HA_ICON)] = F("mdi:sun-snowflake-variant");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.enabled }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"equitherm\": {\"enabled\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"equitherm\": {\"enabled\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SWITCH), F("equitherm")).c_str(), doc);
  }

  bool publishInputEquithermFactorN(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("equitherm_n"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("Equitherm factor N");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-n-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.n_factor|float(0)|round(3) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"equitherm\": {\"n_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0.001f;
    doc[FPSTR(HA_MAX)] = 10;
    doc[FPSTR(HA_STEP)] = 0.001f;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("equitherm_n_factor")).c_str(), doc);
  }

  bool publishInputEquithermFactorK(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("equitherm_k"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("Equitherm factor K");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-k-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.k_factor|float(0)|round(2) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"equitherm\": {\"k_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 10;
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("equitherm_k_factor")).c_str(), doc);
  }

  bool publishInputEquithermFactorT(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.pid.enabled, 'offline', 'online') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("equitherm_t"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_NAME)] = F("Equitherm factor T");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-t-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.t_factor|float(0)|round(2) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"equitherm\": {\"t_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 10;
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = FPSTR(HA_MODE_BOX);
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("equitherm_t_factor")).c_str(), doc);
  }


  bool publishStatusState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("status"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Status");
    doc[FPSTR(HA_ICON)] = F("mdi:list-status");
    doc[FPSTR(HA_STATE_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value == 'online', 'OFF', 'ON') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 60;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("status")).c_str(), doc);
  }

  bool publishEmergencyState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("emergency"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Emergency");
    doc[FPSTR(HA_ICON)] = F("mdi:alert-rhombus-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.master.emergency.state, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("emergency")).c_str(), doc);
  }

  bool publishOpenthermConnectedState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("ot_status"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("connectivity");
    doc[FPSTR(HA_NAME)] = F("Opentherm status");
    doc[FPSTR(HA_ICON)] = F("mdi:list-status");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.connected, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("ot_status")).c_str(), doc);
  }

  bool publishHeatingState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("heating"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    //doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("running");
    doc[FPSTR(HA_NAME)] = F("Heating");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.heating.active, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("heating")).c_str(), doc);
  }

  bool publishDhwState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("dhw"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    //doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("running");
    doc[FPSTR(HA_NAME)] = F("DHW");
    doc[FPSTR(HA_ICON)] = F("mdi:faucet");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.dhw.active, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("dhw")).c_str(), doc);
  }

  bool publishFlameState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("flame"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    //doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("running");
    doc[FPSTR(HA_NAME)] = F("Flame");
    doc[FPSTR(HA_ICON)] = F("mdi:gas-burner");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.flame, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("flame")).c_str(), doc);
  }

  bool publishFaultState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("fault"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Fault");
    doc[FPSTR(HA_ICON)] = F("mdi:alert-remove-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.fault.active, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("fault")).c_str(), doc);
  }

  bool publishDiagState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Diagnostic");
    doc[FPSTR(HA_ICON)] = F("mdi:account-wrench");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.diag.active, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC)).c_str(), doc);
  }

  bool publishExternalPumpState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("ext_pump"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("running");
    doc[FPSTR(HA_NAME)] = F("External pump");
    doc[FPSTR(HA_ICON)] = F("mdi:pump");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.master.externalPump.state, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("ext_pump")).c_str(), doc);
  }

  bool publishFaultCode(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.connected and value_json.slave.fault.active, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("fault_code"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_NAME)] = F("Fault code");
    doc[FPSTR(HA_ICON)] = F("mdi:cog-box");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ \"%02d (0x%02X)\"|format(value_json.slave.fault.code, value_json.slave.fault.code) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SENSOR), F("fault_code")).c_str(), doc);
  }

  bool publishDiagCode(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.connected and value_json.slave.fault.active or value_json.slave.diag.active, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("diagnostic_code"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_NAME)] = F("Diagnostic code");
    doc[FPSTR(HA_ICON)] = F("mdi:information-box");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ \"%02d (0x%02X)\"|format(value_json.slave.diag.code, value_json.slave.diag.code) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SENSOR), F("diagnostic_code")).c_str(), doc);
  }

  bool publishNetworkRssi(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(FPSTR(S_RSSI));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("signal_strength");
    doc[FPSTR(HA_STATE_CLASS)] = FPSTR(HA_STATE_CLASS_MEASUREMENT);
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("dBm");
    doc[FPSTR(HA_NAME)] = F("RSSI");
    doc[FPSTR(HA_ICON)] = F("mdi:signal");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.master.network.rssi|float(0)|round(1) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SENSOR), FPSTR(S_RSSI)).c_str(), doc);
  }

  bool publishUptime(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("uptime"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("duration");
    doc[FPSTR(HA_STATE_CLASS)] = F("total_increasing");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("s");
    doc[FPSTR(HA_NAME)] = F("Uptime");
    doc[FPSTR(HA_ICON)] = F("mdi:clock-start");
    doc[FPSTR(HA_STATE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.master.uptime|int(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SENSOR), F("uptime")).c_str(), doc);
  }


  bool publishClimateHeating(UnitSystem unit = UnitSystem::METRIC, uint8_t minTemp = 20, uint8_t maxTemp = 90, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("heating"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_NAME)] = F("Heating");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");

    doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_CURRENT_TEMPERATURE_TEMPLATE)] = F("{{ iif(value_json.master.heating.indoorTempControl, value_json.master.heating.indoorTemp, value_json.master.heating.currentTemp, 0)|float(0)|round(2) }}");

    doc[FPSTR(HA_TEMPERATURE_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_TEMPERATURE_COMMAND_TEMPLATE)] = F("{\"heating\": {\"target\" : {{ value }}}}");

    doc[FPSTR(HA_TEMPERATURE_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_TEMPERATURE_STATE_TEMPLATE)] = F("{{ value_json.heating.target|float(0)|round(1) }}");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "C";
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "F";
    }

    doc[FPSTR(HA_MODE_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_MODE_COMMAND_TEMPLATE)] = F("{% if value == 'heat' %}{\"heating\": {\"enabled\" : true}}"
      "{% elif value == 'off' %}{\"heating\": {\"enabled\" : false}}{% endif %}");
    doc[FPSTR(HA_MODE_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_MODE_STATE_TEMPLATE)] = F("{{ iif(value_json.heating.enabled, 'heat', 'off') }}");
    doc[FPSTR(HA_MODES)][0] = F("off");
    doc[FPSTR(HA_MODES)][1] = F("heat");

    doc[FPSTR(HA_ACTION_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_ACTION_TEMPLATE)] = F("{{ iif(value_json.slave.heating.active, 'heating', 'idle') }}");

    doc[FPSTR(HA_PRESET_MODE_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_PRESET_MODE_COMMAND_TEMPLATE)] = F("{% if value == 'boost' %}{\"heating\": {\"turbo\" : true}}"
      "{% elif value == 'none' %}{\"heating\": {\"turbo\" : false}}{% endif %}");
    doc[FPSTR(HA_PRESET_MODE_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_PRESET_MODE_VALUE_TEMPLATE)] = F("{{ iif(value_json.heating.turbo, 'boost', 'none') }}");
    doc[FPSTR(HA_PRESET_MODES)][0] = F("boost");

    doc[FPSTR(HA_MIN_TEMP)] = minTemp;
    doc[FPSTR(HA_MAX_TEMP)] = maxTemp;
    doc[FPSTR(HA_TEMP_STEP)] = 0.1f;
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_CLIMATE), F("heating"), '_').c_str(), doc);
  }

  bool publishClimateDhw(UnitSystem unit = UnitSystem::METRIC, uint8_t minTemp = 40, uint8_t maxTemp = 60, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("dhw"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_NAME)] = F("DHW");
    doc[FPSTR(HA_ICON)] = F("mdi:faucet");

    doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_CURRENT_TEMPERATURE_TEMPLATE)] = F("{{ value_json.master.dhw.currentTemp|float(0)|round(1) }}");

    doc[FPSTR(HA_TEMPERATURE_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_TEMPERATURE_COMMAND_TEMPLATE)] = F("{\"dhw\": {\"target\" : {{ value|int(0) }}}}");

    doc[FPSTR(HA_TEMPERATURE_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_TEMPERATURE_STATE_TEMPLATE)] = F("{{ value_json.dhw.target|float(0)|round(1) }}");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "C";
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "F";
    }

    doc[FPSTR(HA_MODE_COMMAND_TOPIC)] = this->setSettingsTopic.c_str();
    doc[FPSTR(HA_MODE_COMMAND_TEMPLATE)] = F("{% if value == 'heat' %}{\"dhw\": {\"enabled\" : true}}"
      "{% elif value == 'off' %}{\"dhw\": {\"enabled\" : false}}{% endif %}");
    doc[FPSTR(HA_MODE_STATE_TOPIC)] = this->settingsTopic.c_str();
    doc[FPSTR(HA_MODE_STATE_TEMPLATE)] = F("{{ iif(value_json.dhw.enabled, 'heat', 'off') }}");
    doc[FPSTR(HA_MODES)][0] = F("off");
    doc[FPSTR(HA_MODES)][1] = F("heat");

    doc[FPSTR(HA_ACTION_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_ACTION_TEMPLATE)] = F("{{ iif(value_json.slave.dhw.active, 'heating', 'idle') }}");

    doc[FPSTR(HA_MIN_TEMP)] = minTemp;
    doc[FPSTR(HA_MAX_TEMP)] = maxTemp;
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_CLIMATE), F("dhw"), '_').c_str(), doc);
  }


  bool publishRestartButton(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(FPSTR(S_RESTART));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_RESTART);
    doc[FPSTR(HA_NAME)] = F("Restart");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setStateTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"actions\": {\"restart\": true}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BUTTON), FPSTR(S_RESTART)).c_str(), doc);
  }

  bool publishResetFaultButton(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.fault.active, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("reset_fault"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_RESTART);
    doc[FPSTR(HA_NAME)] = F("Reset fault");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setStateTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"actions\": {\"resetFault\": true}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BUTTON), F("reset_fault")).c_str(), doc);
  }

  bool publishResetDiagButton(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->statusTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->stateTopic.c_str();
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.slave.diag.active, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectIdWithPrefix(F("reset_diagnostic"));
    doc[FPSTR(HA_DEFAULT_ENTITY_ID)] = doc[FPSTR(HA_UNIQUE_ID)];
    doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(HA_ENTITY_CATEGORY_CONFIG);
    doc[FPSTR(HA_DEVICE_CLASS)] = FPSTR(S_RESTART);
    doc[FPSTR(HA_NAME)] = F("Reset diagnostic");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->setStateTopic.c_str();
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"actions\": {\"resetDiagnostic\": true}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BUTTON), F("reset_diagnostic")).c_str(), doc);
  }


  template <class CT>
  bool deleteEntities(CT category) {
    return this->publish(this->makeConfigTopic(category).c_str());
  }

  bool deleteSwitchDhw() {
    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_SWITCH), F("dhw")).c_str());
  }

  bool deleteInputDhwMinTemp() {
    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_min_temp")).c_str());
  }

  bool deleteInputDhwMaxTemp() {
    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_max_temp")).c_str());
  }

  bool deleteDhwState() {
    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("dhw")).c_str());
  }
  
  bool deleteInputDhwTarget() {
    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_target")).c_str());
  }

  bool deleteClimateDhw() {
    return this->publish(this->makeConfigTopic(FPSTR(HA_ENTITY_CLIMATE), F("dhw"), '_').c_str());
  }

  protected:
    unsigned short expireAfter = 300u;
    String statusTopic, stateTopic, setStateTopic, settingsTopic, setSettingsTopic;
};

const char HaHelper::AVAILABILITY_OT_CONN[] = "{{ iif(value_json.slave.connected, 'online', 'offline') }}";
const char HaHelper::AVAILABILITY_SENSOR_CONN[] = "{{ iif(value_json.connected, 'online', 'offline') }}";