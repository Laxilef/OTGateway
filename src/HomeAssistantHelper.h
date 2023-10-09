#pragma once
extern PubSubClient client;

class HomeAssistantHelper {
public:
  void setPrefix(String value) {
    _prefix = value;
  }

  void setDeviceVersion(String value) {
    _deviceVersion = value;
  }

  void setDeviceManufacturer(String value) {
    _deviceManufacturer = value;
  }

  void setDeviceModel(String value) {
    _deviceModel = value;
  }

  void setDeviceName(String value) {
    _deviceName = value;
  }

  void setDeviceConfigUrl(String value) {
    _deviceConfigUrl = value;
  }

  bool publishSelectOutdoorSensorType(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"sensors\": {\"outdoor\": {\"type\": {% if value == 'Boiler' %}0{% elif value == 'Manual' %}1{% elif value == 'External' %}2{% endif %}}}}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_outdoor_sensor_type");
    doc[F("object_id")] = _prefix + F("_outdoor_sensor_type");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Outdoor temperature source");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{% if value_json.sensors.outdoor.type == 0 %}Boiler{% elif value_json.sensors.outdoor.type == 1 %}Manual{% elif value_json.sensors.outdoor.type == 2 %}External{% endif %}");
    doc[F("options")][0] = F("Boiler");
    doc[F("options")][1] = F("Manual");
    doc[F("options")][2] = F("External");

    client.beginPublish((F("homeassistant/select/") + _prefix + F("/outdoor_sensor_type/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSelectIndoorSensorType(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"sensors\": {\"indoor\": {\"type\": {% if value == 'Manual' %}1{% elif value == 'External' %}2{% endif %}}}}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_indoor_sensor_type");
    doc[F("object_id")] = _prefix + F("_indoor_sensor_type");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Indoor temperature source");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{% if value_json.sensors.indoor.type == 1 %}Manual{% elif value_json.sensors.indoor.type == 2 %}External{% endif %}");
    doc[F("options")][0] = F("Manual");
    doc[F("options")][1] = F("External");

    client.beginPublish((F("homeassistant/select/") + _prefix + F("/indoor_sensor_type/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberOutdoorSensorOffset(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/settings");
    doc[F("availability")][F("value_template")] = F("{{ iif(value_json.sensors.outdoor.type != 1, 'online', 'offline') }}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_outdoor_sensor_offset");
    doc[F("object_id")] = _prefix + F("_outdoor_sensor_offset");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Outdoor sensor offset");
    doc[F("icon")] = F("mdi:altimeter");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.sensors.outdoor.offset|float(0)|round(2) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"sensors\": {\"outdoor\" : {\"offset\" : {{ value }}}}}");
    doc[F("min")] = -10;
    doc[F("max")] = 10;
    doc[F("step")] = 0.1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/outdoor_sensor_offset/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberIndoorSensorOffset(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/settings");
    doc[F("availability")][F("value_template")] = F("{{ iif(value_json.sensors.indoor.type != 1, 'online', 'offline') }}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_indoor_sensor_offset");
    doc[F("object_id")] = _prefix + F("_indoor_sensor_offset");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Indoor sensor offset");
    doc[F("icon")] = F("mdi:altimeter");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.sensors.indoor.offset|float(0)|round(2) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"sensors\": {\"indoor\" : {\"offset\" : {{ value }}}}}");
    doc[F("min")] = -10;
    doc[F("max")] = 10;
    doc[F("step")] = 0.1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/indoor_sensor_offset/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchDebug(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_debug");
    doc[F("object_id")] = _prefix + F("_debug");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Debug");
    doc[F("icon")] = F("mdi:code-braces");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.debug }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"debug\": true}");
    doc[F("payload_off")] = F("{\"debug\": false}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/debug/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchEmergency(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_emergency");
    doc[F("object_id")] = _prefix + F("_emergency");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Use emergency");
    doc[F("icon")] = F("mdi:sun-snowflake-variant");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.emergency.enable }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"emergency\": {\"enable\" : true}}");
    doc[F("payload_off")] = F("{\"emergency\": {\"enable\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/emergency/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEmergencyTarget(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_emergency_target");
    doc[F("object_id")] = _prefix + F("_emergency_target");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Emergency target temp");
    doc[F("icon")] = F("mdi:thermometer-alert");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.emergency.target|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"emergency\": {\"target\" : {{ value }}}}");
    doc[F("min")] = 5;
    doc[F("max")] = 50;
    doc[F("step")] = 0.5;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/emergency_target/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSwitchEmergencyUseEquitherm(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/settings");
    doc[F("availability")][F("value_template")] = F("{{ iif(value_json.sensors.outdoor.type != 1, 'online', 'offline') }}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_emergency_use_equitherm");
    doc[F("object_id")] = _prefix + F("_emergency_use_equitherm");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Use equitherm in emergency");
    doc[F("icon")] = F("mdi:snowflake-alert");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.emergency.useEquitherm }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"emergency\": {\"useEquitherm\" : true}}");
    doc[F("payload_off")] = F("{\"emergency\": {\"useEquitherm\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/emergency_use_equitherm/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchHeating(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating");
    doc[F("object_id")] = _prefix + F("_heating");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Heating");
    doc[F("icon")] = F("mdi:radiator");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.heating.enable }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"heating\": {\"enable\" : true}}");
    doc[F("payload_off")] = F("{\"heating\": {\"enable\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/heating/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSwitchHeatingTurbo(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating_turbo");
    doc[F("object_id")] = _prefix + F("_heating_turbo");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Turbo heating");
    doc[F("icon")] = F("mdi:rocket-launch-outline");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.heating.turbo }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"heating\": {\"turbo\" : true}}");
    doc[F("payload_off")] = F("{\"heating\": {\"turbo\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/heating_turbo/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberHeatingTarget(byte minTemp = 20, byte maxTemp = 90, bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating_target");
    doc[F("object_id")] = _prefix + F("_heating_target");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Heating target");
    doc[F("icon")] = F("mdi:radiator");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.heating.target|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"heating\": {\"target\" : {{ value }}}}");
    doc[F("min")] = minTemp;
    doc[F("max")] = maxTemp <= minTemp ? maxTemp : maxTemp;
    doc[F("step")] = 0.5;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/heating_target/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberHeatingHysteresis(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating_hysteresis");
    doc[F("object_id")] = _prefix + F("_heating_hysteresis");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Heating hysteresis");
    doc[F("icon")] = F("mdi:altimeter");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.heating.hysteresis|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"heating\": {\"hysteresis\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 5;
    doc[F("step")] = 0.1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/heating_hysteresis/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorHeatingSetpoint(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating_setpoint");
    doc[F("object_id")] = _prefix + F("_heating_setpoint");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Heating setpoint");
    doc[F("icon")] = F("mdi:coolant-temperature");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.parameters.heatingSetpoint|int(0) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/heating_setpoint/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorCurrentHeatingMinTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_current_heating_min_temp");
    doc[F("object_id")] = _prefix + F("_current_heating_min_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Current heating min temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-down");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.parameters.heatingMinTemp|int(0) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/current_heating_min_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorCurrentHeatingMaxTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_current_heating_max_temp");
    doc[F("object_id")] = _prefix + F("_current_heating_max_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Current heating max temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-up");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.parameters.heatingMaxTemp|int(0) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/current_heating_max_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberHeatingMinTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating_min_temp");
    doc[F("object_id")] = _prefix + F("_heating_min_temp");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Heating min temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-down");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.heating.minTemp|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"heating\": {\"minTemp\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 99;
    doc[F("step")] = 1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/heating_min_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }
  
  bool publishNumberHeatingMaxTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating_max_temp");
    doc[F("object_id")] = _prefix + F("_heating_max_temp");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Heating max temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-up");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.heating.maxTemp|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"heating\": {\"maxTemp\" : {{ value }}}}");
    doc[F("min")] = 1;
    doc[F("max")] = 100;
    doc[F("step")] = 1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/heating_max_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchDHW(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_dhw");
    doc[F("object_id")] = _prefix + F("_dhw");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("DHW");
    doc[F("icon")] = F("mdi:water-pump");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.dhw.enable }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"dhw\": {\"enable\" : true}}");
    doc[F("payload_off")] = F("{\"dhw\": {\"enable\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/dhw/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberDHWTarget(byte minTemp = 40, byte maxTemp = 60, bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_dhw_target");
    doc[F("object_id")] = _prefix + F("_dhw_target");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("DHW target");
    doc[F("icon")] = F("mdi:water-pump");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.dhw.target|int(0) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"dhw\": {\"target\" : {{ value|int(0) }}}}");
    doc[F("min")] = minTemp;
    doc[F("max")] = maxTemp <= minTemp ? maxTemp : maxTemp;
    doc[F("step")] = 1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/dhw_target/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorCurrentDHWMinTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_current_dhw_min_temp");
    doc[F("object_id")] = _prefix + F("_current_dhw_min_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Current DHW min temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-down");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.parameters.dhwMinTemp|int(0) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/current_dhw_min_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorCurrentDHWMaxTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_current_dhw_max_temp");
    doc[F("object_id")] = _prefix + F("_current_dhw_max_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Current DHW max temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-up");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.parameters.dhwMaxTemp|int(0) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/current_dhw_max_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberDHWMinTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_dhw_min_temp");
    doc[F("object_id")] = _prefix + F("_dhw_min_temp");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("DHW min temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-down");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.dhw.minTemp|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"dhw\": {\"minTemp\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 99;
    doc[F("step")] = 1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/dhw_min_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }
  
  bool publishNumberDHWMaxTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_dhw_max_temp");
    doc[F("object_id")] = _prefix + F("_dhw_max_temp");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("DHW max temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-up");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.dhw.maxTemp|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"dhw\": {\"maxTemp\" : {{ value }}}}");
    doc[F("min")] = 1;
    doc[F("max")] = 100;
    doc[F("step")] = 1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/dhw_max_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchPID(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_pid");
    doc[F("object_id")] = _prefix + F("_pid");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("PID");
    doc[F("icon")] = F("mdi:chart-bar-stacked");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.pid.enable }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"pid\": {\"enable\" : true}}");
    doc[F("payload_off")] = F("{\"pid\": {\"enable\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/pid/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberPIDFactorP(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("unique_id")] = _prefix + F("_pid_p");
    doc[F("object_id")] = _prefix + F("_pid_p");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("PID factor P");
    doc[F("icon")] = F("mdi:alpha-p-circle-outline");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.pid.p_factor|float(0)|round(3) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"pid\": {\"p_factor\" : {{ value }}}}");
    doc[F("min")] = 0.001;
    doc[F("max")] = 10;
    doc[F("step")] = 0.001;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/pid_p_factor/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberPIDFactorI(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("unique_id")] = _prefix + F("_pid_i");
    doc[F("object_id")] = _prefix + F("_pid_i");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("PID factor I");
    doc[F("icon")] = F("mdi:alpha-i-circle-outline");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.pid.i_factor|float(0)|round(3) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"pid\": {\"i_factor\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 10;
    doc[F("step")] = 0.001;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/pid_i_factor/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberPIDFactorD(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("unique_id")] = _prefix + F("_pid_d");
    doc[F("object_id")] = _prefix + F("_pid_d");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("PID factor D");
    doc[F("icon")] = F("mdi:alpha-d-circle-outline");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.pid.d_factor|float(0)|round(3) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"pid\": {\"d_factor\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 10;
    doc[F("step")] = 0.001;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/pid_d_factor/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberPIDMinTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_pid_min_temp");
    doc[F("object_id")] = _prefix + F("_pid_min_temp");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("PID min temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-down");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.pid.minTemp|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"pid\": {\"minTemp\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 99;
    doc[F("step")] = 1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/pid_min_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }
  
  bool publishNumberPIDMaxTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_pid_max_temp");
    doc[F("object_id")] = _prefix + F("_pid_max_temp");
    doc[F("entity_category")] = F("config");
    doc[F("device_class")] = F("temperature");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("PID max temp");
    doc[F("icon")] = F("mdi:thermometer-chevron-up");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.pid.maxTemp|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"pid\": {\"maxTemp\" : {{ value }}}}");
    doc[F("min")] = 1;
    doc[F("max")] = 100;
    doc[F("step")] = 1;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/pid_max_temp/config")).c_str(), measureJson(doc), true);
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchEquitherm(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_equitherm");
    doc[F("object_id")] = _prefix + F("_equitherm");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Equitherm");
    doc[F("icon")] = F("mdi:sun-snowflake-variant");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.equitherm.enable }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("payload_on")] = F("{\"equitherm\": {\"enable\" : true}}");
    doc[F("payload_off")] = F("{\"equitherm\": {\"enable\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/equitherm/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEquithermFactorN(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("unique_id")] = _prefix + F("_equitherm_n");
    doc[F("object_id")] = _prefix + F("_equitherm_n");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Equitherm factor N");
    doc[F("icon")] = F("mdi:alpha-n-circle-outline");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.equitherm.n_factor|float(0)|round(3) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"equitherm\": {\"n_factor\" : {{ value }}}}");
    doc[F("min")] = 0.001;
    doc[F("max")] = 5;
    doc[F("step")] = 0.001;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/equitherm_n_factor/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEquithermFactorK(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("unique_id")] = _prefix + F("_equitherm_k");
    doc[F("object_id")] = _prefix + F("_equitherm_k");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Equitherm factor K");
    doc[F("icon")] = F("mdi:alpha-k-circle-outline");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.equitherm.k_factor|float(0)|round(2) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"equitherm\": {\"k_factor\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 10;
    doc[F("step")] = 0.01;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/equitherm_k_factor/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEquithermFactorT(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/settings");
    doc[F("availability")][F("value_template")] = F("{{ iif(value_json.pid.enable, 'offline', 'online') }}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("unique_id")] = _prefix + F("_equitherm_t");
    doc[F("object_id")] = _prefix + F("_equitherm_t");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Equitherm factor T");
    doc[F("icon")] = F("mdi:alpha-t-circle-outline");
    doc[F("state_topic")] = _prefix + F("/settings");
    doc[F("value_template")] = F("{{ value_json.equitherm.t_factor|float(0)|round(2) }}");
    doc[F("command_topic")] = _prefix + F("/settings/set");
    doc[F("command_template")] = F("{\"equitherm\": {\"t_factor\" : {{ value }}}}");
    doc[F("min")] = 0;
    doc[F("max")] = 10;
    doc[F("step")] = 0.01;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/equitherm_t_factor/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchTuning(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_tuning");
    doc[F("object_id")] = _prefix + F("_tuning");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Tuning");
    doc[F("icon")] = F("mdi:tune-vertical");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("state_on")] = true;
    doc[F("state_off")] = false;
    doc[F("value_template")] = F("{{ value_json.tuning.enable }}");
    doc[F("command_topic")] = _prefix + F("/state/set");
    doc[F("payload_on")] = F("{\"tuning\": {\"enable\" : true}}");
    doc[F("payload_off")] = F("{\"tuning\": {\"enable\" : false}}");

    client.beginPublish((F("homeassistant/switch/") + _prefix + F("/tuning/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSelectTuningRegulator(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("availability_mode")] = F("all");
    doc[F("command_topic")] = _prefix + F("/state/set");
    doc[F("command_template")] = F("{\"tuning\": {\"regulator\": {% if value == 'Equitherm' %}0{% elif value == 'PID' %}1{% endif %}}}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_tuning_regulator");
    doc[F("object_id")] = _prefix + F("_tuning_regulator");
    doc[F("entity_category")] = F("config");
    doc[F("name")] = F("Tuning regulator");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{% if value_json.tuning.regulator == 0 %}Equitherm{% elif value_json.tuning.regulator == 1 %}PID{% endif %}");
    doc[F("options")][0] = F("Equitherm");
    doc[F("options")][1] = F("PID");

    client.beginPublish((F("homeassistant/select/") + _prefix + F("/tuning_regulator/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishBinSensorStatus(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_status");
    doc[F("object_id")] = _prefix + F("_status");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("problem");
    doc[F("name")] = F("Status");
    doc[F("icon")] = F("mdi:list-status");
    doc[F("state_topic")] = _prefix + F("/status");
    doc[F("value_template")] = F("{{ iif(value == 'online', 'OFF', 'ON') }}");
    doc[F("expire_after")] = 60;

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + F("/status/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorOtStatus(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_ot_status");
    doc[F("object_id")] = _prefix + F("_ot_status");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("problem");
    doc[F("name")] = F("Opentherm status");
    doc[F("icon")] = F("mdi:list-status");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ iif(value_json.states.otStatus, 'OFF', 'ON') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + F("/ot_status/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorHeating(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating");
    doc[F("object_id")] = _prefix + F("_heating");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("running");
    doc[F("name")] = F("Heating");
    doc[F("icon")] = F("mdi:radiator");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ iif(value_json.states.heating, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + F("/heating/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorDHW(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_dhw");
    doc[F("object_id")] = _prefix + F("_dhw");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("running");
    doc[F("name")] = F("DHW");
    doc[F("icon")] = F("mdi:water-pump");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ iif(value_json.states.dhw, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + F("/dhw/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorFlame(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_flame");
    doc[F("object_id")] = _prefix + F("_flame");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("running");
    doc[F("name")] = F("Flame");
    doc[F("icon")] = F("mdi:fire");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ iif(value_json.states.flame, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + F("/flame/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorFault(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/state");
    doc[F("availability")][F("value_template")] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_fault");
    doc[F("object_id")] = _prefix + F("_fault");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("problem");
    doc[F("name")] = F("Fault");
    doc[F("icon")] = F("mdi:water-boiler-alert");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ iif(value_json.states.fault, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + F("/fault/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorDiagnostic(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_diagnostic");
    doc[F("object_id")] = _prefix + F("_diagnostic");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("problem");
    doc[F("name")] = F("Diagnostic");
    doc[F("icon")] = F("mdi:account-wrench");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ iif(value_json.states.diagnostic, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + F("/diagnostic/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorFaultCode(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/state");
    doc[F("availability")][F("value_template")] = F("{{ iif(value_json.states.fault, 'online', 'offline') }}");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_fault_code");
    doc[F("object_id")] = _prefix + F("_fault_code");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("name")] = F("Fault code");
    doc[F("icon")] = F("mdi:chat-alert-outline");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ \"E%02d\"|format(value_json.states.faultCode) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/fault_code/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorRssi(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }

    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_rssi";
    doc["object_id"] = _prefix + "_rssi";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "signal_strength";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "dBm";
    doc["name"] = "RSSI";
    doc["icon"] = "mdi:signal";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.states.rssi|float(0)|round(1) }}";

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/rssi/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSensorModulation(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_modulation_level");
    doc[F("object_id")] = _prefix + F("_modulation_level");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("power_factor");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("%");
    doc[F("name")] = F("Modulation level");
    doc[F("icon")] = F("mdi:fire-circle");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.sensors.modulation|float(0)|round(0) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/modulation/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorPressure(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_pressure");
    doc[F("object_id")] = _prefix + F("_pressure");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("pressure");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("bar");
    doc[F("name")] = F("Pressure");
    doc[F("icon")] = F("mdi:gauge");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.sensors.pressure|float(0)|round(2) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/pressure/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishNumberIndoorTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_indoor_temp");
    doc[F("object_id")] = _prefix + F("_indoor_temp");
    doc[F("entity_category")] = F("config");
    //doc[F("entity_registry_visible_default")] = false;
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Indoor temperature");
    doc[F("icon")] = F("mdi:home-thermometer");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.temperatures.indoor|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/state/set");
    doc[F("command_template")] = F("{\"temperatures\": {\"indoor\":{{ value }}}}");
    doc[F("min")] = -99;
    doc[F("max")] = 99;
    doc[F("step")] = 0.01;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/indoor_temp/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorIndoorTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][0][F("topic")] = _prefix + F("/status");
    doc[F("availability_mode")] = F("any");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_indoor_temp");
    doc[F("object_id")] = _prefix + F("_indoor_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Indoor temperature");
    doc[F("icon")] = F("mdi:home-thermometer");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.temperatures.indoor|float(0)|round(1) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/indoor_temp/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberOutdoorTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_outdoor_temp");
    doc[F("object_id")] = _prefix + F("_outdoor_temp");
    doc[F("entity_category")] = F("config");
    //doc[F("entity_registry_visible_default")] = false;
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Outdoor temperature");
    doc[F("icon")] = F("mdi:home-thermometer-outline");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.temperatures.outdoor|float(0)|round(1) }}");
    doc[F("command_topic")] = _prefix + F("/state/set");
    doc[F("command_template")] = F("{\"temperatures\": {\"outdoor\":{{ value }}}}");
    doc[F("min")] = -99;
    doc[F("max")] = 99;
    doc[F("step")] = 0.01;
    doc[F("mode")] = "box";

    client.beginPublish((F("homeassistant/number/") + _prefix + F("/outdoor_temp/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorOutdoorTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][0][F("topic")] = _prefix + F("/status");
    doc[F("availability_mode")] = F("any");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_outdoor_temp");
    doc[F("object_id")] = _prefix + F("_outdoor_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Outdoor temperature");
    doc[F("icon")] = F("mdi:home-thermometer-outline");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.temperatures.outdoor|float(0)|round(1) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/outdoor_temp/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorHeatingTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating_temp");
    doc[F("object_id")] = _prefix + F("_heating_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("Heating temperature");
    doc[F("icon")] = F("mdi:radiator");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.temperatures.heating|float(0)|round(2) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/heating_temp/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorDHWTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_dhw_temp");
    doc[F("object_id")] = _prefix + F("_dhw_temp");
    doc[F("entity_category")] = F("diagnostic");
    doc[F("device_class")] = F("temperature");
    doc[F("state_class")] = F("measurement");
    doc[F("unit_of_measurement")] = F("°C");
    doc[F("name")] = F("DHW temperature");
    doc[F("icon")] = F("mdi:water-pump");
    doc[F("state_topic")] = _prefix + F("/state");
    doc[F("value_template")] = F("{{ value_json.temperatures.dhw|float(0)|round(2) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + F("/dhw_temp/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishClimateHeating(byte minTemp = 20, byte maxTemp = 90, bool enabledByDefault = true) {
    StaticJsonDocument<2048> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }
    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_heating");
    doc[F("object_id")] = _prefix + F("_heating");
    doc[F("name")] = F("Heating");
    doc[F("icon")] = F("mdi:radiator");

    doc[F("current_temperature_topic")] = _prefix + F("/state");
    doc[F("current_temperature_template")] = F("{% if value_json.temperatures.indoor|float(0) != 0 %}{{ value_json.temperatures.indoor|float(0)|round(2) }}"
      "{% else %}{{ value_json.temperatures.heating|float(0)|round(2) }}{% endif %}");

    doc[F("temperature_command_topic")] = _prefix + F("/settings/set");
    doc[F("temperature_command_template")] = F("{\"heating\": {\"target\" : {{ value }}}}");

    doc[F("temperature_state_topic")] = _prefix + F("/settings");
    doc[F("temperature_state_template")] = F("{{ value_json.heating.target|float(0)|round(1) }}");

    doc[F("mode_command_topic")] = _prefix + F("/settings/set");
    doc[F("mode_command_template")] = F("{% if value == 'heat' %}{\"heating\": {\"enable\" : true}}"
      "{% elif value == 'off' %}{\"heating\": {\"enable\" : false}}{% endif %}");
    doc[F("mode_state_topic")] = _prefix + F("/settings");
    doc[F("mode_state_template")] = F("{{ iif(value_json.heating.enable, 'heat', 'off') }}");
    doc[F("modes")][0] = F("off");
    doc[F("modes")][1] = F("heat");

    doc[F("action_topic")] = _prefix + F("/state");
    doc[F("action_template")] = F("{{ iif(value_json.states.heating, 'heating', 'idle') }}");

    doc[F("preset_mode_command_topic")] = _prefix + F("/settings/set");
    doc[F("preset_mode_command_template")] = F("{% if value == 'boost' %}{\"heating\": {\"turbo\" : true}}"
      "{% elif value == 'none' %}{\"heating\": {\"turbo\" : false}}{% endif %}");
    doc[F("preset_mode_state_topic")] = _prefix + F("/settings");
    doc[F("preset_mode_value_template")] = F("{{ iif(value_json.heating.turbo, 'boost', 'none') }}");
    doc[F("preset_modes")][0] = F("boost");

    doc[F("min_temp")] = minTemp;
    doc[F("max_temp")] = maxTemp;
    doc[F("temp_step")] = 0.5;

    client.beginPublish((F("homeassistant/climate/") + _prefix + F("_heating/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishClimateDHW(byte minTemp = 40, byte maxTemp = 60, bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc[F("availability")][F("topic")] = _prefix + F("/status");
    doc[F("device")][F("identifiers")][0] = _prefix;
    doc[F("device")][F("sw_version")] = _deviceVersion;
    doc[F("device")][F("manufacturer")] = _deviceManufacturer;
    doc[F("device")][F("model")] = _deviceModel;
    doc[F("device")][F("name")] = _deviceName;
    if (_deviceConfigUrl) {
      doc[F("device")][F("configuration_url")] = _deviceConfigUrl;
    }

    doc[F("enabled_by_default")] = enabledByDefault;
    doc[F("unique_id")] = _prefix + F("_dhw");
    doc[F("object_id")] = _prefix + F("_dhw");
    doc[F("name")] = F("DHW");
    doc[F("icon")] = F("mdi:water-pump");

    doc[F("current_temperature_topic")] = _prefix + F("/state");
    doc[F("current_temperature_template")] = F("{{ value_json.temperatures.dhw|float(0)|round(1) }}");

    doc[F("temperature_command_topic")] = _prefix + F("/settings/set");
    doc[F("temperature_command_template")] = F("{\"dhw\": {\"target\" : {{ value|int(0) }}}}");

    doc[F("temperature_state_topic")] = _prefix + F("/settings");
    doc[F("temperature_state_template")] = F("{{ value_json.dhw.target|int(0) }}");

    doc[F("mode_command_topic")] = _prefix + F("/settings/set");
    doc[F("mode_command_template")] = F("{% if value == 'heat' %}{\"dhw\": {\"enable\" : true}}"
      "{% elif value == 'off' %}{\"dhw\": {\"enable\" : false}}{% endif %}");
    doc[F("mode_state_topic")] = _prefix + F("/settings");
    doc[F("mode_state_template")] = F("{{ iif(value_json.dhw.enable, 'heat', 'off') }}");
    doc[F("modes")][0] = F("off");
    doc[F("modes")][1] = F("heat");

    doc[F("action_topic")] = _prefix + F("/state");
    doc[F("action_template")] = F("{{ iif(value_json.states.dhw, 'heating', 'idle') }}");

    doc[F("min_temp")] = minTemp;
    doc[F("max_temp")] = maxTemp;

    client.beginPublish((F("homeassistant/climate/") + _prefix + F("_dhw/config")).c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool deleteNumberOutdoorTemp() {
    return client.publish((F("homeassistant/number/") + _prefix + F("/outdoor_temp/config")).c_str(), NULL, true);
  }

  bool deleteSensorOutdoorTemp() {
    return client.publish((F("homeassistant/sensor/") + _prefix + F("/outdoor_temp/config")).c_str(), NULL, true);
  }

  bool deleteNumberIndoorTemp() {
    return client.publish((F("homeassistant/number/") + _prefix + F("/indoor_temp/config")).c_str(), NULL, true);
  }

  bool deleteSensorIndoorTemp() {
    return client.publish((F("homeassistant/sensor/") + _prefix + F("/indoor_temp/config")).c_str(), NULL, true);
  }

  bool deleteSwitchDHW() {
    return client.publish((F("homeassistant/switch/") + _prefix + F("/dhw/config")).c_str(), NULL, true);
  }

  bool deleteSensorCurrentDHWMinTemp() {
    return client.publish((F("homeassistant/sensor/") + _prefix + F("/current_dhw_min_temp/config")).c_str(), NULL, true);
  }

  bool deleteSensorCurrentDHWMaxTemp() {
    return client.publish((F("homeassistant/sensor/") + _prefix + F("/current_dhw_max_temp/config")).c_str(), NULL, true);
  }

  bool deleteNumberDHWMinTemp() {
    return client.publish((F("homeassistant/number/") + _prefix + F("/dhw_min_temp/config")).c_str(), NULL, true);
  }

  bool deleteNumberDHWMaxTemp() {
    return client.publish((F("homeassistant/number/") + _prefix + F("/dhw_max_temp/config")).c_str(), NULL, true);
  }

  bool deleteBinSensorDHW() {
    return client.publish((F("homeassistant/binary_sensor/") + _prefix + F("/dhw/config")).c_str(), NULL, true);
  }

  bool deleteSensorDHWTemp() {
    return client.publish((F("homeassistant/sensor/") + _prefix + F("/dhw_temp/config")).c_str(), NULL, true);
  }

  bool deleteNumberDHWTarget() {
    return client.publish((F("homeassistant/number/") + _prefix + F("/dhw_target/config")).c_str(), NULL, true);
  }

  bool deleteClimateDHW() {
    return client.publish((F("homeassistant/climate/") + _prefix + F("_dhw/config")).c_str(), NULL, true);
  }

private:
  String _prefix = "opentherm";
  String _deviceVersion = "1.0";
  String _deviceManufacturer = "Community";
  String _deviceModel = "Opentherm Gateway";
  String _deviceName = "Opentherm Gateway";
  String _deviceConfigUrl = "";
};
