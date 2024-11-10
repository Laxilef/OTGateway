#pragma once
#include <Arduino.h>
#include "strings.h"

class HomeAssistantHelper {
public:
  typedef std::function<void(const char*, bool)> PublishEventCallback;

  HomeAssistantHelper() = default;

  void setWriter() {
    this->writer = nullptr;
  }

  void setWriter(MqttWriter* writer) {
    this->writer = writer;
  }

  void setPublishEventCallback(PublishEventCallback callback) {
    this->publishEventCallback = callback;
  }

  void setDevicePrefix(const char* value) {
    this->devicePrefix = value;
  }

  void setDeviceVersion(const char* value) {
    this->deviceVersion = value;
  }

  void setDeviceManufacturer(const char* value) {
    this->deviceManufacturer = value;
  }

  void setDeviceModel(const char* value) {
    this->deviceModel = value;
  }

  void setDeviceName(const char* value) {
    this->deviceName = value;
  }

  void setDeviceConfigUrl(const char* value) {
    this->deviceConfigUrl = value;
  }

  bool publish(const char* topic, JsonDocument& doc) {
    if (this->writer == nullptr) {
      if (this->publishEventCallback) {
        this->publishEventCallback(topic, false);
      }
      
      return false;
    }

    doc[FPSTR(HA_DEVICE)][FPSTR(HA_IDENTIFIERS)][0] = this->devicePrefix;
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_SW_VERSION)] = this->deviceVersion;

    if (this->deviceManufacturer != nullptr) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_MANUFACTURER)] = this->deviceManufacturer;
    }

    if (this->deviceModel != nullptr) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_MODEL)] = this->deviceModel;
    }

    if (this->deviceName != nullptr) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_NAME)] = this->deviceName;
    }

    if (this->deviceConfigUrl != nullptr) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_CONF_URL)] = this->deviceConfigUrl;
    }

    bool result = this->writer->publish(topic, doc, true);
    doc.clear();
    doc.shrinkToFit();

    if (this->publishEventCallback) {
      this->publishEventCallback(topic, result);
    }

    return result;
  }

  bool publish(const char* topic) {
    if (this->writer == nullptr) {
      if (this->publishEventCallback) {
        this->publishEventCallback(topic, false);
      }

      return false;
    }

    bool result = writer->publish(topic, nullptr, 0, true);
    if (this->publishEventCallback) {
      this->publishEventCallback(topic, result);
    }

    return result;
  }

  template <class CT, class NT>
  String makeConfigTopic(CT category, NT name, char nameSeparator = '/') {
    String topic = "";
    topic.concat(this->prefix);
    topic.concat('/');
    topic.concat(category);
    topic.concat('/');
    topic.concat(this->devicePrefix);
    topic.concat(nameSeparator);
    topic.concat(name);
    topic.concat(F("/config"));
    return topic;
  }

  template <class T> 
  String getDeviceTopic(T value, char dpvSeparator = '/') {
    String topic = "";
    topic.concat(this->devicePrefix);
    topic.concat(dpvSeparator);
    topic.concat(value);
    return topic;
  }

  template <class CT, class NT>
  String getDeviceTopic(CT category, NT name, char dpcSeparator = '/', char cnSeparator = '/') {
    String topic = "";
    topic.concat(this->devicePrefix);
    topic.concat(dpcSeparator);
    topic.concat(category);
    topic.concat(cnSeparator);
    topic.concat(name);
    return topic;
  }

  template <class CT, class NT, class ST>
  String getDeviceTopic(CT category, NT name, ST suffix, char dpcSeparator = '/', char cnSeparator = '/', char nsSeparator = '/') {
    String topic = "";
    topic.concat(this->devicePrefix);
    topic.concat(dpcSeparator);
    topic.concat(category);
    topic.concat(cnSeparator);
    topic.concat(name);
    topic.concat(nsSeparator);
    topic.concat(suffix);
    return topic;
  }

  template <class T> 
  String getObjectIdWithPrefix(T value, char separator = '_') {
    String topic = "";
    topic.concat(this->devicePrefix);
    topic.concat(separator);
    topic.concat(value);
    return topic;
  }

protected:
  PublishEventCallback publishEventCallback;
  MqttWriter* writer = nullptr;
  const char* prefix = "homeassistant";
  const char* devicePrefix = "";
  const char* deviceVersion = "1.0";
  const char* deviceManufacturer = nullptr;
  const char* deviceModel = nullptr;
  const char* deviceName = nullptr;
  const char* deviceConfigUrl = nullptr;
};
