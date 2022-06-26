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

  bool publishSelectOutdoorTempSource(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = F("{\"outdoorTempSource\": {% if value == 'Boiler' %}0{% elif value == 'Manual' %}1{% elif value == 'External' %}2{% endif %}}");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_outdoorTempSource";
    doc["object_id"] = _prefix + "_outdoorTempSource";
    doc["entity_category"] = "config";
    doc["name"] = "Outdoor temperature source";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = F("{% if value_json.outdoorTempSource == 0 %}Boiler{% elif value_json.outdoorTempSource == 1 %}Manual{% elif value_json.outdoorTempSource == 2 %}External{% endif %}");
    doc["options"][0] = F("Boiler");
    doc["options"][1] = F("Manual");
    doc["options"][2] = F("External");

    client.beginPublish((F("homeassistant/select/") + _prefix + "/outdoorTempSource/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSwitchDebug(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_debug";
    doc["object_id"] = _prefix + "_debug";
    doc["entity_category"] = "config";
    doc["name"] = "Debug";
    doc["icon"] = "mdi:code-braces";
    doc["state_topic"] = _prefix + F("/settings");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.debug }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["payload_on"] = "{\"debug\": true}";
    doc["payload_off"] = "{\"debug\": false}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/debug/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchEmergency(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_emergency";
    doc["object_id"] = _prefix + "_emergency";
    doc["entity_category"] = "config";
    doc["name"] = "Use emergency";
    doc["icon"] = "mdi:sun-snowflake-variant";
    doc["state_topic"] = _prefix + F("/settings");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.emergency.enable }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["payload_on"] = "{\"emergency\": {\"enable\" : true}}";
    doc["payload_off"] = "{\"emergency\": {\"enable\" : false}}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/emergency/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEmergencyTarget(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_emergency_target";
    doc["object_id"] = _prefix + "_emergency_target";
    doc["entity_category"] = "config";
    doc["device_class"] = "temperature";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Emergency target temp";
    doc["icon"] = "mdi:thermometer-alert";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.emergency.target|float(0)|round(1) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"emergency\": {\"target\" : {{ value }}}}";
    doc["min"] = 5;
    doc["max"] = 50;
    doc["step"] = 0.5;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/emergency_target/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSwitchEmergencyUseEquitherm(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/settings");
    doc["availability"]["value_template"] = F("{{ iif(value_json.outdoorTempSource != 1, 'online', 'offline') }}");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_emergency_use_equitherm";
    doc["object_id"] = _prefix + "_emergency_use_equitherm";
    doc["entity_category"] = "config";
    doc["name"] = "Use equitherm in emergency";
    doc["icon"] = "mdi:snowflake-alert";
    doc["state_topic"] = _prefix + F("/settings");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.emergency.useEquitherm }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["payload_on"] = "{\"emergency\": {\"useEquitherm\" : true}}";
    doc["payload_off"] = "{\"emergency\": {\"useEquitherm\" : false}}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/emergency_use_equitherm/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchHeating(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_heating";
    doc["object_id"] = _prefix + "_heating";
    doc["entity_category"] = "config";
    doc["name"] = "Heating";
    doc["icon"] = "mdi:radiator";
    doc["state_topic"] = _prefix + F("/settings");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.heating.enable }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["payload_on"] = "{\"heating\": {\"enable\" : true}}";
    doc["payload_off"] = "{\"heating\": {\"enable\" : false}}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/heating/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberHeatingTarget(byte minTemp = 20, byte maxTemp = 90, bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_heating_target";
    doc["object_id"] = _prefix + "_heating_target";
    doc["entity_category"] = "config";
    doc["device_class"] = "temperature";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Heating target";
    doc["icon"] = "mdi:radiator";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.heating.target|float(0)|round(1) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"heating\": {\"target\" : {{ value }}}}";
    doc["min"] = minTemp;
    doc["max"] = maxTemp;
    doc["step"] = 0.5;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/heating_target/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberHeatingHysteresis(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_heating_hysteresis";
    doc["object_id"] = _prefix + "_heating_hysteresis";
    doc["entity_category"] = "config";
    doc["device_class"] = "temperature";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Heating hysteresis";
    doc["icon"] = "mdi:altimeter";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.heating.hysteresis|float(0)|round(1) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"heating\": {\"hysteresis\" : {{ value }}}}";
    doc["min"] = 0;
    doc["max"] = 5;
    doc["step"] = 0.1;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/heating_hysteresis/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorHeatingSetpoint(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_heating_setpoint";
    doc["object_id"] = _prefix + "_heating_setpoint";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Heating setpoint";
    doc["icon"] = "mdi:coolant-temperature";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.parameters.heatingSetpoint|int(0) }}";

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/heating_setpoint/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchDHW(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_dhw";
    doc["object_id"] = _prefix + "_dhw";
    doc["entity_category"] = "config";
    doc["name"] = "DHW";
    doc["icon"] = "mdi:water-pump";
    doc["state_topic"] = _prefix + F("/settings");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.dhw.enable }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["payload_on"] = "{\"dhw\": {\"enable\" : true}}";
    doc["payload_off"] = "{\"dhw\": {\"enable\" : false}}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/dhw/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberDHWTarget(byte minTemp = 40, byte maxTemp = 60, bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_dhw_target";
    doc["object_id"] = _prefix + "_dhw_target";
    doc["entity_category"] = "config";
    doc["device_class"] = "temperature";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "DHW target";
    doc["icon"] = "mdi:water-pump";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.dhw.target|int(0) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"dhw\": {\"target\" : {{ value|int(0) }}}}";
    doc["min"] = minTemp;
    doc["max"] = maxTemp;
    doc["step"] = 1;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/dhw_target/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchPID(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_pid";
    doc["object_id"] = _prefix + "_pid";
    doc["entity_category"] = "config";
    doc["name"] = "PID";
    doc["icon"] = "mdi:chart-bar-stacked";
    doc["state_topic"] = _prefix + F("/settings");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.pid.enable }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["payload_on"] = "{\"pid\": {\"enable\" : true}}";
    doc["payload_off"] = "{\"pid\": {\"enable\" : false}}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/pid/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberPIDFactorP(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["unique_id"] = _prefix + "_pid_p";
    doc["object_id"] = _prefix + "_pid_p";
    doc["entity_category"] = "config";
    doc["name"] = "PID factor P";
    doc["icon"] = "mdi:alpha-p-circle-outline";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.pid.p_factor|float(0)|round(3) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"pid\": {\"p_factor\" : {{ value }}}}";
    doc["min"] = 0.001;
    doc["max"] = 3;
    doc["step"] = 0.001;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/pid_p_factor/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberPIDFactorI(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["unique_id"] = _prefix + "_pid_i";
    doc["object_id"] = _prefix + "_pid_i";
    doc["entity_category"] = "config";
    doc["name"] = "PID factor I";
    doc["icon"] = "mdi:alpha-i-circle-outline";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.pid.i_factor|float(0)|round(3) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"pid\": {\"i_factor\" : {{ value }}}}";
    doc["min"] = 0;
    doc["max"] = 3;
    doc["step"] = 0.001;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/pid_i_factor/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberPIDFactorD(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["unique_id"] = _prefix + "_pid_d";
    doc["object_id"] = _prefix + "_pid_d";
    doc["entity_category"] = "config";
    doc["name"] = "PID factor D";
    doc["icon"] = "mdi:alpha-d-circle-outline";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.pid.d_factor|float(0)|round(3) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"pid\": {\"d_factor\" : {{ value }}}}";
    doc["min"] = 0;
    doc["max"] = 3;
    doc["step"] = 0.001;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/pid_d_factor/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchEquitherm(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_equitherm";
    doc["object_id"] = _prefix + "_equitherm";
    doc["entity_category"] = "config";
    doc["name"] = "Equitherm";
    doc["icon"] = "mdi:sun-snowflake-variant";
    doc["state_topic"] = _prefix + F("/settings");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.equitherm.enable }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["payload_on"] = "{\"equitherm\": {\"enable\" : true}}";
    doc["payload_off"] = "{\"equitherm\": {\"enable\" : false}}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/equitherm/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEquithermFactorN(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["unique_id"] = _prefix + "_equitherm_n";
    doc["object_id"] = _prefix + "_equitherm_n";
    doc["entity_category"] = "config";
    doc["name"] = "Equitherm factor N";
    doc["icon"] = "mdi:alpha-n-circle-outline";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.equitherm.n_factor|float(0)|round(3) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"equitherm\": {\"n_factor\" : {{ value }}}}";
    doc["min"] = 0.001;
    doc["max"] = 5;
    doc["step"] = 0.001;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/equitherm_n_factor/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEquithermFactorK(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["unique_id"] = _prefix + "_equitherm_k";
    doc["object_id"] = _prefix + "_equitherm_k";
    doc["entity_category"] = "config";
    doc["name"] = "Equitherm factor K";
    doc["icon"] = "mdi:alpha-k-circle-outline";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.equitherm.k_factor|float(0)|round(2) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"equitherm\": {\"k_factor\" : {{ value }}}}";
    doc["min"] = 0;
    doc["max"] = 10;
    doc["step"] = 0.01;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/equitherm_k_factor/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberEquithermFactorT(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["unique_id"] = _prefix + "_equitherm_t";
    doc["object_id"] = _prefix + "_equitherm_t";
    doc["entity_category"] = "config";
    doc["name"] = "Equitherm factor T";
    doc["icon"] = "mdi:alpha-t-circle-outline";
    doc["state_topic"] = _prefix + F("/settings");
    doc["value_template"] = "{{ value_json.equitherm.t_factor|float(0)|round(2) }}";
    doc["command_topic"] = _prefix + "/settings/set";
    doc["command_template"] = "{\"equitherm\": {\"t_factor\" : {{ value }}}}";
    doc["min"] = 0;
    doc["max"] = 10;
    doc["step"] = 0.01;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/equitherm_t_factor/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSwitchTuning(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_tuning";
    doc["object_id"] = _prefix + "_tuning";
    doc["entity_category"] = "config";
    doc["name"] = "Tuning";
    doc["icon"] = "mdi:tune-vertical";
    doc["state_topic"] = _prefix + F("/state");
    doc["state_on"] = true;
    doc["state_off"] = false;
    doc["value_template"] = "{{ value_json.tuning.enable }}";
    doc["command_topic"] = _prefix + "/state/set";
    doc["payload_on"] = "{\"tuning\": {\"enable\" : true}}";
    doc["payload_off"] = "{\"tuning\": {\"enable\" : false}}";

    client.beginPublish((F("homeassistant/switch/") + _prefix + "/tuning/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSelectTuningRegulator(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["availability_mode"] = F("all");
    doc["command_topic"] = _prefix + "/state/set";
    doc["command_template"] = F("{\"tuning\": {\"regulator\": {% if value == 'Equitherm' %}0{% elif value == 'PID' %}1{% endif %}}}");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_tuning_regulator";
    doc["object_id"] = _prefix + "_tuning_regulator";
    doc["entity_category"] = "config";
    doc["name"] = "Tuning regulator";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = F("{% if value_json.tuning.regulator == 0 %}Equitherm{% elif value_json.tuning.regulator == 1 %}PID{% endif %}");
    doc["options"][0] = F("Equitherm");
    doc["options"][1] = F("PID");

    client.beginPublish((F("homeassistant/select/") + _prefix + "/tuning_regulator/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishBinSensorStatus(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_status";
    doc["object_id"] = _prefix + "_status";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "problem";
    doc["name"] = "Status";
    doc["icon"] = "mdi:list-status";
    doc["state_topic"] = _prefix + F("/status");
    doc["value_template"] = "{{ iif(value == 'online', 'OFF', 'ON') }}";
    doc["expire_after"] = 60;

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + "/status/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorOtStatus(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_ot_status";
    doc["object_id"] = _prefix + "_ot_status";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "problem";
    doc["name"] = "Opentherm status";
    doc["icon"] = "mdi:list-status";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ iif(value_json.states.otStatus, 'OFF', 'ON') }}";

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + "/ot_status/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorHeating(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_heating";
    doc["object_id"] = _prefix + "_heating";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "running";
    doc["name"] = "Heating";
    doc["icon"] = "mdi:radiator";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = F("{{ iif(value_json.states.heating, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + "/heating/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorDHW(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_dhw";
    doc["object_id"] = _prefix + "_dhw";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "running";
    doc["name"] = "DHW";
    doc["icon"] = "mdi:water-pump";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = F("{{ iif(value_json.states.dhw, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + "/dhw/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorFlame(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_flame";
    doc["object_id"] = _prefix + "_flame";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "running";
    doc["name"] = "Flame";
    doc["icon"] = "mdi:fire";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = F("{{ iif(value_json.states.flame, 'ON', 'OFF') }}");

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + "/flame/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorFault(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/state");
    doc["availability"]["value_template"] = F("{{ iif(value_json.states.otStatus, 'online', 'offline') }}");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_fault";
    doc["object_id"] = _prefix + "_fault";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "problem";
    doc["name"] = "Fault";
    doc["icon"] = "mdi:water-boiler-alert";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ iif(value_json.states.fault, 'ON', 'OFF') }}";

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + "/fault/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishBinSensorDiagnostic(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_diagnostic";
    doc["object_id"] = _prefix + "_diagnostic";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "problem";
    doc["name"] = "Diagnostic";
    doc["icon"] = "mdi:account-wrench";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ iif(value_json.states.diagnostic, 'ON', 'OFF') }}";

    client.beginPublish((F("homeassistant/binary_sensor/") + _prefix + "/diagnostic/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorFaultCode(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/state");
    doc["availability"]["value_template"] = F("{{ iif(value_json.states.fault, 'online', 'offline') }}");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_fault_code";
    doc["object_id"] = _prefix + "_fault_code";
    doc["entity_category"] = "diagnostic";
    doc["name"] = "Fault code";
    doc["icon"] = "mdi:chat-alert-outline";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = F("{{ \"E%02d\"|format(value_json.states.faultCode) }}");

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/fault_code/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishSensorModulation(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_modulation_level";
    doc["object_id"] = _prefix + "_modulation_level";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "power_factor";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "%";
    doc["name"] = "Modulation level";
    doc["icon"] = "mdi:fire-circle";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.sensors.modulation|float(0)|round(0) }}";

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/modulation/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorPressure(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_pressure";
    doc["object_id"] = _prefix + "_pressure";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "pressure";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "bar";
    doc["name"] = "Pressure";
    doc["icon"] = "mdi:gauge";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.sensors.pressure|float(0)|round(2) }}";

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/pressure/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishNumberIndoorTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    //doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_indoor_temp";
    doc["object_id"] = _prefix + "_indoor_temp";
    doc["entity_category"] = "config";
    //doc["entity_registry_visible_default"] = false;
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Indoor temperature";
    doc["icon"] = "mdi:home-thermometer";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.temperatures.indoor|float(0)|round(1) }}";
    doc["command_topic"] = _prefix + "/state/set";
    doc["command_template"] = "{\"temperatures\": {\"indoor\":{{ value }}}}";
    doc["min"] = -70;
    doc["max"] = 50;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/indoor_temp/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishNumberOutdoorTemp(bool enabledByDefault = true) {
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
    doc["unique_id"] = _prefix + "_outdoor_temp";
    doc["object_id"] = _prefix + "_outdoor_temp";
    doc["entity_category"] = "config";
    //doc["entity_registry_visible_default"] = false;
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Outdoor temperature";
    doc["icon"] = "mdi:home-thermometer-outline";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.temperatures.outdoor|float(0)|round(1) }}";
    doc["command_topic"] = _prefix + "/state/set";
    doc["command_template"] = "{\"temperatures\": {\"outdoor\":{{ value }}}}";
    doc["min"] = -70;
    doc["max"] = 50;

    client.beginPublish((F("homeassistant/number/") + _prefix + "/outdoor_temp/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorOutdoorTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"][0]["topic"] = _prefix + F("/status");
    doc["availability"][1]["topic"] = _prefix + F("/settings");
    doc["availability"][1]["value_template"] = F("{{ iif(value_json.outdoorTempSource == 2, 'online', 'offline') }}");
    doc["availability_mode"] = "any";
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_outdoor_temp";
    doc["object_id"] = _prefix + "_outdoor_temp";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Outdoor temperature";
    doc["icon"] = "mdi:home-thermometer-outline";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.temperatures.outdoor|float(0)|round(1) }}";

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/outdoor_temp/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorHeatingTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_heating_temp";
    doc["object_id"] = _prefix + "_heating_temp";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "Heating temperature";
    doc["icon"] = "mdi:radiator";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.temperatures.heating|float(0)|round(2) }}";

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/heating_temp/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishSensorDHWTemp(bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_dhw_temp";
    doc["object_id"] = _prefix + "_dhw_temp";
    doc["entity_category"] = "diagnostic";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "°C";
    doc["name"] = "DHW temperature";
    doc["icon"] = "mdi:water-pump";
    doc["state_topic"] = _prefix + F("/state");
    doc["value_template"] = "{{ value_json.temperatures.dhw|float(0)|round(2) }}";

    client.beginPublish((F("homeassistant/sensor/") + _prefix + "/dhw_temp/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool publishClimateHeating(byte minTemp = 20, byte maxTemp = 90, bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }
    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_heating";
    doc["object_id"] = _prefix + "_heating";
    doc["name"] = "Heating";
    doc["icon"] = "mdi:radiator";

    doc["current_temperature_topic"] = _prefix + F("/state");
    doc["value_template"] = F("{{ value_json.temperatures.indoor|float(0)|round(2) }}");

    doc["temperature_command_topic"] = _prefix + "/settings/set";
    doc["temperature_command_template"] = "{\"heating\": {\"target\" : {{ value }}}}";

    doc["temperature_state_topic"] = _prefix + F("/settings");
    doc["temperature_state_template"] = F("{{ value_json.heating.target|float(0)|round(1) }}");

    doc["mode_command_topic"] = _prefix + "/settings/set";
    doc["mode_command_template"] = F("{% if value == 'heat' %}{\"heating\": {\"enable\" : true}}"
      "{% elif value == 'off' %}{\"heating\": {\"enable\" : false}}{% endif %}");
    doc["mode_state_topic"] = _prefix + F("/settings");
    doc["mode_state_template"] = F("{{ iif(value_json.heating.enable, 'heat', 'off') }}");
    doc["modes"][0] = "off";
    doc["modes"][1] = "heat";
    doc["min_temp"] = minTemp;
    doc["max_temp"] = maxTemp;
    doc["temp_step"] = 0.5;

    client.beginPublish((F("homeassistant/climate/") + _prefix + "_heating/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  bool publishClimateDHW(byte minTemp = 40, byte maxTemp = 60, bool enabledByDefault = true) {
    StaticJsonDocument<1536> doc;
    doc["availability"]["topic"] = _prefix + F("/status");
    doc["device"]["identifiers"][0] = _prefix;
    doc["device"]["sw_version"] = _deviceVersion;
    doc["device"]["manufacturer"] = _deviceManufacturer;
    doc["device"]["model"] = _deviceModel;
    doc["device"]["name"] = _deviceName;
    if (_deviceConfigUrl) {
      doc["device"]["configuration_url"] = _deviceConfigUrl;
    }

    doc["enabled_by_default"] = enabledByDefault;
    doc["unique_id"] = _prefix + "_dhw";
    doc["object_id"] = _prefix + "_dhw";
    doc["name"] = "DHW";
    doc["icon"] = "mdi:water-pump";

    doc["current_temperature_topic"] = _prefix + F("/state");
    doc["value_template"] = F("{{ value_json.temperatures.dhw|float(0)|round(1) }}");

    doc["temperature_command_topic"] = _prefix + "/settings/set";
    doc["temperature_command_template"] = "{\"dhw\": {\"target\" : {{ value|int(0) }}}}";

    doc["temperature_state_topic"] = _prefix + F("/settings");
    doc["temperature_state_template"] = F("{{ value_json.dhw.target|int(0) }}");

    doc["mode_command_topic"] = _prefix + "/settings/set";
    doc["mode_command_template"] = F("{% if value == 'heat' %}{\"dhw\": {\"enable\" : true}}"
      "{% elif value == 'off' %}{\"dhw\": {\"enable\" : false}}{% endif %}");
    doc["mode_state_topic"] = _prefix + F("/settings");
    doc["mode_state_template"] = F("{{ iif(value_json.dhw.enable, 'heat', 'off') }}");
    doc["modes"][0] = "off";
    doc["modes"][1] = "heat";
    doc["min_temp"] = minTemp;
    doc["max_temp"] = maxTemp;

    client.beginPublish((F("homeassistant/climate/") + _prefix + "_dhw/config").c_str(), measureJson(doc), true);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }


  bool deleteNumberOutdoorTemp() {
    return client.publish((F("homeassistant/number/") + _prefix + "/outdoor_temp/config").c_str(), NULL, true);
  }

  bool deleteSensorOutdoorTemp() {
    return client.publish((F("homeassistant/sensor/") + _prefix + "/outdoor_temp/config").c_str(), NULL, true);
  }

private:
  String _prefix = "opentherm";
  String _deviceVersion = "1.0";
  String _deviceManufacturer = "Community";
  String _deviceModel = "Opentherm Gateway";
  String _deviceName = "Opentherm Gateway";
  String _deviceConfigUrl = "";
};
