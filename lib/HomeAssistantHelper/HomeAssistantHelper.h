#pragma once
#include <Arduino.h>

class HomeAssistantHelper {
public:
  HomeAssistantHelper(PubSubClient& client) :
    client(&client)
  {
  }

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

  bool publish(const char* topic, JsonDocument& doc) {
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_IDENTIFIERS)][0] = _prefix;
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_SW_VERSION)] = _deviceVersion;
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_MANUFACTURER)] = _deviceManufacturer;
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_MODEL)] = _deviceModel;
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_NAME)] = _deviceName;
    if (_deviceConfigUrl) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_CONF_URL)] = _deviceConfigUrl;
    }

    client->beginPublish(topic, measureJson(doc), true);
    serializeJson(doc, *client);
    return client->endPublish();
  }

  bool publish(const char* topic) {
    return client->publish(topic, NULL, true);
  }

  String getTopic(const char* category, const char* name, const char* nameSeparator = "/") {
    String topic = "homeassistant/";
    topic.concat(category);
    topic.concat("/");
    topic.concat(_prefix);
    topic.concat(nameSeparator);
    topic.concat(name);
    topic.concat("/config");
    return topic;
  }

protected:
  PubSubClient* client;
  String _prefix = "";
  String _deviceVersion = "1.0";
  String _deviceManufacturer = "Community";
  String _deviceModel = "";
  String _deviceName = "";
  String _deviceConfigUrl = "";
};
