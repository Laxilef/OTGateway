#include <unordered_map>
#include <MqttClient.h>
#include <MqttWiFiClient.h>
#include <MqttWriter.h>
#include "HaHelper.h"

extern FileData fsSettings;

class MqttTask : public Task {
public:
  MqttTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {
    this->wifiClient = new MqttWiFiClient();
    this->client = new MqttClient(this->wifiClient);
    this->writer = new MqttWriter(this->client, 256);
    this->haHelper = new HaHelper();
  }

  ~MqttTask() {
    delete this->haHelper;

    if (this->client != nullptr) {
      if (this->client->connected()) {
        this->client->stop();

        if (this->connected) {
          this->onDisconnect();
          this->connected = false;
        }
      }

      delete this->client;
    }

    delete this->writer;
    delete this->wifiClient;
  }

  void disable() {
    this->client->stop();

    if (this->connected) {
      this->onDisconnect();
      this->connected = false;
    }
    
    Task::disable();

    Log.sinfoln(FPSTR(L_MQTT), F("Disabled"));
  }

  void enable() {
    Task::enable();

    Log.sinfoln(FPSTR(L_MQTT), F("Enabled"));
  }

  inline bool isConnected() {
    return this->connected;
  }

  inline void resetPublishedSettingsTime() {
    this->prevPubSettingsTime = 0;
  }

  inline void resetPublishedSensorTime(uint8_t sensorId) {
    this->prevPubSensorTime[sensorId] = 0;
  }

  inline void resetPublishedVarsTime() {
    this->prevPubVarsTime = 0;
  }

  inline void rebuildHaEntity(uint8_t sensorId, Sensors::Settings& prevSettings) {
    this->queueRebuildingHaEntities[sensorId] = prevSettings;
  }

protected:
  MqttWiFiClient* wifiClient = nullptr;
  MqttClient* client = nullptr;
  HaHelper* haHelper = nullptr;
  MqttWriter* writer = nullptr;
  UnitSystem currentUnitSystem = UnitSystem::METRIC;
  bool currentHomeAssistantDiscovery = false;
  std::unordered_map<uint8_t, Sensors::Settings> queueRebuildingHaEntities;
  unsigned short readyForSendTime = 30000;
  unsigned long lastReconnectTime = 0;
  unsigned long connectedTime = 0;
  unsigned long disconnectedTime = 0;
  unsigned long prevPubVarsTime = 0;
  unsigned long prevPubSettingsTime = 0;
  std::unordered_map<uint8_t, unsigned long> prevPubSensorTime;
  bool connected = false;
  bool newConnection = false;

  #if defined(ARDUINO_ARCH_ESP32)
  const char* getTaskName() override {
    return "Mqtt";
  }
  
  /*BaseType_t getTaskCore() override {
    return 1;
  }*/

  int getTaskPriority() override {
    return 2;
  }
  #endif

  inline bool isReadyForSend() {
    return millis() - this->connectedTime > this->readyForSendTime;
  }

  void setup() {
    Log.sinfoln(FPSTR(L_MQTT), F("Started"));

    // wificlient settings
    #ifdef ARDUINO_ARCH_ESP8266
    this->wifiClient->setSync(true);
    this->wifiClient->setNoDelay(true);
    #endif

    // client settings
    this->client->setKeepAliveInterval(15000);
    this->client->setTxPayloadSize(256);
    #ifdef ARDUINO_ARCH_ESP8266
    this->client->setConnectionTimeout(1000);
    #else
    this->client->setConnectionTimeout(3000);
    #endif

    this->client->onMessage([this] (void*, size_t length) {
      const String& topic = this->client->messageTopic();
      if (!length || length > 2048 || !topic.length()) {
        return;
      }

      uint8_t payload[length];
      for (size_t i = 0; i < length && this->client->available(); i++) {
        payload[i] = this->client->read();
      }
      
      this->onMessage(topic, payload, length);
    });

    // writer settings
    #ifdef ARDUINO_ARCH_ESP32
    this->writer->setYieldCallback([this] {
      this->delay(10);
    });
    #endif

    this->writer->setPublishEventCallback([this] (const char* topic, size_t written, size_t length, bool result) {
      Log.straceln(FPSTR(L_MQTT), F("%s publish %u of %u bytes to topic: %s"), result ? F("Successfully") : F("Failed"), written, length, topic);

      #ifdef ARDUINO_ARCH_ESP8266
      ::optimistic_yield(1000);
      #endif

      //this->client->poll();
      this->delay(250);
    });

    #ifdef ARDUINO_ARCH_ESP8266
    this->writer->setFlushEventCallback([this] (size_t, size_t) {
      ::optimistic_yield(1000);

      if (this->wifiClient->connected()) {
        this->wifiClient->flush();
      }

      ::optimistic_yield(1000);
    });
    #endif

    // ha helper settings
    this->haHelper->setDevicePrefix(settings.mqtt.prefix);
    this->haHelper->setDeviceVersion(BUILD_VERSION);
    this->haHelper->setDeviceModel(PROJECT_NAME);
    this->haHelper->setDeviceName(PROJECT_NAME);
    this->haHelper->setWriter(this->writer);

    sprintf(buffer, CONFIG_URL, WiFi.localIP().toString().c_str());
    this->haHelper->setDeviceConfigUrl(buffer);
  }

