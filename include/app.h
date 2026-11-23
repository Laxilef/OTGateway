#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaHADiscovery.h>
#include <MycilaTaskManager.h>
#include <esp_log.h>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>

#include "macros.h"
#include "types.h"
#include "strings_shared.h"
#include "Settings.h"
#include "utils.h"

using namespace Mycila;

class AsyncWebServer;
class WebSerial;

// Configuration
extern NetworkSettings networkSettings;
extern Settings settings;
extern Sensors::Settings sensorsSettings[SENSORS_AMOUNT];
void initConfig();
void saveSettingsNow();
void requestSaveNetworkSettings();
void requestSaveSettings();
// Call after releasing lifecycle, MQTT queue and Sensors data locks.
void requestSaveSensorsSettings();

JsonVariant toJson(const NetworkSettings& src, JsonVariant dst);
JsonVariant toJson(const SystemSettings& src, JsonVariant dst);
JsonVariant toJson(const PortalSettings& src, JsonVariant dst);
JsonVariant toJson(const OpenThermSettings& src, JsonVariant dst);
JsonVariant toJson(const MqttSettings& src, JsonVariant dst);
JsonVariant toJson(const EmergencySettings& src, JsonVariant dst);
JsonVariant toJson(const HeatingSettings& src, JsonVariant dst);
JsonVariant toJson(const DhwSettings& src, JsonVariant dst);
JsonVariant toJson(const PidSettings& src, JsonVariant dst);
JsonVariant toJson(const EquithermSettings& src, JsonVariant dst);
JsonVariant toJson(const ExternalPumpSettings& src, JsonVariant dst);
JsonVariant toJson(const CascadeControlSettings& src, JsonVariant dst);
SettingResult fromJson(JsonVariantConst src, NetworkSettings& dst);
SettingResult fromJson(JsonVariantConst src, SystemSettings& dst);
SettingResult fromJson(JsonVariantConst src, PortalSettings& dst);
SettingResult fromJson(JsonVariantConst src, OpenThermSettings& dst);
SettingResult fromJson(JsonVariantConst src, MqttSettings& dst);
SettingResult fromJson(JsonVariantConst src, EmergencySettings& dst);
SettingResult fromJson(JsonVariantConst src, HeatingSettings& dst);
SettingResult fromJson(JsonVariantConst src, DhwSettings& dst);
SettingResult fromJson(JsonVariantConst src, PidSettings& dst);
SettingResult fromJson(JsonVariantConst src, EquithermSettings& dst);
SettingResult fromJson(JsonVariantConst src, ExternalPumpSettings& dst);
SettingResult fromJson(JsonVariantConst src, CascadeControlSettings& dst);
SettingResult applySetting(Settings& dst, std::string_view path, bool value);
SettingResult applySetting(Settings& dst, std::string_view path, float value);
bool normalizeSettings(Settings& dst);
JsonVariant toJson(const uint8_t sensorId, const Sensors::Settings& src, JsonVariant dst);
// Caller holds Sensors::mutex while accessing shared settings and checking slug uniqueness.
SettingResult fromJson(const JsonVariantConst src, Sensors::Settings& dst);
JsonVariant toJson(const Sensors::Settings& sensor, const Sensors::Result& result, JsonVariant dst);
JsonVariant toJson(const uint8_t sensorId, JsonVariant dst);
JsonVariant toJson(const Variables& src, JsonVariant dst);
SettingResult fromJson(const JsonVariantConst src, Variables& dst);
bool applyAction(std::string_view action, bool value);

// Application state and task managers
extern Variables vars;
extern TaskManager coreTaskManager;
extern TaskManager systemTaskManager;
extern TaskManager openthermTaskManager;
extern TaskManager sensorsTaskManager;
// Schedule once with resume(delay); scheduled() also covers the delay before restart.
// Request only outside OTA and while the task is not already scheduled.
extern Task restartTask;
void initSystem();

std::string getResetReason();

// Control and regulation
void initControl();
void initRegulator();
void initLeds();

// Sensors
void initSensors();

// OpenTherm
extern Task writeRoomTemperatureTask;
extern Task writeRoomTemperatureCh2Task;
void initOpenTherm();

// Logging
extern WebSerial* webSerial;
extern Task configureLoggingTask;
bool initLogging();
void drainLoggingSerial();

// Web server and MQTT
extern AsyncWebServer* webServer;
void initPortal();
extern Task mqttConfigureTask;
// Protect MQTT client and task pointers while checking and using them.
extern std::mutex mqttMutex;
extern Task* mqttPublishSettingsTask;
extern Task* mqttPublishStateTask;
extern Task* mqttPublishSensorsTask;
void initMqtt();
void stopMqtt(bool destroy);
bool isMqttConnected();
void requestReconfigureMqtt();
void requestReconfigureMqttSensor(uint8_t sensorId, std::optional<Sensors::Settings> previousSettings = std::nullopt);
void requestPublishMqttSensor(uint8_t sensorId);
std::string mqttTopic(std::initializer_list<std::string_view> parts);
template <typename... Parts>
std::string mqttTopic(const Parts&... parts) {
  return mqttTopic({std::string_view(parts)...});
}

// Home Assistant discovery
extern HA::Discovery haDiscovery;
void publishHaEntity(HaEntity entityId, bool remove);
void publishDynamicSensor(
  const Sensors::Settings& sensor,
  Sensors::ValueType valueType = Sensors::ValueType::PRIMARY, bool enabledByDefault = true
);
void publishDynamicManualSensor(const Sensors::Settings& sensor, bool enabledByDefault = true);
void deleteDynamicSensor(const Sensors::Settings& sensor, Sensors::ValueType valueType = Sensors::ValueType::PRIMARY);
void publishConnectionDynamicSensor(const Sensors::Settings& sensor, bool enabledByDefault = true);
void deleteConnectionDynamicSensor(const Sensors::Settings& sensor);
void publishSignalQualityDynamicSensor(const Sensors::Settings& sensor, bool enabledByDefault = false);
void deleteSignalQualityDynamicSensor(const Sensors::Settings& sensor);

// Network
void initNetwork();
// Register during setup, before starting systemTaskManager.
// Callbacks run on the system worker: only signal client owners, without blocking.
void onNetworkEvent(NetworkEventCallback callback);
void onNetworkStateChange(NetworkStateCallback callback);
NetworkState getNetworkState(NetworkMode mode = NetworkMode::STA);
NetworkMode getNetworkMode();
const char* getNetworkConnectionType();
IPAddress getNetworkIp();
IPAddress getNetworkSubnet();
IPAddress getNetworkGateway();
IPAddress getNetworkDns();
String getNetworkMac();
bool isEthernetConfigured(const NetworkSettings& config = networkSettings);
bool isNetworkInterfaceStarted(NetworkMode mode);
// Last state published by the system worker, including during event callbacks.
bool isNetworkConnected();
void reconnectNetwork();
