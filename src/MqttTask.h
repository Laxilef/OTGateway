#include <WiFiClient.h>
#include <PubSubClient.h>
#include <MqttWiFiClient.h>
#include <MqttWriter.h>
#include "HaHelper.h"


class MqttTask : public Task {
public:
  MqttTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {
    this->wifiClient = new MqttWiFiClient();
    this->client = new PubSubClient();
    this->writer = new MqttWriter(this->client, 256);
    this->haHelper = new HaHelper();
  }

  ~MqttTask() {
    if (this->haHelper != nullptr) {
      delete this->haHelper;
    }

    if (this->client != nullptr) {
      if (this->client->connected()) {
        this->client->disconnect();
      }

      delete this->client;
    }

    if (this->writer != nullptr) {
      delete this->writer;
    }

    if (this->wifiClient != nullptr) {
      delete this->wifiClient;
    }
  }

protected:
  MqttWiFiClient* wifiClient = nullptr;
  PubSubClient* client = nullptr;
  HaHelper* haHelper = nullptr;
  MqttWriter* writer = nullptr;
  unsigned short readyForSendTime = 15000;
  unsigned long lastReconnectTime = 0;
  unsigned long connectedTime = 0;
  unsigned long disconnectedTime = 0;
  unsigned long prevPubVarsTime = 0;
  unsigned long prevPubSettingsTime = 0;
  bool connected = false;
  bool newConnection = false;

  const char* getTaskName() {
    return "Mqtt";
  }
  
  /*int getTaskCore() {
    return 1;
  }*/

  int getTaskPriority() {
    return 1;
  }

  bool isReadyForSend() {
    return millis() - this->connectedTime > this->readyForSendTime;
  }

  void setup() {
    Log.sinfoln("MQTT", F("Started"));

    // wificlient settings
    this->wifiClient->setSync(true);

    // client settings
    this->client->setClient(*this->wifiClient);
    this->client->setSocketTimeout(3);
    this->client->setKeepAlive(15);
    this->client->setBufferSize(768);
    this->client->setCallback([this] (char* topic, uint8_t* payload, unsigned int length) {
      this->onMessage(topic, payload, length);
    });

    // writer settings
    this->writer->setYieldCallback([this] {
      this->delay(10);
    });
    this->writer->setEventPublishCallback([this] (const char* topic, size_t written, size_t length, bool result) {
      Log.straceln("MQTT", F("%s publish %u of %u bytes to topic: %s"), result ? F("Successfully") : F("Failed"), written, length, topic);

      this->client->loop();
      this->delay(250);
    });
    this->writer->setEventFlushCallback([this] (size_t, size_t) {
      if (!this->wifiClient->getSync() && this->wifiClient->connected()) {
        this->wifiClient->flush();
      }

      #ifdef ARDUINO_ARCH_ESP8266
      ::yield();
      #endif
    });

    // ha helper settings
    this->haHelper->setDevicePrefix(settings.mqtt.prefix);
    this->haHelper->setDeviceVersion(PROJECT_VERSION);
    this->haHelper->setDeviceModel(PROJECT_NAME);
    this->haHelper->setDeviceName(PROJECT_NAME);
    this->haHelper->setWriter(this->writer);

    sprintf(buffer, CONFIG_URL, WiFi.localIP().toString().c_str());
    this->haHelper->setDeviceConfigUrl(buffer);
  }