  void loop() {
    if (vars.states.restarting || vars.states.upgrading) {
      return;
    }

    if (this->connected && !this->client->connected()) {
      this->connected = false;
      this->onDisconnect();

    } else if (!this->connected && millis() - this->lastReconnectTime >= MQTT_RECONNECT_INTERVAL) {
      Log.sinfoln(FPSTR(L_MQTT), F("Connecting to %s:%u..."), settings.mqtt.server, settings.mqtt.port);

      this->haHelper->setDevicePrefix(settings.mqtt.prefix);
      this->haHelper->updateCachedTopics();
      this->client->stop();
      this->client->setId(networkSettings.hostname);
      this->client->setUsernamePassword(settings.mqtt.user, settings.mqtt.password);

      this->client->beginWill(this->haHelper->getDeviceTopic(F("status")).c_str(), 7, true, 1);
      this->client->print(F("offline"));
      this->client->endWill();

      this->client->connect(settings.mqtt.server, settings.mqtt.port);
      this->lastReconnectTime = millis();
      this->yield();

    } else if (!this->connected && this->client->connected()) {
      this->connected = true;
      this->onConnect();
    }

    if (!this->connected) {
      return;
    }

    this->client->poll();

    // delay for publish data
    if (!this->isReadyForSend()) {
      return;
    }

    #ifdef ARDUINO_ARCH_ESP8266
    ::optimistic_yield(1000);
    #endif

    // publish variables and status
    if (this->newConnection || millis() - this->prevPubVarsTime > (settings.mqtt.interval * 1000u)) {
      this->writer->publish(this->haHelper->getDeviceTopic(F("status")).c_str(), "online", false);
      this->publishVariables(this->haHelper->getDeviceTopic(F("state")).c_str());
      this->prevPubVarsTime = millis();
    }

    // publish settings
    if (this->newConnection || millis() - this->prevPubSettingsTime > (settings.mqtt.interval * 10000u)) {
      this->publishSettings(this->haHelper->getDeviceTopic(F("settings")).c_str());
      this->prevPubSettingsTime = millis();
    }

    // publish sensors
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      if (!Sensors::hasEnabledAndValid(sensorId)) {
        continue;
      }

      auto& rSensor = Sensors::results[sensorId];
      bool needUpdate = false;
      if (millis() - this->prevPubSensorTime[sensorId] > ((this->haHelper->getExpireAfter() - 10) * 1000u)) {
        needUpdate = true;

      } else if (rSensor.activityTime >= this->prevPubSensorTime[sensorId]) {
        auto estimated = rSensor.activityTime - this->prevPubSensorTime[sensorId];
        needUpdate = estimated > 1000u;
      }

      if (this->newConnection || needUpdate) {
        this->publishSensor(sensorId);
        this->prevPubSensorTime[sensorId] = millis();
      }
    }

