#pragma once
#include <HomeAssistantHelper.h>

class HaHelper : public HomeAssistantHelper {
public:
  static const byte TEMP_SOURCE_HEATING = 0;
  static const byte TEMP_SOURCE_INDOOR = 1;

  bool publishSwitchHeating(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("Heating");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.enable }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"heating\": {\"enable\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"heating\": {\"enable\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SWITCH), F("heating")).c_str(), doc);
  }

  bool publishSwitchHeatingTurbo(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_turbo"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_turbo"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("Turbo heating");
    doc[FPSTR(HA_ICON)] = F("mdi:rocket-launch-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.turbo }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"heating\": {\"turbo\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"heating\": {\"turbo\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SWITCH), F("heating_turbo")).c_str(), doc);
  }

  bool publishNumberHeatingTarget(UnitSystem unit = UnitSystem::METRIC, byte minTemp = 20, byte maxTemp = 90, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_target"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_target"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Heating target");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.target|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"target\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = minTemp;
    doc[FPSTR(HA_MAX)] = maxTemp;
    doc[FPSTR(HA_STEP)] = 0.5f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_target")).c_str(), doc);
  }

  bool publishNumberHeatingHysteresis(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_hysteresis"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_hysteresis"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Heating hysteresis");
    doc[FPSTR(HA_ICON)] = F("mdi:altimeter");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.hysteresis|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"hysteresis\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 5;
    doc[FPSTR(HA_STEP)] = 0.1f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_hysteresis")).c_str(), doc);
  }

  bool publishSensorHeatingSetpoint(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_setpoint"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_setpoint"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Heating setpoint");
    doc[FPSTR(HA_ICON)] = F("mdi:coolant-temperature");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.parameters.heatingSetpoint|float(0)|round(1) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("heating_setpoint")).c_str(), doc);
  }

  bool publishSensorBoilerHeatingMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("boiler_heating_min_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("boiler_heating_min_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Boiler heating min temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-down");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.parameters.heatingMinTemp|int(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("boiler_heating_min_temp")).c_str(), doc);
  }

  bool publishSensorBoilerHeatingMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("boiler_heating_max_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("boiler_heating_max_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Boiler heating max temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-up");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.parameters.heatingMaxTemp|int(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("boiler_heating_max_temp")).c_str(), doc);
  }

  bool publishNumberHeatingMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_min_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_min_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

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
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.minTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"minTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_min_temp")).c_str(), doc);
  }

  bool publishNumberHeatingMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_max_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_max_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

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
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.heating.maxTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"heating\": {\"maxTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("heating_max_temp")).c_str(), doc);
  }


  bool publishSwitchDhw(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("DHW");
    doc[FPSTR(HA_ICON)] = F("mdi:water-pump");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.dhw.enable }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"dhw\": {\"enable\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"dhw\": {\"enable\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SWITCH), F("dhw")).c_str(), doc);
  }

  bool publishNumberDhwTarget(UnitSystem unit = UnitSystem::METRIC, byte minTemp = 40, byte maxTemp = 60, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw_target"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw_target"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("DHW target");
    doc[FPSTR(HA_ICON)] = F("mdi:water-pump");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.dhw.target|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"dhw\": {\"target\" : {{ value|int(0) }}}}");
    doc[FPSTR(HA_MIN)] = minTemp;
    doc[FPSTR(HA_MAX)] = maxTemp > minTemp ? maxTemp : minTemp;
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_target")).c_str(), doc);
  }

  bool publishSensorBoilerDhwMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("boiler_dhw_min_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("boiler_dhw_min_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Boiler DHW min temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-down");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.parameters.dhwMinTemp|int(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("boiler_dhw_min_temp")).c_str(), doc);
  }

  bool publishSensorBoilerDhwMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("boiler_dhw_max_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("boiler_dhw_max_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Boiler DHW max temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-up");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.parameters.dhwMaxTemp|int(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("boiler_dhw_max_temp")).c_str(), doc);
  }

  bool publishNumberDhwMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw_min_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw_min_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

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
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.dhw.minTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"dhw\": {\"minTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_min_temp")).c_str(), doc);
  }

  bool publishNumberDhwMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw_max_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw_max_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

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
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.dhw.maxTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"dhw\": {\"maxTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_max_temp")).c_str(), doc);
  }


  bool publishSwitchPid(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pid"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pid"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("PID");
    doc[FPSTR(HA_ICON)] = F("mdi:chart-bar-stacked");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.enable }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"pid\": {\"enable\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"pid\": {\"enable\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SWITCH), F("pid")).c_str(), doc);
  }

  bool publishNumberPidFactorP(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pid_p"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pid_p"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("PID factor P");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-p-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.p_factor|float(0)|round(3) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"p_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0.1f;
    doc[FPSTR(HA_MAX)] = 1000;
    doc[FPSTR(HA_STEP)] = 0.1f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_p_factor")).c_str(), doc);
  }

  bool publishNumberPidFactorI(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pid_i"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pid_i"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("PID factor I");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-i-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.i_factor|float(0)|round(4) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"i_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 100;
    doc[FPSTR(HA_STEP)] = 0.001f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_i_factor")).c_str(), doc);
  }

  bool publishNumberPidFactorD(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pid_d"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pid_d"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("PID factor D");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-d-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.d_factor|float(0)|round(3) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"d_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 100000;
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_d_factor")).c_str(), doc);
  }

  bool publishNumberPidDt(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pid_dt"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pid_dt"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("duration");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("s");
    doc[FPSTR(HA_NAME)] = F("PID DT");
    doc[FPSTR(HA_ICON)] = F("mdi:timer-cog-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.dt|int(0) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"dt\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 30;
    doc[FPSTR(HA_MAX)] = 1800;
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_dt")).c_str(), doc);
  }

  bool publishNumberPidMinTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pid_min_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pid_min_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = 0;
      doc[FPSTR(HA_MAX)] = 99;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = 0;
      doc[FPSTR(HA_MAX)] = 211;
    }

    doc[FPSTR(HA_NAME)] = F("PID min temp");
    doc[FPSTR(HA_ICON)] = F("mdi:thermometer-chevron-down");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.minTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"minTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_min_temp")).c_str(), doc);
  }

  bool publishNumberPidMaxTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pid_max_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pid_max_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");

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
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.pid.maxTemp|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"pid\": {\"maxTemp\" : {{ value }}}}");
    doc[FPSTR(HA_STEP)] = 1;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("pid_max_temp")).c_str(), doc);
  }


  bool publishSwitchEquitherm(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("equitherm"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("equitherm"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("Equitherm");
    doc[FPSTR(HA_ICON)] = F("mdi:sun-snowflake-variant");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_STATE_ON)] = true;
    doc[FPSTR(HA_STATE_OFF)] = false;
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.enable }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_PAYLOAD_ON)] = F("{\"equitherm\": {\"enable\" : true}}");
    doc[FPSTR(HA_PAYLOAD_OFF)] = F("{\"equitherm\": {\"enable\" : false}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SWITCH), F("equitherm")).c_str(), doc);
  }

  bool publishNumberEquithermFactorN(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("equitherm_n"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("equitherm_n"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("Equitherm factor N");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-n-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.n_factor|float(0)|round(3) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"equitherm\": {\"n_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0.001f;
    doc[FPSTR(HA_MAX)] = 10;
    doc[FPSTR(HA_STEP)] = 0.001f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("equitherm_n_factor")).c_str(), doc);
  }

  bool publishNumberEquithermFactorK(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("equitherm_k"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("equitherm_k"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("Equitherm factor K");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-k-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.k_factor|float(0)|round(2) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"equitherm\": {\"k_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 10;
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("equitherm_k_factor")).c_str(), doc);
  }

  bool publishNumberEquithermFactorT(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.pid.enable, 'offline', 'online') }}");
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("equitherm_t"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("equitherm_t"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_NAME)] = F("Equitherm factor T");
    doc[FPSTR(HA_ICON)] = F("mdi:alpha-t-circle-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.equitherm.t_factor|float(0)|round(2) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"equitherm\": {\"t_factor\" : {{ value }}}}");
    doc[FPSTR(HA_MIN)] = 0;
    doc[FPSTR(HA_MAX)] = 10;
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("equitherm_t_factor")).c_str(), doc);
  }


  bool publishBinSensorStatus(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("status"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("status"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Status");
    doc[FPSTR(HA_ICON)] = F("mdi:list-status");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value == 'online', 'OFF', 'ON') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 60;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("status")).c_str(), doc);
  }

  bool publishBinSensorOtStatus(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("ot_status"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("ot_status"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Opentherm status");
    doc[FPSTR(HA_ICON)] = F("mdi:list-status");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'OFF', 'ON') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("ot_status")).c_str(), doc);
  }

  bool publishBinSensorHeating(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("running");
    doc[FPSTR(HA_NAME)] = F("Heating");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.heating, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("heating")).c_str(), doc);
  }

  bool publishBinSensorDhw(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("running");
    doc[FPSTR(HA_NAME)] = F("DHW");
    doc[FPSTR(HA_ICON)] = F("mdi:water-pump");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.dhw, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("dhw")).c_str(), doc);
  }

  bool publishBinSensorFlame(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("flame"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("flame"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("running");
    doc[FPSTR(HA_NAME)] = F("Flame");
    doc[FPSTR(HA_ICON)] = F("mdi:fire");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.flame, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("flame")).c_str(), doc);
  }

  bool publishBinSensorFault(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("fault"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("fault"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Fault");
    doc[FPSTR(HA_ICON)] = F("mdi:water-boiler-alert");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.fault, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("fault")).c_str(), doc);
  }

  bool publishBinSensorDiagnostic(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("diagnostic"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("diagnostic"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("problem");
    doc[FPSTR(HA_NAME)] = F("Diagnostic");
    doc[FPSTR(HA_ICON)] = F("mdi:account-wrench");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.diagnostic, 'ON', 'OFF') }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("diagnostic")).c_str(), doc);
  }

  bool publishSensorPower(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("power"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("power"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("power");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("kW");
    doc[FPSTR(HA_NAME)] = F("Current power");
    doc[FPSTR(HA_ICON)] = F("mdi:chart-bar");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.sensors.power|float(0)|round(2) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("power")).c_str(), doc);
  }

  bool publishSensorFaultCode(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus and value_json.states.fault, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("fault_code"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("fault_code"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_NAME)] = F("Fault code");
    doc[FPSTR(HA_ICON)] = F("mdi:chat-alert-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ \"%02d (0x%02X)\"|format(value_json.sensors.faultCode, value_json.sensors.faultCode) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("fault_code")).c_str(), doc);
  }

  bool publishSensorDiagnosticCode(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus and value_json.states.fault or value_json.states.diagnostic, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("diagnostic_code"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("diagnostic_code"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_NAME)] = F("Diagnostic code");
    doc[FPSTR(HA_ICON)] = F("mdi:chat-alert-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ \"%02d (0x%02X)\"|format(value_json.sensors.diagnosticCode, value_json.sensors.diagnosticCode) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("diagnostic_code")).c_str(), doc);
  }

  bool publishSensorRssi(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("rssi"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("rssi"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("signal_strength");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("dBm");
    doc[FPSTR(HA_NAME)] = F("RSSI");
    doc[FPSTR(HA_ICON)] = F("mdi:signal");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.sensors.rssi|float(0)|round(1) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("rssi")).c_str(), doc);
  }

  bool publishSensorUptime(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("uptime"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("uptime"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("duration");
    doc[FPSTR(HA_STATE_CLASS)] = F("total_increasing");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("s");
    doc[FPSTR(HA_NAME)] = F("Uptime");
    doc[FPSTR(HA_ICON)] = F("mdi:clock-start");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.sensors.uptime|int(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("uptime")).c_str(), doc);
  }


  bool publishSensorModulation(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("modulation_level"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("modulation_level"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("power_factor");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("%");
    doc[FPSTR(HA_NAME)] = F("Modulation level");
    doc[FPSTR(HA_ICON)] = F("mdi:fire-circle");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.sensors.modulation|float(0)|round(0) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("modulation")).c_str(), doc);
  }

  bool publishSensorPressure(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("pressure"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("pressure"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("pressure");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("bar");

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("psi");
    }

    doc[FPSTR(HA_NAME)] = F("Pressure");
    doc[FPSTR(HA_ICON)] = F("mdi:gauge");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.sensors.pressure|float(0)|round(2) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("pressure")).c_str(), doc);
  }

  bool publishSensorDhwFlowRate(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw_flow_rate"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw_flow_rate"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("volume_flow_rate");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("L/min");

    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("gal/min");
    }

    doc[FPSTR(HA_NAME)] = F("DHW flow rate");
    doc[FPSTR(HA_ICON)] = F("mdi:water-pump");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.sensors.dhwFlowRate|float(0)|round(2) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("dhw_flow_rate")).c_str(), doc);
  }


  bool publishNumberIndoorTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("indoor_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("indoor_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = -99;
      doc[FPSTR(HA_MAX)] = 99;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = -147;
      doc[FPSTR(HA_MAX)] = 211;
    }

    doc[FPSTR(HA_NAME)] = F("Indoor temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:home-thermometer");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.indoor|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("state/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"temperatures\": {\"indoor\":{{ value }}}}");
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("indoor_temp")).c_str(), doc);
  }

  bool publishSensorIndoorTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("indoor_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("indoor_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Indoor temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:home-thermometer");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.indoor|float(0)|round(1) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("indoor_temp")).c_str(), doc);
  }

  bool publishNumberOutdoorTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("outdoor_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("outdoor_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      doc[FPSTR(HA_MIN)] = -99;
      doc[FPSTR(HA_MAX)] = 99;
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
      doc[FPSTR(HA_MIN)] = -147;
      doc[FPSTR(HA_MAX)] = 211;
    }

    doc[FPSTR(HA_NAME)] = F("Outdoor temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:home-thermometer-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.outdoor|float(0)|round(1) }}");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("state/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"temperatures\": {\"outdoor\":{{ value }}}}");
    doc[FPSTR(HA_STEP)] = 0.01f;
    doc[FPSTR(HA_MODE)] = "box";
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("outdoor_temp")).c_str(), doc);
  }

  bool publishSensorOutdoorTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("outdoor_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("outdoor_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Outdoor temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:home-thermometer-outline");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.outdoor|float(0)|round(1) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("outdoor_temp")).c_str(), doc);
  }

  bool publishSensorHeatingTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Heating temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.heating|float(0)|round(2) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("heating_temp")).c_str(), doc);
  }

  bool publishSensorHeatingReturnTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating_return_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating_return_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Heating return temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.heatingReturn|float(0)|round(2) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("heating_return_temp")).c_str(), doc);
  }

  bool publishSensorDhwTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("DHW temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:water-pump");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.dhw|float(0)|round(2) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("dhw_temp")).c_str(), doc);
  }

  bool publishSensorExhaustTemp(UnitSystem unit = UnitSystem::METRIC, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][0][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][1][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[FPSTR(HA_AVAILABILITY_MODE)] = F("all");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("exhaust_temp"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("exhaust_temp"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("diagnostic");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_C);
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = FPSTR(HA_UNIT_OF_MEASUREMENT_F);
    }

    doc[FPSTR(HA_NAME)] = F("Exhaust temperature");
    doc[FPSTR(HA_ICON)] = F("mdi:smoke");
    doc[FPSTR(HA_STATE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_VALUE_TEMPLATE)] = F("{{ value_json.temperatures.exhaust|float(0)|round(2) }}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("exhaust_temp")).c_str(), doc);
  }


  bool publishClimateHeating(UnitSystem unit = UnitSystem::METRIC, byte minTemp = 20, byte maxTemp = 90, byte currentTempSource = HaHelper::TEMP_SOURCE_HEATING, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("heating"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("heating"));
    doc[FPSTR(HA_NAME)] = F("Heating");
    doc[FPSTR(HA_ICON)] = F("mdi:radiator");

    if (currentTempSource == HaHelper::TEMP_SOURCE_HEATING || currentTempSource == HaHelper::TEMP_SOURCE_INDOOR) {
      doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)] = this->getDeviceTopic(F("state"));
    }

    if (currentTempSource == HaHelper::TEMP_SOURCE_HEATING) {
      doc[FPSTR(HA_CURRENT_TEMPERATURE_TEMPLATE)] = F("{{ value_json.temperatures.heating|float(0)|round(2) }}");

    } else if (currentTempSource == HaHelper::TEMP_SOURCE_INDOOR) {
      doc[FPSTR(HA_CURRENT_TEMPERATURE_TEMPLATE)] = F("{{ value_json.temperatures.indoor|float(0)|round(2) }}");
    }

    doc[FPSTR(HA_TEMPERATURE_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_TEMPERATURE_COMMAND_TEMPLATE)] = F("{\"heating\": {\"target\" : {{ value }}}}");

    doc[FPSTR(HA_TEMPERATURE_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_TEMPERATURE_STATE_TEMPLATE)] = F("{{ value_json.heating.target|float(0)|round(1) }}");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "C";
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "F";
    }

    doc[FPSTR(HA_MODE_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_MODE_COMMAND_TEMPLATE)] = F("{% if value == 'heat' %}{\"heating\": {\"enable\" : true}}"
      "{% elif value == 'off' %}{\"heating\": {\"enable\" : false}}{% endif %}");
    doc[FPSTR(HA_MODE_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_MODE_STATE_TEMPLATE)] = F("{{ iif(value_json.heating.enable, 'heat', 'off') }}");
    doc[FPSTR(HA_MODES)][0] = F("off");
    doc[FPSTR(HA_MODES)][1] = F("heat");

    doc[FPSTR(HA_ACTION_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_ACTION_TEMPLATE)] = F("{{ iif(value_json.states.heating, 'heating', 'idle') }}");

    doc[FPSTR(HA_PRESET_MODE_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_PRESET_MODE_COMMAND_TEMPLATE)] = F("{% if value == 'boost' %}{\"heating\": {\"turbo\" : true}}"
      "{% elif value == 'none' %}{\"heating\": {\"turbo\" : false}}{% endif %}");
    doc[FPSTR(HA_PRESET_MODE_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_PRESET_MODE_VALUE_TEMPLATE)] = F("{{ iif(value_json.heating.turbo, 'boost', 'none') }}");
    doc[FPSTR(HA_PRESET_MODES)][0] = F("boost");

    doc[FPSTR(HA_MIN_TEMP)] = minTemp;
    doc[FPSTR(HA_MAX_TEMP)] = maxTemp;
    doc[FPSTR(HA_TEMP_STEP)] = 0.5f;
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_CLIMATE), F("heating"), '_').c_str(), doc);
  }

  bool publishClimateDhw(UnitSystem unit = UnitSystem::METRIC, byte minTemp = 40, byte maxTemp = 60, bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("status"));
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("dhw"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("dhw"));
    doc[FPSTR(HA_NAME)] = F("DHW");
    doc[FPSTR(HA_ICON)] = F("mdi:water-pump");

    doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_CURRENT_TEMPERATURE_TEMPLATE)] = F("{{ value_json.temperatures.dhw|float(0)|round(1) }}");

    doc[FPSTR(HA_TEMPERATURE_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_TEMPERATURE_COMMAND_TEMPLATE)] = F("{\"dhw\": {\"target\" : {{ value|int(0) }}}}");

    doc[FPSTR(HA_TEMPERATURE_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_TEMPERATURE_STATE_TEMPLATE)] = F("{{ value_json.dhw.target|float(0)|round(1) }}");

    if (unit == UnitSystem::METRIC) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "C";
      
    } else if (unit == UnitSystem::IMPERIAL) {
      doc[FPSTR(HA_TEMPERATURE_UNIT)] = "F";
    }

    doc[FPSTR(HA_MODE_COMMAND_TOPIC)] = this->getDeviceTopic(F("settings/set"));
    doc[FPSTR(HA_MODE_COMMAND_TEMPLATE)] = F("{% if value == 'heat' %}{\"dhw\": {\"enable\" : true}}"
      "{% elif value == 'off' %}{\"dhw\": {\"enable\" : false}}{% endif %}");
    doc[FPSTR(HA_MODE_STATE_TOPIC)] = this->getDeviceTopic(F("settings"));
    doc[FPSTR(HA_MODE_STATE_TEMPLATE)] = F("{{ iif(value_json.dhw.enable, 'heat', 'off') }}");
    doc[FPSTR(HA_MODES)][0] = F("off");
    doc[FPSTR(HA_MODES)][1] = F("heat");

    doc[FPSTR(HA_ACTION_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_ACTION_TEMPLATE)] = F("{{ iif(value_json.states.dhw, 'heating', 'idle') }}");

    doc[FPSTR(HA_MIN_TEMP)] = minTemp;
    doc[FPSTR(HA_MAX_TEMP)] = maxTemp;
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_CLIMATE), F("dhw"), '_').c_str(), doc);
  }


  bool publishButtonRestart(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("restart"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("restart"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("restart");
    doc[FPSTR(HA_NAME)] = F("Restart");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("state/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"actions\": {\"restart\": true}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BUTTON), F("restart")).c_str(), doc);
  }

  bool publishButtonResetFault(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.fault, 'online', 'offline') }}");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("reset_fault"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("reset_fault"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("restart");
    doc[FPSTR(HA_NAME)] = F("Reset fault");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("state/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"actions\": {\"resetFault\": true}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BUTTON), F("reset_fault")).c_str(), doc);
  }

  bool publishButtonResetDiagnostic(bool enabledByDefault = true) {
    JsonDocument doc;
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_TOPIC)] = this->getDeviceTopic(F("state"));
    doc[FPSTR(HA_AVAILABILITY)][FPSTR(HA_VALUE_TEMPLATE)] = F("{{ iif(value_json.states.diagnostic, 'online', 'offline') }}");
    doc[FPSTR(HA_ENABLED_BY_DEFAULT)] = enabledByDefault;
    doc[FPSTR(HA_UNIQUE_ID)] = this->getObjectId(F("reset_diagnostic"));
    doc[FPSTR(HA_OBJECT_ID)] = this->getObjectId(F("reset_diagnostic"));
    doc[FPSTR(HA_ENTITY_CATEGORY)] = F("config");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("restart");
    doc[FPSTR(HA_NAME)] = F("Reset diagnostic");
    doc[FPSTR(HA_COMMAND_TOPIC)] = this->getDeviceTopic(F("state/set"));
    doc[FPSTR(HA_COMMAND_TEMPLATE)] = F("{\"actions\": {\"resetDiagnostic\": true}}");
    doc[FPSTR(HA_EXPIRE_AFTER)] = 120;
    doc.shrinkToFit();

    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BUTTON), F("reset_diagnostic")).c_str(), doc);
  }


  bool deleteNumberOutdoorTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("outdoor_temp")).c_str());
  }

  bool deleteSensorOutdoorTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("outdoor_temp")).c_str());
  }

  bool deleteNumberIndoorTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("indoor_temp")).c_str());
  }

  bool deleteSensorIndoorTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("indoor_temp")).c_str());
  }

  bool deleteSwitchDhw() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SWITCH), F("dhw")).c_str());
  }

  bool deleteSensorBoilerDhwMinTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("boiler_dhw_min_temp")).c_str());
  }

  bool deleteSensorBoilerDhwMaxTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("boiler_dhw_max_temp")).c_str());
  }

  bool deleteNumberDhwMinTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_min_temp")).c_str());
  }

  bool deleteNumberDhwMaxTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_max_temp")).c_str());
  }

  bool deleteBinSensorDhw() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_BINARY_SENSOR), F("dhw")).c_str());
  }

  bool deleteSensorDhwTemp() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("dhw_temp")).c_str());
  }

  bool deleteNumberDhwTarget() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_NUMBER), F("dhw_target")).c_str());
  }

  bool deleteSensorDhwFlowRate() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_SENSOR), F("dhw_flow_rate")).c_str());
  }

  bool deleteClimateDhw() {
    return this->publish(this->getTopic(FPSTR(HA_ENTITY_CLIMATE), F("dhw"), '_').c_str());
  }
};
