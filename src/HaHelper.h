#pragma once
#include <HomeAssistantHelper.h>

class HaHelper : public HomeAssistantHelper {
public:
  static const uint8_t TEMP_SOURCE_HEATING = 0;
  static const uint8_t TEMP_SOURCE_INDOOR = 1;
  static const char AVAILABILITY_OT_CONN[];
  static const char AVAILABILITY_SENSOR_CONN[];

  void updateCachedTopics() {
    this->statusTopic = this->getDeviceTopic("status");
    this->stateTopic = this->getDeviceTopic("state");
    this->setStateTopic = this->getDeviceTopic("state/set");
    this->settingsTopic = this->getDeviceTopic("settings");
    this->setSettingsTopic = this->getDeviceTopic("settings/set");
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
        doc[HA_DEVICE_CLASS] = S_TEMPERATURE;
        if (unit == UnitSystem::METRIC) {
          doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;

          if (sSensor.type == Sensors::Type::MANUAL) {
            doc[HA_MIN] = -99;
            doc[HA_MAX] = 99;
          }

        } else if (unit == UnitSystem::IMPERIAL) {
          doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;

          if (sSensor.type == Sensors::Type::MANUAL) {
            doc[HA_MIN] = -147;
            doc[HA_MAX] = 211;
          }
        }
        break;

      case Sensors::Purpose::DHW_FLOW_RATE:
        doc[HA_DEVICE_CLASS] = "volume_flow_rate";
        if (unit == UnitSystem::METRIC) {
          doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_L_MIN;

        } else if (unit == UnitSystem::IMPERIAL) {
          doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_GAL_MIN;
        }
        break;

      case Sensors::Purpose::MODULATION_LEVEL:
      case Sensors::Purpose::POWER_FACTOR:
        doc[HA_DEVICE_CLASS] = "power_factor";
        doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_PERCENT;
        break;

      case Sensors::Purpose::POWER:
        doc[HA_DEVICE_CLASS] = "power";
        doc[HA_UNIT_OF_MEASUREMENT] = "kW";
        break;

      case Sensors::Purpose::FAN_SPEED:
        doc[HA_UNIT_OF_MEASUREMENT] = "RPM";
        break;

      case Sensors::Purpose::CO2:
        doc[HA_DEVICE_CLASS] = "carbon_dioxide";
        doc[HA_UNIT_OF_MEASUREMENT] = "ppm";
        break;

      case Sensors::Purpose::PRESSURE:
        doc[HA_DEVICE_CLASS] = "pressure";
        if (unit == UnitSystem::METRIC) {
          doc[HA_UNIT_OF_MEASUREMENT] = "bar";

        } else if (unit == UnitSystem::IMPERIAL) {
          doc[HA_UNIT_OF_MEASUREMENT] = "psi";
        }
        break;

      case Sensors::Purpose::HUMIDITY:
        doc[HA_DEVICE_CLASS] = S_HUMIDITY;
        doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_PERCENT;
        break;

      default:
        break;
    }

    // set icon
    switch (sSensor.purpose) {
      case Sensors::Purpose::OUTDOOR_TEMP:
        doc[HA_ICON] = "mdi:home-thermometer-outline";
        break;

      case Sensors::Purpose::INDOOR_TEMP:
        doc[HA_ICON] = "mdi:home-thermometer";
        break;

      case Sensors::Purpose::HEATING_TEMP:
        doc[HA_ICON] = "mdi:radiator";
        break;

      case Sensors::Purpose::HEATING_RETURN_TEMP:
        doc[HA_ICON] = "mdi:heating-coil";
        break;

      case Sensors::Purpose::DHW_TEMP:
        doc[HA_ICON] = "mdi:faucet";
        break;

      case Sensors::Purpose::DHW_RETURN_TEMP:
        doc[HA_ICON] = "mdi:heating-coil";
        break;

      case Sensors::Purpose::EXHAUST_TEMP:
        doc[HA_ICON] = "mdi:smoke";
        break;

      case Sensors::Purpose::TEMPERATURE:
        doc[HA_ICON] = "mdi:thermometer-lines";
        break;

      case Sensors::Purpose::DHW_FLOW_RATE:
        doc[HA_ICON] = "mdi:faucet";
        break;

      case Sensors::Purpose::MODULATION_LEVEL:
        doc[HA_ICON] = "mdi:fire-circle";
        break;

      case Sensors::Purpose::POWER_FACTOR:
      case Sensors::Purpose::POWER:
        doc[HA_ICON] = "mdi:chart-bar";
        break;

      case Sensors::Purpose::FAN_SPEED:
        doc[HA_ICON] = "mdi:fan";
        break;

      case Sensors::Purpose::PRESSURE:
        doc[HA_ICON] = "mdi:gauge";
        break;

      case Sensors::Purpose::HUMIDITY:
        doc[HA_ICON] = "mdi:water-percent";
        break;

      default:
        break;
    }