    // publish ha entities if not published
    if (settings.mqtt.homeAssistantDiscovery) {
      if (this->newConnection || !this->currentHomeAssistantDiscovery || this->currentUnitSystem != settings.system.unitSystem) {
        this->publishHaEntities();
        this->publishNonStaticHaEntities(true);
        this->currentHomeAssistantDiscovery = true;
        this->currentUnitSystem = settings.system.unitSystem;

      } else {
        // publish non static ha entities
        this->publishNonStaticHaEntities();
      }


      for (auto& [sensorId, prevSettings] : this->queueRebuildingHaEntities) {
        Log.sinfoln(FPSTR(L_MQTT_HA), F("Rebuilding config for sensor #%hhu '%s'"), sensorId, prevSettings.name);

        // delete old config
        if (strlen(prevSettings.name) && prevSettings.enabled) {
          switch (prevSettings.type) {
            case Sensors::Type::BLUETOOTH:
              this->haHelper->deleteConnectionDynamicSensor(prevSettings);
              this->haHelper->deleteSignalQualityDynamicSensor(prevSettings);
              this->haHelper->deleteDynamicSensor(prevSettings, Sensors::ValueType::TEMPERATURE);
              this->haHelper->deleteDynamicSensor(prevSettings, Sensors::ValueType::HUMIDITY);
              this->haHelper->deleteDynamicSensor(prevSettings, Sensors::ValueType::BATTERY);
              this->haHelper->deleteDynamicSensor(prevSettings, Sensors::ValueType::RSSI);
              break;

            case Sensors::Type::DALLAS_TEMP:
              this->haHelper->deleteConnectionDynamicSensor(prevSettings);
              this->haHelper->deleteSignalQualityDynamicSensor(prevSettings);
              this->haHelper->deleteDynamicSensor(prevSettings, Sensors::ValueType::TEMPERATURE);
              break;
            
            case Sensors::Type::MANUAL:
              this->client->unsubscribe(
                this->haHelper->getDeviceTopic(
                  F("sensors"),
                  Sensors::makeObjectId(prevSettings.name).c_str(),
                  F("set")
                ).c_str()
              );

            default:
              this->haHelper->deleteDynamicSensor(prevSettings, Sensors::ValueType::PRIMARY);
          }
        }

        if (!Sensors::hasEnabledAndValid(sensorId)) {
          continue;
        }

        // make new config
        auto& sSettings = Sensors::settings[sensorId];
        switch (sSettings.type) {
          case Sensors::Type::BLUETOOTH:
            this->haHelper->publishConnectionDynamicSensor(sSettings);
            this->haHelper->publishSignalQualityDynamicSensor(sSettings, false);
            this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::TEMPERATURE, settings.system.unitSystem);
            this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::HUMIDITY, settings.system.unitSystem);
            this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::BATTERY, settings.system.unitSystem);
            this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::RSSI, settings.system.unitSystem, false);
            break;

          case Sensors::Type::DALLAS_TEMP:
            this->haHelper->publishConnectionDynamicSensor(sSettings);
            this->haHelper->publishSignalQualityDynamicSensor(sSettings, false);
            this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::TEMPERATURE, settings.system.unitSystem);
            break;

          case Sensors::Type::MANUAL: 
            this->client->subscribe(
              this->haHelper->getDeviceTopic(
                F("sensors"),
                Sensors::makeObjectId(prevSettings.name).c_str(),
                F("set")
              ).c_str()
            );
          
          default:
            this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::PRIMARY, settings.system.unitSystem);
        }
      }
      this->queueRebuildingHaEntities.clear();

    } else if (this->currentHomeAssistantDiscovery) {
      this->currentHomeAssistantDiscovery = false;
    }

    if (this->newConnection) {
      this->newConnection = false;
    }
  }

  void onConnect() {
    this->connectedTime = millis();
    this->newConnection = true;
    unsigned long downtime = (millis() - this->disconnectedTime) / 1000;
    Log.sinfoln(FPSTR(L_MQTT), F("Connected (downtime: %u s.)"), downtime);

    this->client->subscribe(this->haHelper->getDeviceTopic(F("settings/set")).c_str());
    this->client->subscribe(this->haHelper->getDeviceTopic(F("state/set")).c_str());
  }

  void onDisconnect() {
    this->disconnectedTime = millis();

    unsigned long uptime = (millis() - this->connectedTime) / 1000;
    Log.swarningln(FPSTR(L_MQTT), F("Disconnected (reason: %d uptime: %lu s.)"), this->client->connectError(), uptime);
  }

  void onMessage(const String& topic, uint8_t* payload, size_t length) {
    if (!length) {
      return;
    }

    if (settings.system.logLevel >= TinyLogger::Level::TRACE) {
      Log.strace(FPSTR(L_MQTT_MSG), F("Topic: %s\r\n>  "), topic.c_str());
      if (Log.lock()) {
        for (size_t i = 0; i < length; i++) {
          if (payload[i] == 0) {
            break;
          } else if (payload[i] == 13) {
            continue;
          } else if (payload[i] == 10) {
            Log.print(F("\r\n>  "));
          } else {
            Log.print((char) payload[i]);
          }
        }
        Log.print(F("\r\n\n"));
        Log.flush();
        Log.unlock();
      }
    }

    JsonDocument doc;
    DeserializationError dErr = deserializeJson(doc, payload, length);
    if (dErr != DeserializationError::Ok) {
      Log.swarningln(FPSTR(L_MQTT_MSG), F("Error on deserialization: %s"), dErr.f_str());
      return;

    } else if (doc.isNull() || !doc.size()) {
      Log.swarningln(FPSTR(L_MQTT_MSG), F("Not valid json"));
      return;
    }
    doc.shrinkToFit();

    // delete topic
    this->writer->publish(topic.c_str(), nullptr, 0, true);

    if (this->haHelper->getDeviceTopic(F("state/set")).equals(topic)) {
      if (jsonToVars(doc, vars)) {
        this->resetPublishedVarsTime();
      }

    } else if (this->haHelper->getDeviceTopic(F("settings/set")).equals(topic)) {
      if (safeJsonToSettings(doc, settings)) {
        this->resetPublishedSettingsTime();
        fsSettings.update();
      }

    } else {
      const String& sensorsTopic = this->haHelper->getDeviceTopic(F("sensors/"));
      auto stLength = sensorsTopic.length();

      if (topic.startsWith(sensorsTopic) && topic.endsWith(F("/set"))) {
        if (topic.length() > stLength + 4) {
          const String& name = topic.substring(stLength, topic.indexOf('/', stLength));
          int16_t id = Sensors::getIdByObjectId(name.c_str());

          if (id == -1) {
            return;
          }

          if (jsonToSensorResult(id, doc)) {
            this->resetPublishedSensorTime(id);
          }
        }
      }
    }
  }

  void publishHaEntities() {
    // heating
    this->haHelper->publishSwitchHeatingTurbo(false);
    this->haHelper->publishInputHeatingHysteresis(settings.system.unitSystem);
    this->haHelper->publishInputHeatingTurboFactor(false);
    this->haHelper->publishInputHeatingMinTemp(settings.system.unitSystem);
    this->haHelper->publishInputHeatingMaxTemp(settings.system.unitSystem);

    // pid
    this->haHelper->publishSwitchPid();
    this->haHelper->publishInputPidFactorP(false);
    this->haHelper->publishInputPidFactorI(false);
    this->haHelper->publishInputPidFactorD(false);
    this->haHelper->publishInputPidDt(false);
    this->haHelper->publishInputPidMinTemp(settings.system.unitSystem, false);
    this->haHelper->publishInputPidMaxTemp(settings.system.unitSystem, false);

    // equitherm
    this->haHelper->publishSwitchEquitherm();
    this->haHelper->publishInputEquithermFactorN(false);
    this->haHelper->publishInputEquithermFactorK(false);
    this->haHelper->publishInputEquithermFactorT(false);

    // states
    this->haHelper->publishStatusState();
    this->haHelper->publishEmergencyState();
    this->haHelper->publishOpenthermConnectedState();
    this->haHelper->publishHeatingState();
    this->haHelper->publishFlameState();
    this->haHelper->publishFaultState();
    this->haHelper->publishDiagState();
    this->haHelper->publishExternalPumpState(false);

    // sensors
    this->haHelper->publishFaultCode();
    this->haHelper->publishDiagCode();
    this->haHelper->publishNetworkRssi(false);
    this->haHelper->publishUptime(false);

    // buttons
    this->haHelper->publishRestartButton(false);
    this->haHelper->publishResetFaultButton();
    this->haHelper->publishResetDiagButton();

    // dynamic sensors
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      if (!Sensors::hasEnabledAndValid(sensorId)) {
        continue;
      }

      auto& sSettings = Sensors::settings[sensorId];
      switch (sSettings.type) {
        case Sensors::Type::BLUETOOTH:
          this->haHelper->publishConnectionDynamicSensor(sSettings);
          this->haHelper->publishSignalQualityDynamicSensor(sSettings, false);
          this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::TEMPERATURE, settings.system.unitSystem);
          this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::HUMIDITY, settings.system.unitSystem);
          this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::BATTERY, settings.system.unitSystem);
          this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::RSSI, settings.system.unitSystem, false);
          break;

        case Sensors::Type::DALLAS_TEMP:
          this->haHelper->publishConnectionDynamicSensor(sSettings);
          this->haHelper->publishSignalQualityDynamicSensor(sSettings, false);
          this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::TEMPERATURE, settings.system.unitSystem);
          break;

        case Sensors::Type::MANUAL:
          this->client->subscribe(
            this->haHelper->getDeviceTopic(
              F("sensors"),
              Sensors::makeObjectId(sSettings.name).c_str(),
              F("set")
            ).c_str()
          );
        
        default:
          this->haHelper->publishDynamicSensor(sSettings, Sensors::ValueType::PRIMARY, settings.system.unitSystem);
      }
    }
  }

  bool publishNonStaticHaEntities(bool force = false) {
    static byte _heatingMinTemp, _heatingMaxTemp, _dhwMinTemp, _dhwMaxTemp = 0;
    static bool _indoorTempControl, _dhwPresent = false;

    bool published = false;
    if (force || _dhwPresent != settings.opentherm.dhwPresent) {
      _dhwPresent = settings.opentherm.dhwPresent;

      if (_dhwPresent) {
        this->haHelper->publishInputDhwMinTemp(settings.system.unitSystem);
        this->haHelper->publishInputDhwMaxTemp(settings.system.unitSystem);
        this->haHelper->publishDhwState();

      } else {
        this->haHelper->deleteSwitchDhw();
        this->haHelper->deleteInputDhwMinTemp();
        this->haHelper->deleteInputDhwMaxTemp();
        this->haHelper->deleteDhwState();
        this->haHelper->deleteInputDhwTarget();
        this->haHelper->deleteClimateDhw();
      }

      published = true;
    }

    if (force || _indoorTempControl != vars.master.heating.indoorTempControl || _heatingMinTemp != vars.master.heating.minTemp || _heatingMaxTemp != vars.master.heating.maxTemp) {
      _heatingMinTemp = vars.master.heating.minTemp;
      _heatingMaxTemp = vars.master.heating.maxTemp;
      _indoorTempControl = vars.master.heating.indoorTempControl;

      this->haHelper->publishClimateHeating(
        settings.system.unitSystem,
        vars.master.heating.minTemp,
        vars.master.heating.maxTemp
      );

      published = true;
    }

    if (_dhwPresent && (force || _dhwMinTemp != settings.dhw.minTemp || _dhwMaxTemp != settings.dhw.maxTemp)) {
      _dhwMinTemp = settings.dhw.minTemp;
      _dhwMaxTemp = settings.dhw.maxTemp;

      this->haHelper->publishClimateDhw(settings.system.unitSystem, settings.dhw.minTemp, settings.dhw.maxTemp);

      published = true;
    }

    return published;
  }

  bool publishSettings(const char* topic) {
    JsonDocument doc;
    safeSettingsToJson(settings, doc);
    doc.shrinkToFit();

    return this->writer->publish(topic, doc, true);
  }

  bool publishSensor(uint8_t sensorId) {
    auto& sSettings = Sensors::settings[sensorId];

    if (!Sensors::isValidSensorId(sensorId)) {
      return false;

    } else if (!strlen(sSettings.name)) {
      return false;
    }

    JsonDocument doc;
    sensorResultToJson(sensorId, doc);
    doc.shrinkToFit();

    return this->writer->publish(
      this->haHelper->getDeviceTopic(
        F("sensors"),
        Sensors::makeObjectId(sSettings.name).c_str()
      ).c_str(),
      doc,
      true
    );
  }

  bool publishVariables(const char* topic) {
    JsonDocument doc;
    varsToJson(vars, doc);
    doc.shrinkToFit();

    return this->writer->publish(topic, doc, true);
  }
};