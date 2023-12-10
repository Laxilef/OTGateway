#pragma once
#include <Arduino.h>
#include <StreamUtils.h>

class HomeAssistantHelper {
public:
  HomeAssistantHelper(PubSubClient& client) {
    this->client = &client;
  }

  void setYieldCallback(void(*yieldCallback)(void*)) {
    this->yieldCallback = yieldCallback;
    this->yieldArg = nullptr;
  }

  void setYieldCallback(void(*yieldCallback)(void*), void* arg) {
    this->yieldCallback = yieldCallback;
    this->yieldArg = arg;
  }

  void setBufferedClient() {
    this->bClient = nullptr;
  }

  void setBufferedClient(BufferingPrint* bClient) {
    this->bClient = bClient;
  }

  void setDevicePrefix(String value) {
    devicePrefix = value;
  }

  void setDeviceVersion(String value) {
    deviceVersion = value;
  }

  void setDeviceManufacturer(String value) {
    deviceManufacturer = value;
  }

  void setDeviceModel(String value) {
    deviceModel = value;
  }

  void setDeviceName(String value) {
    deviceName = value;
  }

  void setDeviceConfigUrl(String value) {
    deviceConfigUrl = value;
  }

  bool publish(const char* topic, JsonDocument& doc) {
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_IDENTIFIERS)][0] = devicePrefix;
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_SW_VERSION)] = deviceVersion;

    if (deviceManufacturer) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_MANUFACTURER)] = deviceManufacturer;
    }
    
    if (deviceModel) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_MODEL)] = deviceModel;
    }

    if (deviceName) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_NAME)] = deviceName;
    }
    
    if (deviceConfigUrl) {
      doc[FPSTR(HA_DEVICE)][FPSTR(HA_CONF_URL)] = deviceConfigUrl;
    }

    client->beginPublish(topic, measureJson(doc), true);
    if (this->bClient != nullptr) {
      serializeJson(doc, *this->bClient);
      this->bClient->flush();

    } else {
      serializeJson(doc, *client);
    }

    int pubResult = client->endPublish();
    if (this->yieldCallback != nullptr) {
      this->yieldCallback(yieldArg);
    }

    return pubResult;
  }

  bool publish(const char* topic) {
    return client->publish(topic, NULL, true);
  }

  String getTopic(const char* category, const char* name, const char* nameSeparator = "/") {
    String topic = "";
    topic.concat(prefix);
    topic.concat("/");
    topic.concat(category);
    topic.concat("/");
    topic.concat(devicePrefix);
    topic.concat(nameSeparator);
    topic.concat(name);
    topic.concat("/config");
    return topic;
  }

protected:
  void(*yieldCallback)(void*) = nullptr;
  void* yieldArg = nullptr;
  PubSubClient* client;
  BufferingPrint* bClient = nullptr;
  String prefix = "homeassistant";
  String devicePrefix = "";
  String deviceVersion = "1.0";
  String deviceManufacturer = "Community";
  String deviceModel = "";
  String deviceName = "";
  String deviceConfigUrl = "";
};