    String objId = Sensors::makeObjectId(sSensor.name);

    // state topic
    doc[HA_STATE_TOPIC] = this->getDeviceTopic(
      "sensors",
      objId.c_str()
    );

    // set device class, name, value template for Bluetooth/DHT sensors
    // or name & value template for another sensors
    if (sSensor.type == Sensors::Type::BLUETOOTH ||
        sSensor.type == Sensors::Type::DHT11 || sSensor.type == Sensors::Type::DHT22) {
      // available state topic
      doc[HA_AVAILABILITY][1][HA_TOPIC] = doc[HA_STATE_TOPIC];
      doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = JsonString(AVAILABILITY_SENSOR_CONN, true);

      String sName = sSensor.name;
      switch (vType) {
        case Sensors::ValueType::TEMPERATURE:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, "temp");
          sName += " temperature";

          doc[HA_DEVICE_CLASS] = S_TEMPERATURE;
          if (unit == UnitSystem::METRIC) {
            doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;

          } else if (unit == UnitSystem::IMPERIAL) {
            doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
          }
          doc[HA_NAME] = sName;
          doc[HA_VALUE_TEMPLATE] = "{{ value_json.temperature|float(0)|round(2) }}";
          break;

        case Sensors::ValueType::HUMIDITY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, S_HUMIDITY);
          sName += " humidity";

          doc[HA_DEVICE_CLASS] = S_HUMIDITY;
          doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_PERCENT;
          doc[HA_NAME] = sName;
          doc[HA_VALUE_TEMPLATE] = "{{ value_json.humidity|float(0)|round(2) }}";
          break;