  void loop() {
    if (!this->client->connected() && this->connected) {
      this->connected = false;
      this->onDisconnect();
    }

    if (this->wifiClient == nullptr || (!this->client->connected() && millis() - this->lastReconnectTime >= MQTT_RECONNECT_INTERVAL)) {
      Log.sinfoln("MQTT", F("Not connected, state: %d"), this->client->state());
      Log.sinfoln("MQTT", F("Connecting to %s:%u..."), settings.mqtt.server, settings.mqtt.port);

      this->client->setServer(settings.mqtt.server, settings.mqtt.port);
      this->client->connect(settings.hostname, settings.mqtt.user, settings.mqtt.password);

      this->lastReconnectTime = millis();
    }

    if (this->client->connected() && !this->connected) {
      this->connected = true;
      this->onConnect();
    }

    if (!this->client->connected()) {
      if (settings.emergency.enable && !vars.states.emergency) {
        if (millis() - this->disconnectedTime > EMERGENCY_TIME_TRESHOLD) {
          vars.states.emergency = true;
          Log.sinfoln("MQTT", F("Emergency mode enabled"));
        }
      }

      return;
    }

    this->client->loop();

    // delay for publish data
    if (!this->isReadyForSend()) {
      return;
    }

    // publish variables and status
    if (this->newConnection || millis() - this->prevPubVarsTime > settings.mqtt.interval) {
      this->writer->publish(
        this->haHelper->getDeviceTopic("status").c_str(),
        !vars.states.otStatus ? "offline" : vars.states.fault ? "fault" : "online",
        true
      );

      this->publishVariables(this->haHelper->getDeviceTopic("state").c_str());
      this->prevPubVarsTime = millis();
    }

    // publish settings
    if (this->newConnection || millis() - this->prevPubSettingsTime > settings.mqtt.interval * 10) {
      this->publishSettings(this->haHelper->getDeviceTopic("settings").c_str());
      this->prevPubSettingsTime = millis();
    }

    // publish ha entities if not published
    if (this->newConnection) {
      this->publishHaEntities();
      this->publishNonStaticHaEntities(true);
      this->newConnection = false;

    } else {
      // publish non static ha entities
      this->publishNonStaticHaEntities();
    }
  }

  void onConnect() {
    this->connectedTime = millis();
    this->newConnection = true;
    unsigned long downtime = (millis() - this->disconnectedTime) / 1000;
    Log.sinfoln("MQTT", F("Connected (downtime: %u s.)"), downtime);

    if (vars.states.emergency) {
      vars.states.emergency = false;
      Log.sinfoln("MQTT", F("Emergency mode disabled"));
    }

    this->client->subscribe(this->haHelper->getDeviceTopic("settings/set").c_str());
    this->client->subscribe(this->haHelper->getDeviceTopic("state/set").c_str());
  }

  void onDisconnect() {
    this->disconnectedTime = millis();

    unsigned long uptime = (millis() - this->connectedTime) / 1000;
    Log.swarningln("MQTT", F("Disconnected (reason: %d uptime: %u s.)"), this->client->state(), uptime);
  }

  void onMessage(char* topic, uint8_t* payload, unsigned int length) {
    if (!length) {
      return;
    }

    if (settings.debug) {
      Log.strace("MQTT.MSG", F("Topic: %s\r\n>  "), topic);
      if (Log.lock()) {
        for (size_t i = 0; i < length; i++) {
          if (payload[i] == 0) {
            break;
          } else if (payload[i] == 13) {
            continue;
          } else if (payload[i] == 10) {
            Log.print("\r\n>  ");
          } else {
            Log.print((char) payload[i]);
          }
        }
        Log.print("\r\n\n");
        Log.flush();
        Log.unlock();
      }
    }

    JsonDocument doc;
    DeserializationError dErr = deserializeJson(doc, payload, length);
    if (dErr != DeserializationError::Ok || doc.isNull()) {
      Log.swarningln("MQTT.MSG", F("Error on deserialization: %s"), dErr.f_str());
      return;
    }

    if (this->haHelper->getDeviceTopic("state/set").equals(topic)) {
      this->writer->publish(this->haHelper->getDeviceTopic("state/set").c_str(), nullptr, 0, true);
      this->updateVariables(doc);

    } else if (this->haHelper->getDeviceTopic("settings/set").equals(topic)) {
      this->writer->publish(this->haHelper->getDeviceTopic("settings/set").c_str(), nullptr, 0, true);
      this->updateSettings(doc);
    }
  }


