#pragma once
#include <Arduino.h>
#include <StreamUtils.h>

class HomeAssistantHelper {
public:
  HomeAssistantHelper() {}

  HomeAssistantHelper(PubSubClient* client) {
    this->setClient(client);
  }

  void setClient() {
    this->client = nullptr;
  }

  void setClient(PubSubClient* client) {
    this->client = client;
  }

  void setYieldCallback(void(*yieldCallback)(void*)) {
    this->yieldCallback = yieldCallback;
    this->yieldArg = nullptr;
  }

  void setYieldCallback(void(*yieldCallback)(void*), void* arg) {
    this->yieldCallback = yieldCallback;
    this->yieldArg = arg;
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
    if (this->client == nullptr) {
      return false;
    }

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


    size_t docSize = measureJson(doc);
    uint8_t* buffer = (uint8_t*) malloc(docSize * sizeof(*buffer));
    size_t length = serializeJson(doc, buffer, docSize);

    size_t written = 0;
    if (length != 0) {
      if (this->client->beginPublish(topic, docSize, true)) {
        for (size_t offset = 0; offset < docSize; offset += 128) {
          size_t packetSize = offset + 128 <= docSize ? 128 : docSize - offset;
          written += this->client->write(buffer + offset, packetSize);
        }
        
        this->client->flush();
      }
    }
    free(buffer);

    Log.straceln("MQTT", "Publish %u of %u bytes to topic: %s", written, docSize, topic);

    if (this->yieldCallback != nullptr) {
      this->yieldCallback(yieldArg);
    }

    return docSize == written;
  }

  bool publish(const char* topic) {
    if (this->client == nullptr) {
      return false;
    }

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
  PubSubClient* client = nullptr;
  String prefix = "homeassistant";
  String devicePrefix = "";
  String deviceVersion = "1.0";
  String deviceManufacturer = "Community";
  String deviceModel = "";
  String deviceName = "";
  String deviceConfigUrl = "";
};