        case Sensors::ValueType::BATTERY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, S_BATTERY);
          sName += " battery";

          doc[HA_DEVICE_CLASS] = S_BATTERY;
          doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
          doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_PERCENT;
          doc[HA_NAME] = sName;
          doc[HA_VALUE_TEMPLATE] = "{{ value_json.battery|float(0)|round(2) }}";
          break;

        case Sensors::ValueType::RSSI:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, S_RSSI);
          sName += " RSSI";

          doc[HA_DEVICE_CLASS] = "signal_strength";
          doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
          doc[HA_UNIT_OF_MEASUREMENT] = "dBm";
          doc[HA_NAME] = sName;
          doc[HA_VALUE_TEMPLATE] = "{{ value_json.rssi|float(0)|round(2) }}";
          break;

        default:
          return false;
      }

    } else if (sSensor.type == Sensors::Type::MANUAL) {
      doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
      doc[HA_STEP] = 0.01f;
      doc[HA_MODE] = HA_MODE_BOX;

      doc[HA_COMMAND_TOPIC] = this->getDeviceTopic(
        "sensors",
        objId.c_str(),
        "set"
      );
      doc[HA_COMMAND_TEMPLATE] = "{\"value\": {{ value }}}";

      doc[HA_NAME] = sSensor.name;
      doc[HA_VALUE_TEMPLATE] = "{{ value_json.value|float(0)|round(2) }}";

    } else {
      // available state topic
      doc[HA_AVAILABILITY][1][HA_TOPIC] = doc[HA_STATE_TOPIC];
      doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = JsonString(AVAILABILITY_SENSOR_CONN, true);

      doc[HA_NAME] = sSensor.name;
      doc[HA_VALUE_TEMPLATE] = "{{ value_json.value|float(0)|round(2) }}";
    }

    // object id's
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix(objId.c_str());
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(
      sSensor.type == Sensors::Type::MANUAL
        ? HA_ENTITY_NUMBER
        : HA_ENTITY_SENSOR,
      objId.c_str()
    );

    const String& configTopic = this->makeConfigTopic(
      sSensor.type == Sensors::Type::MANUAL ? HA_ENTITY_NUMBER : HA_ENTITY_SENSOR,
      objId.c_str()
    );
    objId.clear();

    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_STATE_CLASS] = HA_STATE_CLASS_MEASUREMENT;
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(configTopic.c_str(), doc);
  }

  bool deleteDynamicSensor(Sensors::Settings& sSensor, Sensors::ValueType vType = Sensors::ValueType::PRIMARY) {
    String objId;

    if (sSensor.type == Sensors::Type::BLUETOOTH) {
      switch (vType) {
        case Sensors::ValueType::TEMPERATURE:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, "temp");
          break;

        case Sensors::ValueType::HUMIDITY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, S_HUMIDITY);
          break;

        case Sensors::ValueType::BATTERY:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, S_BATTERY);
          break;

        case Sensors::ValueType::RSSI:
          Sensors::makeObjectIdWithSuffix(objId, sSensor.name, S_RSSI);
          break;

        default:
          return false;
      }

    } else {
      Sensors::makeObjectId(objId, sSensor.name);
    }

    const String& configTopic = this->makeConfigTopic(
      sSensor.type == Sensors::Type::MANUAL ? HA_ENTITY_NUMBER : HA_ENTITY_SENSOR,
      objId.c_str()
    );
    objId.clear();

    return this->publish(configTopic.c_str());
  }

  bool publishConnectionDynamicSensor(Sensors::Settings& sSensor, bool enabledByDefault = true) {
    JsonDocument doc;
    String objId = Sensors::makeObjectIdWithSuffix(sSensor.name, "connected");

    // object id's
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix(objId.c_str());
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, objId.c_str());

    // state topic
    doc[HA_STATE_TOPIC] = this->getDeviceTopic(
      "sensors",
      Sensors::makeObjectId(sSensor.name).c_str()
    );

    // sensor name
    {
      String sName = sSensor.name;
      sName.trim();
      sName += " connected";

      doc[HA_NAME] = sName;
    }

    const String& configTopic = this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, objId.c_str());
    objId.clear();


    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "connectivity";
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.connected, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(configTopic.c_str(), doc);
  }

  bool deleteConnectionDynamicSensor(Sensors::Settings& sSensor) {
    const String& configTopic = this->makeConfigTopic(
      HA_ENTITY_BINARY_SENSOR,
      Sensors::makeObjectIdWithSuffix(sSensor.name, "connected").c_str()
    );

    return this->publish(configTopic.c_str());
  }

  bool publishSignalQualityDynamicSensor(Sensors::Settings& sSensor, bool enabledByDefault = true) {
    JsonDocument doc;
    String objId = Sensors::makeObjectIdWithSuffix(sSensor.name, "signal_quality");

    // object id's
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix(objId.c_str());
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SENSOR, objId.c_str());

    // state topic
    doc[HA_STATE_TOPIC] = this->getDeviceTopic(
      "sensors",
      Sensors::makeObjectId(sSensor.name).c_str()
    );

    // sensor name
    {
      String sName = sSensor.name;
      sName.trim();
      sName += " signal quality";

      doc[HA_NAME] = sName;
    }

    const String& configTopic = this->makeConfigTopic(HA_ENTITY_SENSOR, objId.c_str());
    objId.clear();


    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    //doc[HA_DEVICE_CLASS] = "signal_strength";
    doc[HA_STATE_CLASS] = HA_STATE_CLASS_MEASUREMENT;
    doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_PERCENT;
    doc[HA_ICON] = "mdi:signal";
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.signalQuality|float(0)|round(0) }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(configTopic.c_str(), doc);
  }

  bool deleteSignalQualityDynamicSensor(Sensors::Settings& sSensor) {
    const String& configTopic = this->makeConfigTopic(
      HA_ENTITY_SENSOR,
      Sensors::makeObjectIdWithSuffix(sSensor.name, "signal_quality").c_str()
    );

    return this->publish(configTopic.c_str());
  }


  bool publishSwitchHeatingTurbo(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating_turbo");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SWITCH, "heating_turbo");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "Turbo heating";
    doc[HA_ICON] = "mdi:rocket-launch-outline";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_STATE_ON] = true;
    doc[HA_STATE_OFF] = false;
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.heating.turbo }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_PAYLOAD_ON] = "{\"heating\": {\"turbo\" : true}}";
    doc[HA_PAYLOAD_OFF] = "{\"heating\": {\"turbo\" : false}}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SWITCH, "heating_turbo").c_str(), doc);
  }

  bool publishSwitchHeatingHysteresis(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating_hysteresis");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SWITCH, "heating_hysteresis");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "Use heating hysteresis";
    doc[HA_ICON] = "mdi:altimeter";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_STATE_ON] = true;
    doc[HA_STATE_OFF] = false;
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.heating.hysteresis.enabled }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_PAYLOAD_ON] = "{\"heating\": {\"hysteresis\" : {\"enabled\" : true}}}";
    doc[HA_PAYLOAD_OFF] = "{\"heating\": {\"hysteresis\" : {\"enabled\" : false}}}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SWITCH, "heating_hysteresis").c_str(), doc);
  }

  bool publishInputHeatingHysteresis(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating_hysteresis");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "heating_hysteresis");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;

    if (unit == UnitSystem::METRIC) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
    }

    doc[HA_NAME] = "Heating hysteresis";
    doc[HA_ICON] = "mdi:altimeter";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.heating.hysteresis.value|float(0)|round(2) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"heating\": {\"hysteresis\" : {\"value\" : {{ value }}}}}";
    doc[HA_MIN] = 0;
    doc[HA_MAX] = 15;
    doc[HA_STEP] = 0.01f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "heating_hysteresis").c_str(), doc);
  }

  bool publishInputHeatingTurboFactor(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating_turbo_factor");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "heating_turbo_factor");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = "power_factor";
    doc[HA_NAME] = "Heating turbo factor";
    doc[HA_ICON] = "mdi:multiplication-box";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.heating.turboFactor|float(0)|round(2) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"heating\": {\"turboFactor\" : {{ value }}}}";
    doc[HA_MIN] = 1.5;
    doc[HA_MAX] = 10;
    doc[HA_STEP] = 0.01f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "heating_turbo_factor").c_str(), doc);
  }

  bool publishInputHeatingMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating_min_temp");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "heating_min_temp");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;

    if (unit == UnitSystem::METRIC) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;
      doc[HA_MIN] = 0;
      doc[HA_MAX] = 99;

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
      doc[HA_MIN] = 32;
      doc[HA_MAX] = 211;
    }

    doc[HA_NAME] = "Heating min temp";
    doc[HA_ICON] = "mdi:thermometer-chevron-down";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.heating.minTemp|float(0)|round(1) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"heating\": {\"minTemp\" : {{ value }}}}";
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "heating_min_temp").c_str(), doc);
  }

  bool publishInputHeatingMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating_max_temp");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "heating_max_temp");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;

    if (unit == UnitSystem::METRIC) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;
      doc[HA_MIN] = 1;
      doc[HA_MAX] = 100;

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
      doc[HA_MIN] = 33;
      doc[HA_MAX] = 212;
    }

    doc[HA_NAME] = "Heating max temp";
    doc[HA_ICON] = "mdi:thermometer-chevron-up";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.heating.maxTemp|float(0)|round(1) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"heating\": {\"maxTemp\" : {{ value }}}}";
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "heating_max_temp").c_str(), doc);
  }


  bool publishInputDhwMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("dhw_min_temp");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "dhw_min_temp");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;

    if (unit == UnitSystem::METRIC) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;
      doc[HA_MIN] = 0;
      doc[HA_MAX] = 99;

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
      doc[HA_MIN] = 32;
      doc[HA_MAX] = 211;
    }

    doc[HA_NAME] = "DHW min temp";
    doc[HA_ICON] = "mdi:thermometer-chevron-down";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.dhw.minTemp|float(0)|round(1) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"dhw\": {\"minTemp\" : {{ value }}}}";
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "dhw_min_temp").c_str(), doc);
  }

  bool publishInputDhwMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("dhw_max_temp");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "dhw_max_temp");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;

    if (unit == UnitSystem::METRIC) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;
      doc[HA_MIN] = 1;
      doc[HA_MAX] = 100;

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
      doc[HA_MIN] = 33;
      doc[HA_MAX] = 212;
    }

    doc[HA_NAME] = "DHW max temp";
    doc[HA_ICON] = "mdi:thermometer-chevron-up";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.dhw.maxTemp|float(0)|round(1) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"dhw\": {\"maxTemp\" : {{ value }}}}";
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "dhw_max_temp").c_str(), doc);
  }


  bool publishSwitchPid(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("pid");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SWITCH, "pid");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "PID";
    doc[HA_ICON] = "mdi:chart-bar-stacked";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_STATE_ON] = true;
    doc[HA_STATE_OFF] = false;
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.pid.enabled }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_PAYLOAD_ON] = "{\"pid\": {\"enabled\" : true}}";
    doc[HA_PAYLOAD_OFF] = "{\"pid\": {\"enabled\" : false}}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SWITCH, "pid").c_str(), doc);
  }

  bool publishInputPidFactorP(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("pid_p");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "pid_p");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "PID factor P";
    doc[HA_ICON] = "mdi:alpha-p-circle-outline";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.pid.p_factor|float(0)|round(3) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"pid\": {\"p_factor\" : {{ value }}}}";
    doc[HA_MIN] = 0.1f;
    doc[HA_MAX] = 1000;
    doc[HA_STEP] = 0.1f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "pid_p_factor").c_str(), doc);
  }

  bool publishInputPidFactorI(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("pid_i");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "pid_i");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "PID factor I";
    doc[HA_ICON] = "mdi:alpha-i-circle-outline";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.pid.i_factor|float(0)|round(4) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"pid\": {\"i_factor\" : {{ value }}}}";
    doc[HA_MIN] = 0;
    doc[HA_MAX] = 100;
    doc[HA_STEP] = 0.001f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "pid_i_factor").c_str(), doc);
  }

  bool publishInputPidFactorD(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("pid_d");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "pid_d");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "PID factor D";
    doc[HA_ICON] = "mdi:alpha-d-circle-outline";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.pid.d_factor|float(0)|round(3) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"pid\": {\"d_factor\" : {{ value }}}}";
    doc[HA_MIN] = 0;
    doc[HA_MAX] = 100000;
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "pid_d_factor").c_str(), doc);
  }

  bool publishInputPidDt(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("pid_dt");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "pid_dt");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = "duration";
    doc[HA_UNIT_OF_MEASUREMENT] = "s";
    doc[HA_NAME] = "PID DT";
    doc[HA_ICON] = "mdi:timer-cog-outline";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.pid.dt|int(0) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"pid\": {\"dt\" : {{ value }}}}";
    doc[HA_MIN] = 30;
    doc[HA_MAX] = 1800;
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "pid_dt").c_str(), doc);
  }

  bool publishInputPidMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("pid_min_temp");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "pid_min_temp");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;

    if (unit == UnitSystem::METRIC) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;
      doc[HA_MIN] = -99;
      doc[HA_MAX] = 99;

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
      doc[HA_MIN] = -146;
      doc[HA_MAX] = 211;
    }

    doc[HA_NAME] = "PID min temp";
    doc[HA_ICON] = "mdi:thermometer-chevron-down";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.pid.minTemp|float(0)|round(1) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"pid\": {\"minTemp\" : {{ value }}}}";
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "pid_min_temp").c_str(), doc);
  }

  bool publishInputPidMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("pid_max_temp");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "pid_max_temp");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;

    if (unit == UnitSystem::METRIC) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_C;
      doc[HA_MIN] = 1;
      doc[HA_MAX] = 100;

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_UNIT_OF_MEASUREMENT] = HA_UNIT_OF_MEASUREMENT_F;
      doc[HA_MIN] = 1;
      doc[HA_MAX] = 212;
    }

    doc[HA_NAME] = "PID max temp";
    doc[HA_ICON] = "mdi:thermometer-chevron-up";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.pid.maxTemp|float(0)|round(1) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"pid\": {\"maxTemp\" : {{ value }}}}";
    doc[HA_STEP] = 1;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "pid_max_temp").c_str(), doc);
  }


  bool publishSwitchEquitherm(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("equitherm");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SWITCH, "equitherm");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "Equitherm";
    doc[HA_ICON] = "mdi:sun-snowflake-variant";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_STATE_ON] = true;
    doc[HA_STATE_OFF] = false;
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.equitherm.enabled }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_PAYLOAD_ON] = "{\"equitherm\": {\"enabled\" : true}}";
    doc[HA_PAYLOAD_OFF] = "{\"equitherm\": {\"enabled\" : false}}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SWITCH, "equitherm").c_str(), doc);
  }

  bool publishInputEquithermSlope(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("equitherm_slope");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "equitherm_slope");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "Equitherm slope";
    doc[HA_ICON] = "mdi:slope-uphill";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.equitherm.slope|float(0)|round(3) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"equitherm\": {\"slope\" : {{ value }}}}";
    doc[HA_MIN] = 0.001f;
    doc[HA_MAX] = 10;
    doc[HA_STEP] = 0.001f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "equitherm_slope").c_str(), doc);
  }

  bool publishInputEquithermExponent(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("equitherm_exponent");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "equitherm_exponent");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "Equitherm exponent";
    doc[HA_ICON] = "mdi:exponent";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.equitherm.exponent|float(0)|round(3) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"equitherm\": {\"exponent\" : {{ value }}}}";
    doc[HA_MIN] = 0.1;
    doc[HA_MAX] = 2;
    doc[HA_STEP] = 0.001f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "equitherm_exponent").c_str(), doc);
  }

  bool publishInputEquithermShift(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("equitherm_shift");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "equitherm_shift");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_TEMPERATURE;
    doc[HA_NAME] = "Equitherm shift";
    doc[HA_ICON] = "mdi:chart-areaspline";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.equitherm.shift|float(0)|round(2) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"equitherm\": {\"shift\" : {{ value }}}}";
    doc[HA_MIN] = -15;
    doc[HA_MAX] = 15;
    doc[HA_STEP] = 0.01f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "equitherm_shift").c_str(), doc);
  }

  bool publishInputEquithermTargetDiffFactor(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->settingsTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = "{{ iif(value_json.pid.enabled, 'offline', 'online') }}";
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("equitherm_target_diff_factor");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_NUMBER, "equitherm_target_diff_factor");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_NAME] = "Equitherm target diff factor";
    doc[HA_ICON] = "mdi:chart-timeline-variant-shimmer";
    doc[HA_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.equitherm.targetDiffFactor|float(0)|round(3) }}";
    doc[HA_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"equitherm\": {\"targetDiffFactor\" : {{ value }}}}";
    doc[HA_MIN] = 0;
    doc[HA_MAX] = 10;
    doc[HA_STEP] = 0.001f;
    doc[HA_MODE] = HA_MODE_BOX;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "equitherm_target_diff_factor").c_str(), doc);
  }


  bool publishStatusState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("status");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "status");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "problem";
    doc[HA_NAME] = "Status";
    doc[HA_ICON] = "mdi:list-status";
    doc[HA_STATE_TOPIC] = this->statusTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value == 'online', 'OFF', 'ON') }}";
    doc[HA_EXPIRE_AFTER] = 60;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "status").c_str(), doc);
  }

  bool publishEmergencyState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("emergency");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "emergency");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "problem";
    doc[HA_NAME] = "Emergency";
    doc[HA_ICON] = "mdi:alert-rhombus-outline";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.master.emergency.state, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "emergency").c_str(), doc);
  }

  bool publishOpenthermConnectedState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("ot_status");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "ot_status");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "connectivity";
    doc[HA_NAME] = "Opentherm status";
    doc[HA_ICON] = "mdi:list-status";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.connected, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "ot_status").c_str(), doc);
  }

  bool publishHeatingState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "heating");
    doc[HA_DEVICE_CLASS] = "running";
    doc[HA_NAME] = "Heating";
    doc[HA_ICON] = "mdi:radiator";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.heating.active, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "heating").c_str(), doc);
  }

  bool publishDhwState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("dhw");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "dhw");
    doc[HA_DEVICE_CLASS] = "running";
    doc[HA_NAME] = "DHW";
    doc[HA_ICON] = "mdi:faucet";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.dhw.active, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "dhw").c_str(), doc);
  }

  bool publishFlameState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("flame");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "flame");
    doc[HA_DEVICE_CLASS] = "running";
    doc[HA_NAME] = "Flame";
    doc[HA_ICON] = "mdi:gas-burner";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.flame, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "flame").c_str(), doc);
  }

  bool publishFaultState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("fault");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "fault");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "problem";
    doc[HA_NAME] = "Fault";
    doc[HA_ICON] = "mdi:alert-remove-outline";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.fault.active, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "fault").c_str(), doc);
  }

  bool publishDiagState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = JsonString(AVAILABILITY_OT_CONN, true);
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix(HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, HA_ENTITY_CATEGORY_DIAGNOSTIC);
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "problem";
    doc[HA_NAME] = "Diagnostic";
    doc[HA_ICON] = "mdi:account-wrench";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.diag.active, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, HA_ENTITY_CATEGORY_DIAGNOSTIC).c_str(), doc);
  }

  bool publishExternalPumpState(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("ext_pump");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BINARY_SENSOR, "ext_pump");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "running";
    doc[HA_NAME] = "External pump";
    doc[HA_ICON] = "mdi:pump";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ iif(value_json.master.externalPump.state, 'ON', 'OFF') }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "ext_pump").c_str(), doc);
  }

  bool publishFaultCode(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.connected and value_json.slave.fault.active, 'online', 'offline') }}";
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("fault_code");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SENSOR, "fault_code");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_NAME] = "Fault code";
    doc[HA_ICON] = "mdi:cog-box";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ \"%02d (0x%02X)\"|format(value_json.slave.fault.code, value_json.slave.fault.code) }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SENSOR, "fault_code").c_str(), doc);
  }

  bool publishDiagCode(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.connected and value_json.slave.fault.active or value_json.slave.diag.active, 'online', 'offline') }}";
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("diagnostic_code");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SENSOR, "diagnostic_code");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_NAME] = "Diagnostic code";
    doc[HA_ICON] = "mdi:information-box";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ \"%02d (0x%02X)\"|format(value_json.slave.diag.code, value_json.slave.diag.code) }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SENSOR, "diagnostic_code").c_str(), doc);
  }

  bool publishNetworkRssi(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix(S_RSSI);
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SENSOR, S_RSSI);
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "signal_strength";
    doc[HA_STATE_CLASS] = HA_STATE_CLASS_MEASUREMENT;
    doc[HA_UNIT_OF_MEASUREMENT] = "dBm";
    doc[HA_NAME] = "RSSI";
    doc[HA_ICON] = "mdi:signal";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.master.network.rssi|float(0)|round(1) }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SENSOR, S_RSSI).c_str(), doc);
  }

  bool publishUptime(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("uptime");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_SENSOR, "uptime");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_DIAGNOSTIC;
    doc[HA_DEVICE_CLASS] = "duration";
    doc[HA_STATE_CLASS] = "total_increasing";
    doc[HA_UNIT_OF_MEASUREMENT] = "s";
    doc[HA_NAME] = "Uptime";
    doc[HA_ICON] = "mdi:clock-start";
    doc[HA_STATE_TOPIC] = this->stateTopic.c_str();
    doc[HA_VALUE_TEMPLATE] = "{{ value_json.master.uptime|int(0) }}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_SENSOR, "uptime").c_str(), doc);
  }


  bool publishClimateHeating(UnitSystem unit = UnitSystem::METRIC, uint8_t minTemp = 20, uint8_t maxTemp = 90, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("heating");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_CLIMATE, "heating");
    doc[HA_NAME] = "Heating";
    doc[HA_ICON] = "mdi:radiator";

    doc[HA_CURRENT_TEMPERATURE_TOPIC] = this->stateTopic.c_str();
    doc[HA_CURRENT_TEMPERATURE_TEMPLATE] = "{{ iif(value_json.master.heating.indoorTempControl, value_json.master.heating.indoorTemp, value_json.master.heating.currentTemp, 0)|float(0)|round(2) }}";

    doc[HA_TEMPERATURE_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_TEMPERATURE_COMMAND_TEMPLATE] = "{\"heating\": {\"target\" : {{ value }}}}";

    doc[HA_TEMPERATURE_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_TEMPERATURE_STATE_TEMPLATE] = "{{ value_json.heating.target|float(0)|round(1) }}";

    if (unit == UnitSystem::METRIC) {
      doc[HA_TEMPERATURE_UNIT] = "C";

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_TEMPERATURE_UNIT] = "F";
    }

    doc[HA_MODE_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_MODE_COMMAND_TEMPLATE] = "{% if value == 'heat' %}{\"heating\": {\"enabled\" : true}}"
      "{% elif value == 'off' %}{\"heating\": {\"enabled\" : false}}{% endif %}";
    doc[HA_MODE_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_MODE_STATE_TEMPLATE] = "{{ iif(value_json.heating.enabled, 'heat', 'off') }}";
    doc[HA_MODES][0] = "off";
    doc[HA_MODES][1] = "heat";

    doc[HA_ACTION_TOPIC] = this->stateTopic.c_str();
    doc[HA_ACTION_TEMPLATE] = "{{ iif(value_json.slave.heating.active, 'heating', 'idle') }}";

    doc[HA_PRESET_MODE_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_PRESET_MODE_COMMAND_TEMPLATE] = "{% if value == 'boost' %}{\"heating\": {\"turbo\" : true}}"
      "{% elif value == 'none' %}{\"heating\": {\"turbo\" : false}}{% endif %}";
    doc[HA_PRESET_MODE_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_PRESET_MODE_VALUE_TEMPLATE] = "{{ iif(value_json.heating.turbo, 'boost', 'none') }}";
    doc[HA_PRESET_MODES][0] = "boost";

    doc[HA_MIN_TEMP] = minTemp;
    doc[HA_MAX_TEMP] = maxTemp;
    doc[HA_TEMP_STEP] = 0.1f;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_CLIMATE, "heating", '_').c_str(), doc);
  }

  bool publishClimateDhw(UnitSystem unit = UnitSystem::METRIC, uint8_t minTemp = 40, uint8_t maxTemp = 60, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("dhw");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_CLIMATE, "dhw");
    doc[HA_NAME] = "DHW";
    doc[HA_ICON] = "mdi:faucet";

    doc[HA_CURRENT_TEMPERATURE_TOPIC] = this->stateTopic.c_str();
    doc[HA_CURRENT_TEMPERATURE_TEMPLATE] = "{{ value_json.master.dhw.currentTemp|float(0)|round(1) }}";

    doc[HA_TEMPERATURE_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_TEMPERATURE_COMMAND_TEMPLATE] = "{\"dhw\": {\"target\" : {{ value|int(0) }}}}";

    doc[HA_TEMPERATURE_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_TEMPERATURE_STATE_TEMPLATE] = "{{ value_json.dhw.target|float(0)|round(1) }}";

    if (unit == UnitSystem::METRIC) {
      doc[HA_TEMPERATURE_UNIT] = "C";

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[HA_TEMPERATURE_UNIT] = "F";
    }

    doc[HA_MODE_COMMAND_TOPIC] = this->setSettingsTopic.c_str();
    doc[HA_MODE_COMMAND_TEMPLATE] = "{% if value == 'heat' %}{\"dhw\": {\"enabled\" : true}}"
      "{% elif value == 'off' %}{\"dhw\": {\"enabled\" : false}}{% endif %}";
    doc[HA_MODE_STATE_TOPIC] = this->settingsTopic.c_str();
    doc[HA_MODE_STATE_TEMPLATE] = "{{ iif(value_json.dhw.enabled, 'heat', 'off') }}";
    doc[HA_MODES][0] = "off";
    doc[HA_MODES][1] = "heat";

    doc[HA_ACTION_TOPIC] = this->stateTopic.c_str();
    doc[HA_ACTION_TEMPLATE] = "{{ iif(value_json.slave.dhw.active, 'heating', 'idle') }}";

    doc[HA_MIN_TEMP] = minTemp;
    doc[HA_MAX_TEMP] = maxTemp;
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_CLIMATE, "dhw", '_').c_str(), doc);
  }


  bool publishRestartButton(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix(S_RESTART);
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BUTTON, S_RESTART);
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_RESTART;
    doc[HA_NAME] = "Restart";
    doc[HA_COMMAND_TOPIC] = this->setStateTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"actions\": {\"restart\": true}}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BUTTON, S_RESTART).c_str(), doc);
  }

  bool publishResetFaultButton(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.fault.active, 'online', 'offline') }}";
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("reset_fault");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BUTTON, "reset_fault");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_RESTART;
    doc[HA_NAME] = "Reset fault";
    doc[HA_COMMAND_TOPIC] = this->setStateTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"actions\": {\"resetFault\": true}}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BUTTON, "reset_fault").c_str(), doc);
  }

  bool publishResetDiagButton(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[HA_AVAILABILITY][0][HA_TOPIC] = this->statusTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_TOPIC] = this->stateTopic.c_str();
    doc[HA_AVAILABILITY][1][HA_VALUE_TEMPLATE] = "{{ iif(value_json.slave.diag.active, 'online', 'offline') }}";
    doc[HA_AVAILABILITY_MODE] = "all";
    doc[HA_ENABLED_BY_DEFAULT] = enabledByDefault;
    doc[HA_UNIQUE_ID] = this->getUniqueIdWithPrefix("reset_diagnostic");
    doc[HA_DEFAULT_ENTITY_ID] = this->getEntityIdWithPrefix(HA_ENTITY_BUTTON, "reset_diagnostic");
    doc[HA_ENTITY_CATEGORY] = HA_ENTITY_CATEGORY_CONFIG;
    doc[HA_DEVICE_CLASS] = S_RESTART;
    doc[HA_NAME] = "Reset diagnostic";
    doc[HA_COMMAND_TOPIC] = this->setStateTopic.c_str();
    doc[HA_COMMAND_TEMPLATE] = "{\"actions\": {\"resetDiagnostic\": true}}";
    doc[HA_EXPIRE_AFTER] = this->expireAfter;
    doc.shrinkToFit();

    return this->publish(this->makeConfigTopic(HA_ENTITY_BUTTON, "reset_diagnostic").c_str(), doc);
  }


  template <class CT>
  bool deleteEntities(CT category) {
    return this->publish(this->makeConfigTopic(category).c_str());
  }

  bool deleteSwitchDhw() {
    return this->publish(this->makeConfigTopic(HA_ENTITY_SWITCH, "dhw").c_str());
  }

  bool deleteInputDhwMinTemp() {
    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "dhw_min_temp").c_str());
  }

  bool deleteInputDhwMaxTemp() {
    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "dhw_max_temp").c_str());
  }

  bool deleteDhwState() {
    return this->publish(this->makeConfigTopic(HA_ENTITY_BINARY_SENSOR, "dhw").c_str());
  }

  bool deleteInputDhwTarget() {
    return this->publish(this->makeConfigTopic(HA_ENTITY_NUMBER, "dhw_target").c_str());
  }

  bool deleteClimateDhw() {
    return this->publish(this->makeConfigTopic(HA_ENTITY_CLIMATE, "dhw", '_').c_str());
  }

  protected:
    unsigned short expireAfter = 300u;
    String statusTopic, stateTopic, setStateTopic, settingsTopic, setSettingsTopic;
};

const char HaHelper::AVAILABILITY_OT_CONN[] = "{{ iif(value_json.slave.connected, 'online', 'offline') }}";
const char HaHelper::AVAILABILITY_SENSOR_CONN[] = "{{ iif(value_json.connected, 'online', 'offline') }}";