  bool updateSettings(JsonDocument& doc) {
    bool flag = false;

    if (!doc["debug"].isNull() && doc["debug"].is<bool>()) {
      settings.debug = doc["debug"].as<bool>();
      flag = true;
    }


    // emergency
    if (!doc["emergency"]["enable"].isNull() && doc["emergency"]["enable"].is<bool>()) {
      settings.emergency.enable = doc["emergency"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["emergency"]["target"].isNull() && doc["emergency"]["target"].is<float>()) {
      if (doc["emergency"]["target"].as<float>() > 0 && doc["emergency"]["target"].as<float>() < 100) {
        settings.emergency.target = round(doc["emergency"]["target"].as<float>() * 10) / 10;
        flag = true;
      }
    }

    if (!doc["emergency"]["useEquitherm"].isNull() && doc["emergency"]["useEquitherm"].is<bool>()) {
      settings.emergency.useEquitherm = doc["emergency"]["useEquitherm"].as<bool>();
      flag = true;
    }


    // heating
    if (!doc["heating"]["enable"].isNull() && doc["heating"]["enable"].is<bool>()) {
      settings.heating.enable = doc["heating"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["heating"]["turbo"].isNull() && doc["heating"]["turbo"].is<bool>()) {
      settings.heating.turbo = doc["heating"]["turbo"].as<bool>();
      flag = true;
    }

    if (!doc["heating"]["target"].isNull() && doc["heating"]["target"].is<float>()) {
      if (doc["heating"]["target"].as<float>() > 0 && doc["heating"]["target"].as<float>() < 100) {
        settings.heating.target = round(doc["heating"]["target"].as<float>() * 10) / 10;
        flag = true;
      }
    }

    if (!doc["heating"]["hysteresis"].isNull() && doc["heating"]["hysteresis"].is<float>()) {
      if (doc["heating"]["hysteresis"].as<float>() >= 0 && doc["heating"]["hysteresis"].as<float>() <= 5) {
        settings.heating.hysteresis = round(doc["heating"]["hysteresis"].as<float>() * 10) / 10;
        flag = true;
      }
    }

    if (!doc["heating"]["maxModulation"].isNull() && doc["heating"]["maxModulation"].is<unsigned char>()) {
      if (doc["heating"]["maxModulation"].as<unsigned char>() > 0 && doc["heating"]["maxModulation"].as<unsigned char>() <= 100) {
        settings.heating.maxModulation = doc["heating"]["maxModulation"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["heating"]["maxTemp"].isNull() && doc["heating"]["maxTemp"].is<unsigned char>()) {
      if (doc["heating"]["maxTemp"].as<unsigned char>() > 0 && doc["heating"]["maxTemp"].as<unsigned char>() <= 100) {
        settings.heating.maxTemp = doc["heating"]["maxTemp"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["heating"]["minTemp"].isNull() && doc["heating"]["minTemp"].is<unsigned char>()) {
      if (doc["heating"]["minTemp"].as<unsigned char>() >= 0 && doc["heating"]["minTemp"].as<unsigned char>() < 100) {
        settings.heating.minTemp = doc["heating"]["minTemp"].as<unsigned char>();
        flag = true;
      }
    }


    // dhw
    if (!doc["dhw"]["enable"].isNull() && doc["dhw"]["enable"].is<bool>()) {
      settings.dhw.enable = doc["dhw"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["dhw"]["target"].isNull() && doc["dhw"]["target"].is<unsigned char>()) {
      if (doc["dhw"]["target"].as<unsigned char>() >= 0 && doc["dhw"]["target"].as<unsigned char>() < 100) {
        settings.dhw.target = doc["dhw"]["target"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["dhw"]["maxTemp"].isNull() && doc["dhw"]["maxTemp"].is<unsigned char>()) {
      if (doc["dhw"]["maxTemp"].as<unsigned char>() > 0 && doc["dhw"]["maxTemp"].as<unsigned char>() <= 100) {
        settings.dhw.maxTemp = doc["dhw"]["maxTemp"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["dhw"]["minTemp"].isNull() && doc["dhw"]["minTemp"].is<unsigned char>()) {
      if (doc["dhw"]["minTemp"].as<unsigned char>() >= 0 && doc["dhw"]["minTemp"].as<unsigned char>() < 100) {
        settings.dhw.minTemp = doc["dhw"]["minTemp"].as<unsigned char>();
        flag = true;
      }
    }


    // pid
    if (!doc["pid"]["enable"].isNull() && doc["pid"]["enable"].is<bool>()) {
      settings.pid.enable = doc["pid"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["pid"]["p_factor"].isNull() && doc["pid"]["p_factor"].is<float>()) {
      if (doc["pid"]["p_factor"].as<float>() > 0 && doc["pid"]["p_factor"].as<float>() <= 10) {
        settings.pid.p_factor = round(doc["pid"]["p_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["pid"]["i_factor"].isNull() && doc["pid"]["i_factor"].is<float>()) {
      if (doc["pid"]["i_factor"].as<float>() >= 0 && doc["pid"]["i_factor"].as<float>() <= 10) {
        settings.pid.i_factor = round(doc["pid"]["i_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["pid"]["d_factor"].isNull() && doc["pid"]["d_factor"].is<float>()) {
      if (doc["pid"]["d_factor"].as<float>() >= 0 && doc["pid"]["d_factor"].as<float>() <= 10) {
        settings.pid.d_factor = round(doc["pid"]["d_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["pid"]["maxTemp"].isNull() && doc["pid"]["maxTemp"].is<unsigned char>()) {
      if (doc["pid"]["maxTemp"].as<unsigned char>() > 0 && doc["pid"]["maxTemp"].as<unsigned char>() <= 100 && doc["pid"]["maxTemp"].as<unsigned char>() > settings.pid.minTemp) {
        settings.pid.maxTemp = doc["pid"]["maxTemp"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["pid"]["minTemp"].isNull() && doc["pid"]["minTemp"].is<unsigned char>()) {
      if (doc["pid"]["minTemp"].as<unsigned char>() >= 0 && doc["pid"]["minTemp"].as<unsigned char>() < 100 && doc["pid"]["minTemp"].as<unsigned char>() < settings.pid.maxTemp) {
        settings.pid.minTemp = doc["pid"]["minTemp"].as<unsigned char>();
        flag = true;
      }
    }

    // equitherm
    if (!doc["equitherm"]["enable"].isNull() && doc["equitherm"]["enable"].is<bool>()) {
      settings.equitherm.enable = doc["equitherm"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["equitherm"]["n_factor"].isNull() && doc["equitherm"]["n_factor"].is<float>()) {
      if (doc["equitherm"]["n_factor"].as<float>() > 0 && doc["equitherm"]["n_factor"].as<float>() <= 10) {
        settings.equitherm.n_factor = round(doc["equitherm"]["n_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["equitherm"]["k_factor"].isNull() && doc["equitherm"]["k_factor"].is<float>()) {
      if (doc["equitherm"]["k_factor"].as<float>() >= 0 && doc["equitherm"]["k_factor"].as<float>() <= 10) {
        settings.equitherm.k_factor = round(doc["equitherm"]["k_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["equitherm"]["t_factor"].isNull() && doc["equitherm"]["t_factor"].is<float>()) {
      if (doc["equitherm"]["t_factor"].as<float>() >= 0 && doc["equitherm"]["t_factor"].as<float>() <= 10) {
        settings.equitherm.t_factor = round(doc["equitherm"]["t_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }


    // sensors
    if (!doc["sensors"]["outdoor"]["type"].isNull() && doc["sensors"]["outdoor"]["type"].is<unsigned char>()) {
      if (doc["sensors"]["outdoor"]["type"].as<unsigned char>() >= 0 && doc["sensors"]["outdoor"]["type"].as<unsigned char>() <= 2) {
        settings.sensors.outdoor.type = doc["sensors"]["outdoor"]["type"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["sensors"]["outdoor"]["offset"].isNull() && doc["sensors"]["outdoor"]["offset"].is<float>()) {
      if (doc["sensors"]["outdoor"]["offset"].as<float>() >= -10 && doc["sensors"]["outdoor"]["offset"].as<float>() <= 10) {
        settings.sensors.outdoor.offset = round(doc["sensors"]["outdoor"]["offset"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["sensors"]["indoor"]["type"].isNull() && doc["sensors"]["indoor"]["type"].is<unsigned char>()) {
      if (doc["sensors"]["indoor"]["type"].as<unsigned char>() >= 1 && doc["sensors"]["indoor"]["type"].as<unsigned char>() <= 3) {
        settings.sensors.indoor.type = doc["sensors"]["indoor"]["type"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["sensors"]["indoor"]["offset"].isNull() && doc["sensors"]["indoor"]["offset"].is<float>()) {
      if (doc["sensors"]["indoor"]["offset"].as<float>() >= -10 && doc["sensors"]["indoor"]["offset"].as<float>() <= 10) {
        settings.sensors.indoor.offset = round(doc["sensors"]["indoor"]["offset"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    doc.clear();
    doc.shrinkToFit();

    if (flag) {
      this->prevPubSettingsTime = 0;
      eeSettings.update();
      return true;
    }

    return false;
  }

  bool updateVariables(JsonDocument& doc) {
    bool flag = false;

    if (!doc["ping"].isNull() && doc["ping"]) {
      flag = true;
    }

    if (!doc["tuning"]["enable"].isNull() && doc["tuning"]["enable"].is<bool>()) {
      vars.tuning.enable = doc["tuning"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["tuning"]["regulator"].isNull() && doc["tuning"]["regulator"].is<unsigned char>()) {
      if (doc["tuning"]["regulator"].as<unsigned char>() >= 0 && doc["tuning"]["regulator"].as<unsigned char>() <= 1) {
        vars.tuning.regulator = doc["tuning"]["regulator"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["temperatures"]["indoor"].isNull() && doc["temperatures"]["indoor"].is<float>()) {
      if (settings.sensors.indoor.type == 1 && doc["temperatures"]["indoor"].as<float>() > -100 && doc["temperatures"]["indoor"].as<float>() < 100) {
        vars.temperatures.indoor = round(doc["temperatures"]["indoor"].as<float>() * 100) / 100;
        flag = true;
      }
    }

    if (!doc["temperatures"]["outdoor"].isNull() && doc["temperatures"]["outdoor"].is<float>()) {
      if (settings.sensors.outdoor.type == 1 && doc["temperatures"]["outdoor"].as<float>() > -100 && doc["temperatures"]["outdoor"].as<float>() < 100) {
        vars.temperatures.outdoor = round(doc["temperatures"]["outdoor"].as<float>() * 100) / 100;
        flag = true;
      }
    }

    if (!doc["actions"]["restart"].isNull() && doc["actions"]["restart"].is<bool>() && doc["actions"]["restart"].as<bool>()) {
      vars.actions.restart = true;
    }

    if (!doc["actions"]["resetFault"].isNull() && doc["actions"]["resetFault"].is<bool>() && doc["actions"]["resetFault"].as<bool>()) {
      vars.actions.resetFault = true;
    }

    if (!doc["actions"]["resetDiagnostic"].isNull() && doc["actions"]["resetDiagnostic"].is<bool>() && doc["actions"]["resetDiagnostic"].as<bool>()) {
      vars.actions.resetDiagnostic = true;
    }

    doc.clear();
    doc.shrinkToFit();

    if (flag) {
      this->prevPubVarsTime = 0;
      return true;
    }

    return false;
  }

  void publishHaEntities() {
    // main
    this->haHelper->publishSelectOutdoorSensorType();
    this->haHelper->publishSelectIndoorSensorType();
    this->haHelper->publishNumberOutdoorSensorOffset(false);
    this->haHelper->publishNumberIndoorSensorOffset(false);
    this->haHelper->publishSwitchDebug(false);

    // emergency
    this->haHelper->publishSwitchEmergency();
    this->haHelper->publishNumberEmergencyTarget();
    this->haHelper->publishSwitchEmergencyUseEquitherm();

    // heating
    this->haHelper->publishSwitchHeating(false);
    this->haHelper->publishSwitchHeatingTurbo();
    this->haHelper->publishNumberHeatingHysteresis();
    this->haHelper->publishSensorHeatingSetpoint(false);
    this->haHelper->publishSensorCurrentHeatingMinTemp(false);
    this->haHelper->publishSensorCurrentHeatingMaxTemp(false);
    this->haHelper->publishNumberHeatingMinTemp(false);
    this->haHelper->publishNumberHeatingMaxTemp(false);
    this->haHelper->publishNumberHeatingMaxModulation(false);

    // pid
    this->haHelper->publishSwitchPID();
    this->haHelper->publishNumberPIDFactorP();
    this->haHelper->publishNumberPIDFactorI();
    this->haHelper->publishNumberPIDFactorD();
    this->haHelper->publishNumberPIDMinTemp(false);
    this->haHelper->publishNumberPIDMaxTemp(false);

    // equitherm
    this->haHelper->publishSwitchEquitherm();
    this->haHelper->publishNumberEquithermFactorN();
    this->haHelper->publishNumberEquithermFactorK();
    this->haHelper->publishNumberEquithermFactorT();

    // tuning
    this->haHelper->publishSwitchTuning();
    this->haHelper->publishSelectTuningRegulator();

    // states
    this->haHelper->publishBinSensorStatus();
    this->haHelper->publishBinSensorOtStatus();
    this->haHelper->publishBinSensorHeating();
    this->haHelper->publishBinSensorFlame();
    this->haHelper->publishBinSensorFault();
    this->haHelper->publishBinSensorDiagnostic();

    // sensors
    this->haHelper->publishSensorModulation(false);
    this->haHelper->publishSensorPressure(false);
    this->haHelper->publishSensorFaultCode();
    this->haHelper->publishSensorRssi(false);
    this->haHelper->publishSensorUptime(false);

    // temperatures
    this->haHelper->publishNumberIndoorTemp();
    this->haHelper->publishSensorHeatingTemp();

    // buttons
    this->haHelper->publishButtonRestart(false);
    this->haHelper->publishButtonResetFault();
    this->haHelper->publishButtonResetDiagnostic();
  }

  bool publishNonStaticHaEntities(bool force = false) {
    static byte _heatingMinTemp, _heatingMaxTemp, _dhwMinTemp, _dhwMaxTemp = 0;
    static bool _isStupidMode, _editableOutdoorTemp, _editableIndoorTemp, _dhwPresent = false;

    bool published = false;
    bool isStupidMode = !settings.pid.enable && !settings.equitherm.enable;
    byte heatingMinTemp = isStupidMode ? settings.heating.minTemp : 10;
    byte heatingMaxTemp = isStupidMode ? settings.heating.maxTemp : 30;
    bool editableOutdoorTemp = settings.sensors.outdoor.type == 1;
    bool editableIndoorTemp = settings.sensors.indoor.type == 1;

    if (force || _dhwPresent != settings.opentherm.dhwPresent) {
      _dhwPresent = settings.opentherm.dhwPresent;

      if (_dhwPresent) {
        this->haHelper->publishSwitchDhw(false);
        this->haHelper->publishSensorCurrentDhwMinTemp(false);
        this->haHelper->publishSensorCurrentDhwMaxTemp(false);
        this->haHelper->publishNumberDhwMinTemp(false);
        this->haHelper->publishNumberDhwMaxTemp(false);
        this->haHelper->publishBinSensorDhw();
        this->haHelper->publishSensorDhwTemp();
        this->haHelper->publishSensorDhwFlowRate(false);

      } else {
        this->haHelper->deleteSwitchDhw();
        this->haHelper->deleteSensorCurrentDhwMinTemp();
        this->haHelper->deleteSensorCurrentDhwMaxTemp();
        this->haHelper->deleteNumberDhwMinTemp();
        this->haHelper->deleteNumberDhwMaxTemp();
        this->haHelper->deleteBinSensorDhw();
        this->haHelper->deleteSensorDhwTemp();
        this->haHelper->deleteNumberDhwTarget();
        this->haHelper->deleteClimateDhw();
        this->haHelper->deleteSensorDhwFlowRate();
      }

      published = true;
    }

    if (force || _heatingMinTemp != heatingMinTemp || _heatingMaxTemp != heatingMaxTemp) {
      if (settings.heating.target < heatingMinTemp || settings.heating.target > heatingMaxTemp) {
        settings.heating.target = constrain(settings.heating.target, heatingMinTemp, heatingMaxTemp);
      }

      _heatingMinTemp = heatingMinTemp;
      _heatingMaxTemp = heatingMaxTemp;
      _isStupidMode = isStupidMode;

      this->haHelper->publishNumberHeatingTarget(heatingMinTemp, heatingMaxTemp, false);
      this->haHelper->publishClimateHeating(
        heatingMinTemp,
        heatingMaxTemp,
        isStupidMode ? HaHelper::TEMP_SOURCE_HEATING : HaHelper::TEMP_SOURCE_INDOOR
      );

      published = true;

    } else if (_isStupidMode != isStupidMode) {
      _isStupidMode = isStupidMode;
      this->haHelper->publishClimateHeating(
        heatingMinTemp,
        heatingMaxTemp,
        isStupidMode ? HaHelper::TEMP_SOURCE_HEATING : HaHelper::TEMP_SOURCE_INDOOR
      );

      published = true;
    }

    if (_dhwPresent && (force || _dhwMinTemp != settings.dhw.minTemp || _dhwMaxTemp != settings.dhw.maxTemp)) {
      _dhwMinTemp = settings.dhw.minTemp;
      _dhwMaxTemp = settings.dhw.maxTemp;

      this->haHelper->publishNumberDhwTarget(settings.dhw.minTemp, settings.dhw.maxTemp, false);
      this->haHelper->publishClimateDhw(settings.dhw.minTemp, settings.dhw.maxTemp);

      published = true;
    }

    if (force || _editableOutdoorTemp != editableOutdoorTemp) {
      _editableOutdoorTemp = editableOutdoorTemp;

      if (editableOutdoorTemp) {
        this->haHelper->deleteSensorOutdoorTemp();
        this->haHelper->publishNumberOutdoorTemp();
      } else {
        this->haHelper->deleteNumberOutdoorTemp();
        this->haHelper->publishSensorOutdoorTemp();
      }

      published = true;
    }

    if (force || _editableIndoorTemp != editableIndoorTemp) {
      _editableIndoorTemp = editableIndoorTemp;

      if (editableIndoorTemp) {
        this->haHelper->deleteSensorIndoorTemp();
        this->haHelper->publishNumberIndoorTemp();
      } else {
        this->haHelper->deleteNumberIndoorTemp();
        this->haHelper->publishSensorIndoorTemp();
      }

      published = true;
    }

    return published;
  }

  bool publishSettings(const char* topic) {
    JsonDocument doc;
    doc["debug"] = settings.debug;

    doc["emergency"]["enable"] = settings.emergency.enable;
    doc["emergency"]["target"] = settings.emergency.target;
    doc["emergency"]["useEquitherm"] = settings.emergency.useEquitherm;

    doc["heating"]["enable"] = settings.heating.enable;
    doc["heating"]["turbo"] = settings.heating.turbo;
    doc["heating"]["target"] = settings.heating.target;
    doc["heating"]["hysteresis"] = settings.heating.hysteresis;
    doc["heating"]["minTemp"] = settings.heating.minTemp;
    doc["heating"]["maxTemp"] = settings.heating.maxTemp;
    doc["heating"]["maxModulation"] = settings.heating.maxModulation;

    doc["dhw"]["enable"] = settings.dhw.enable;
    doc["dhw"]["target"] = settings.dhw.target;
    doc["dhw"]["minTemp"] = settings.dhw.minTemp;
    doc["dhw"]["maxTemp"] = settings.dhw.maxTemp;

    doc["pid"]["enable"] = settings.pid.enable;
    doc["pid"]["p_factor"] = settings.pid.p_factor;
    doc["pid"]["i_factor"] = settings.pid.i_factor;
    doc["pid"]["d_factor"] = settings.pid.d_factor;
    doc["pid"]["minTemp"] = settings.pid.minTemp;
    doc["pid"]["maxTemp"] = settings.pid.maxTemp;

    doc["equitherm"]["enable"] = settings.equitherm.enable;
    doc["equitherm"]["n_factor"] = settings.equitherm.n_factor;
    doc["equitherm"]["k_factor"] = settings.equitherm.k_factor;
    doc["equitherm"]["t_factor"] = settings.equitherm.t_factor;

    doc["sensors"]["outdoor"]["type"] = settings.sensors.outdoor.type;
    doc["sensors"]["outdoor"]["offset"] = settings.sensors.outdoor.offset;

    doc["sensors"]["indoor"]["type"] = settings.sensors.indoor.type;
    doc["sensors"]["indoor"]["offset"] = settings.sensors.indoor.offset;

    doc.shrinkToFit();

    return this->writer->publish(topic, doc, true);
  }

  bool publishVariables(const char* topic) {
    JsonDocument doc;

    doc["tuning"]["enable"] = vars.tuning.enable;
    doc["tuning"]["regulator"] = vars.tuning.regulator;

    doc["states"]["otStatus"] = vars.states.otStatus;
    doc["states"]["heating"] = vars.states.heating;
    doc["states"]["dhw"] = vars.states.dhw;
    doc["states"]["flame"] = vars.states.flame;
    doc["states"]["fault"] = vars.states.fault;
    doc["states"]["diagnostic"] = vars.states.diagnostic;

    doc["sensors"]["modulation"] = vars.sensors.modulation;
    doc["sensors"]["pressure"] = vars.sensors.pressure;
    doc["sensors"]["dhwFlowRate"] = vars.sensors.dhwFlowRate;
    doc["sensors"]["faultCode"] = vars.sensors.faultCode;
    doc["sensors"]["rssi"] = vars.sensors.rssi;
    doc["sensors"]["uptime"] = (unsigned long) (millis() / 1000);

    doc["temperatures"]["indoor"] = vars.temperatures.indoor;
    doc["temperatures"]["outdoor"] = vars.temperatures.outdoor;
    doc["temperatures"]["heating"] = vars.temperatures.heating;
    doc["temperatures"]["dhw"] = vars.temperatures.dhw;

    doc["parameters"]["heatingEnabled"] = vars.parameters.heatingEnabled;
    doc["parameters"]["heatingMinTemp"] = vars.parameters.heatingMinTemp;
    doc["parameters"]["heatingMaxTemp"] = vars.parameters.heatingMaxTemp;
    doc["parameters"]["heatingSetpoint"] = vars.parameters.heatingSetpoint;
    doc["parameters"]["dhwMinTemp"] = vars.parameters.dhwMinTemp;
    doc["parameters"]["dhwMaxTemp"] = vars.parameters.dhwMaxTemp;

    doc.shrinkToFit();

    return this->writer->publish(topic, doc, true);
  }
};