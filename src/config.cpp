#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>

#include <FileData.h>
#include <LittleFS.h>
#include <cmath>
#include <string>
#include <string_view>

NetworkSettings networkSettings;
Settings settings;
Sensors::Settings sensorsSettings[SENSORS_AMOUNT] = {
  {
    false,
    "Outdoor temp",
    "outdoor_temp",
    Sensors::Purpose::OUTDOOR_TEMP,
    Sensors::Type::OT_OUTDOOR_TEMP,
    DEFAULT_SENSOR_OUTDOOR_GPIO
  },
  {
    false,
    "Indoor temp",
    "indoor_temp",
    Sensors::Purpose::INDOOR_TEMP,
    Sensors::Type::DALLAS_TEMP,
    DEFAULT_SENSOR_INDOOR_GPIO
  },
  {
    true,
    "Heating temp",
    "heating_temp",
    Sensors::Purpose::HEATING_TEMP,
    Sensors::Type::OT_HEATING_TEMP,
  },
  {
    true,
    "Heating return temp",
    "heating_return_temp",
    Sensors::Purpose::HEATING_RETURN_TEMP,
    Sensors::Type::OT_HEATING_RETURN_TEMP,
  },
  {
    true,
    "Heating setpoint temp",
    "heating_setpoint_temp",
    Sensors::Purpose::TEMPERATURE,
    Sensors::Type::HEATING_SETPOINT_TEMP,
  },
  {
    true,
    "DHW temp",
    "dhw_temp",
    Sensors::Purpose::DHW_TEMP,
    Sensors::Type::OT_DHW_TEMP,
  },
  {
    true,
    "DHW flow rate",
    "dhw_flow_rate",
    Sensors::Purpose::DHW_FLOW_RATE,
    Sensors::Type::OT_DHW_FLOW_RATE,
  },
  {
    true,
    "Exhaust temp",
    "exhaust_temp",
    Sensors::Purpose::EXHAUST_TEMP,
    Sensors::Type::OT_EXHAUST_TEMP,
  },
  {
    true,
    "Pressure",
    "pressure",
    Sensors::Purpose::PRESSURE,
    Sensors::Type::OT_PRESSURE,
  },
  {
    true,
    "Modulation level",
    "modulation_level",
    Sensors::Purpose::MODULATION_LEVEL,
    Sensors::Type::OT_MODULATION_LEVEL,
  },
  {
    true,
    "Power",
    "power",
    Sensors::Purpose::POWER,
    Sensors::Type::OT_CURRENT_POWER,
  }
};

static FileData fsNetworkSettings(&LittleFS, "/network.conf", 'n', &networkSettings, sizeof(networkSettings), 1000);
static FileData fsSettings(&LittleFS, "/settings.conf", 's', &settings, sizeof(settings), 60000);
static FileData fsSensorsSettings(&LittleFS, "/sensors.conf", 'e', &sensorsSettings, sizeof(sensorsSettings), 60000);

// FileData requests can arrive while the save task is running.
static std::mutex settingsSaveMutex;

void requestSaveNetworkSettings() {
  std::lock_guard<std::mutex> lock(settingsSaveMutex);
  fsNetworkSettings.update();
}

void requestSaveSettings() {
  std::lock_guard<std::mutex> lock(settingsSaveMutex);
  fsSettings.update();
}

void requestSaveSensorsSettings() {
  std::lock_guard<std::mutex> lock(Sensors::mutex);
  fsSensorsSettings.update();
}

static Task settingsSaveTask("Settings save", [](void*) {
  FDstat_t status;
  {
    std::lock_guard<std::mutex> lock(settingsSaveMutex);
    status = fsNetworkSettings.tick();
  }
  if (status == FD_WRITE) {
    ESP_LOGI(L_NETWORK_SETTINGS, "Updated");
  }

  {
    std::lock_guard<std::mutex> lock(settingsSaveMutex);
    status = fsSettings.tick();
  }
  if (status == FD_WRITE) {
    ESP_LOGI(L_SETTINGS, "Updated");
  }

  {
    std::lock_guard<std::mutex> lock(Sensors::mutex);
    status = fsSensorsSettings.tick();
  }
  if (status == FD_WRITE) {
    ESP_LOGI(L_SENSORS_SETTINGS, "Updated");
  }
});

void saveSettingsNow() {
  {
    std::lock_guard<std::mutex> lock(settingsSaveMutex);
    fsSettings.updateNow();
  }

  {
    std::lock_guard<std::mutex> lock(Sensors::mutex);
    fsSensorsSettings.updateNow();
  }

  // Always preserve the network configuration
  {
    std::lock_guard<std::mutex> lock(settingsSaveMutex);
    if (fsNetworkSettings.updateNow() == FD_FILE_ERR && LittleFS.begin()) {
      fsNetworkSettings.write();
    }
  }
}

// Initialization
static bool loadNetworkSettings() {
  bool loaded = false;

  switch (fsNetworkSettings.read()) {
    case FD_FS_ERR:
      ESP_LOGW(L_NETWORK_SETTINGS, "Filesystem error, load default");
      break;

    case FD_FILE_ERR:
      ESP_LOGW(L_NETWORK_SETTINGS, "Bad data, load default");
      break;

    case FD_WRITE:
      ESP_LOGI(L_NETWORK_SETTINGS, "Not found, load default");
      break;

    case FD_ADD:
    case FD_READ:
      ESP_LOGI(L_NETWORK_SETTINGS, "Loaded");
      loaded = true;
      break;

    default:
      break;
  }

  if (!networkSettings.hostname[0]) {
    strcpy(networkSettings.hostname, getChipId("otgateway-").c_str());
    requestSaveNetworkSettings();
  }

  return loaded;
}

static bool loadSettings() {
  bool loaded = false;

  switch (fsSettings.read()) {
    case FD_FS_ERR:
      ESP_LOGW(L_SETTINGS, "Filesystem error, load default");
      break;

    case FD_FILE_ERR:
      ESP_LOGW(L_SETTINGS, "Bad data, load default");
      break;

    case FD_WRITE:
      ESP_LOGI(L_SETTINGS, "Not found, load default");
      break;

    case FD_ADD:
    case FD_READ:
      if (memcmp(settings.validationValue, SETTINGS_VALID_VALUE, sizeof(settings.validationValue)) == 0) {
        ESP_LOGI(L_SETTINGS, "Loaded");
        loaded = true;

      } else {
        ESP_LOGW(L_SETTINGS, "Loaded, but not valid, reset to default");
        settings = Settings{};
        fsSettings.reset();
      }
      break;

    default:
      break;
  }

  if (settings.system.logLevel < ESP_LOG_NONE || settings.system.logLevel > ESP_LOG_VERBOSE) {
    settings.system.logLevel = static_cast<esp_log_level_t>(DEFAULT_LOG_LEVEL);
    requestSaveSettings();
  }

  // generate mqtt prefix if it is empty
  if (!strlen(settings.mqtt.prefix)) {
    strcpy(settings.mqtt.prefix, getChipId("otgateway_").c_str());
    requestSaveSettings();
  }

  return loaded;
}

static bool loadSensorsSettings() {
  bool loaded = false;

  switch (fsSensorsSettings.read()) {
    case FD_FS_ERR:
      ESP_LOGW(L_SENSORS, "Filesystem error, load default");
      break;

    case FD_FILE_ERR:
      ESP_LOGW(L_SENSORS, "Bad data, load default");
      break;

    case FD_WRITE:
      ESP_LOGI(L_SENSORS, "Not found, load default");
      break;

    case FD_ADD:
    case FD_READ:
      ESP_LOGI(L_SENSORS, "Loaded");
      loaded = true;
      break;

    default:
      break;
  }

  return loaded;
}

void initConfig() {
  loadNetworkSettings();
  loadSettings();
  loadSensorsSettings();
  configureLoggingTask.forceRun();

  settingsSaveTask.setInterval(100);
  coreTaskManager.addTask(settingsSaveTask);
}

// JSON API

// Shared validation

static bool isSettingInteger(float value, float minValue, float maxValue) {
  return std::isfinite(value) && value >= minValue && value <= maxValue && std::trunc(value) == value;
}

// Network settings

static bool isHostnameValid(const char* value) {
  if (value == nullptr) {
    return false;
  }

  size_t len = strlen(value);
  if (len == 0 || len > 24 || value[0] == '-' || value[len - 1] == '-') {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    const char symbol = value[i];

    // Lowercase ASCII letters.
    if (symbol >= 'a' && symbol <= 'z') {
      continue;
    }

    // Uppercase ASCII letters.
    if (symbol >= 'A' && symbol <= 'Z') {
      continue;
    }

    // Decimal digits.
    if (symbol >= '0' && symbol <= '9') {
      continue;
    }

    // Hyphens are allowed inside the hostname.
    if (symbol == '-') {
      continue;
    }

    // All other characters are invalid.
    return false;
  }

  return true;
}

JsonVariant toJson(const NetworkSettings& src, JsonVariant dst) {
  dst[S_HOSTNAME] = src.hostname;

  dst[S_USE_DHCP] = src.useDhcp;
  dst[S_STATIC_CONFIG][S_IP] = src.staticConfig.ip;
  dst[S_STATIC_CONFIG][S_GATEWAY] = src.staticConfig.gateway;
  dst[S_STATIC_CONFIG][S_SUBNET] = src.staticConfig.subnet;
  dst[S_STATIC_CONFIG][S_DNS] = src.staticConfig.dns;

  dst[S_AP][S_SSID] = src.ap.ssid;
  dst[S_AP][S_PASSWORD] = src.ap.password;
  dst[S_AP][S_CHANNEL] = src.ap.channel;

  dst[S_STA][S_SSID] = src.sta.ssid;
  dst[S_STA][S_PASSWORD] = src.sta.password;
  dst[S_STA][S_CHANNEL] = src.sta.channel;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, NetworkSettings& dst) {
  SettingResult result;

  // hostname
  if (src[S_HOSTNAME].is<JsonString>()) {
    const JsonString value = src[S_HOSTNAME].as<JsonString>();

    if (value.size() < sizeof(dst.hostname) && isHostnameValid(value.c_str())) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.hostname) != 0) {
        strcpy(dst.hostname, value.c_str());
        result.changed = true;
      }
    }
  }

  // use dhcp
  if (src[S_USE_DHCP].is<bool>()) {
    const auto value = src[S_USE_DHCP].as<bool>();
    result.accepted = true;

    if (value != dst.useDhcp) {
      dst.useDhcp = value;
      result.changed = true;
    }
  }


  // static config
  if (src[S_STATIC_CONFIG][S_IP].is<JsonString>()) {
    const JsonString value = src[S_STATIC_CONFIG][S_IP].as<JsonString>();

    if (value.size() < sizeof(dst.staticConfig.ip)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.staticConfig.ip) != 0) {
        strcpy(dst.staticConfig.ip, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_STATIC_CONFIG][S_GATEWAY].is<JsonString>()) {
    const JsonString value = src[S_STATIC_CONFIG][S_GATEWAY].as<JsonString>();

    if (value.size() < sizeof(dst.staticConfig.gateway)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.staticConfig.gateway) != 0) {
        strcpy(dst.staticConfig.gateway, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_STATIC_CONFIG][S_SUBNET].is<JsonString>()) {
    const JsonString value = src[S_STATIC_CONFIG][S_SUBNET].as<JsonString>();

    if (value.size() < sizeof(dst.staticConfig.subnet)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.staticConfig.subnet) != 0) {
        strcpy(dst.staticConfig.subnet, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_STATIC_CONFIG][S_DNS].is<JsonString>()) {
    const JsonString value = src[S_STATIC_CONFIG][S_DNS].as<JsonString>();

    if (value.size() < sizeof(dst.staticConfig.dns)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.staticConfig.dns) != 0) {
        strcpy(dst.staticConfig.dns, value.c_str());
        result.changed = true;
      }
    }
  }


  // ap
  if (src[S_AP][S_SSID].is<JsonString>()) {
    const JsonString value = src[S_AP][S_SSID].as<JsonString>();

    if (value.size() && value.size() < sizeof(dst.ap.ssid)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.ap.ssid) != 0) {
        strcpy(dst.ap.ssid, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_AP][S_PASSWORD].is<JsonString>()) {
    const JsonString value = src[S_AP][S_PASSWORD].as<JsonString>();

    if (value.size() && value.size() < sizeof(dst.ap.password)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.ap.password) != 0) {
        strcpy(dst.ap.password, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_AP][S_CHANNEL].is<uint8_t>()) {
    const auto value = src[S_AP][S_CHANNEL].as<uint8_t>();

    if (value < 12) {
      result.accepted = true;

      if (value != dst.ap.channel) {
        dst.ap.channel = value;
        result.changed = true;
      }
    }
  }


  // sta
  if (src[S_STA][S_SSID].is<JsonString>()) {
    const JsonString value = src[S_STA][S_SSID].as<JsonString>();

    if (value.size() && value.size() < sizeof(dst.sta.ssid)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.sta.ssid) != 0) {
        strcpy(dst.sta.ssid, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_STA][S_PASSWORD].is<JsonString>()) {
    const JsonString value = src[S_STA][S_PASSWORD].as<JsonString>();

    if (value.size() < sizeof(dst.sta.password)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.sta.password) != 0) {
        strcpy(dst.sta.password, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_STA][S_CHANNEL].is<uint8_t>()) {
    const auto value = src[S_STA][S_CHANNEL].as<uint8_t>();

    if (value < 12) {
      result.accepted = true;

      if (value != dst.sta.channel) {
        dst.sta.channel = value;
        result.changed = true;
      }
    }
  }

  return result;
}

// System settings

JsonVariant toJson(const SystemSettings& src, JsonVariant dst) {
  auto system = dst.to<JsonObject>();
  system[S_LOG_LEVEL] = static_cast<uint8_t>(src.logLevel);

  auto serial = system[S_SERIAL].to<JsonObject>();
  serial[S_ENABLED] = src.serial.enabled;
  serial[S_BAUDRATE] = src.serial.baudrate;

  auto webSerial = system[S_WEBSERIAL].to<JsonObject>();
  webSerial[S_ENABLED] = src.webSerial.enabled;

  auto ntp = system[S_NTP].to<JsonObject>();
  ntp[S_SERVER] = src.ntp.server;
  ntp[S_TIMEZONE] = src.ntp.timezone;

  system[S_UNIT_SYSTEM] = static_cast<uint8_t>(src.unitSystem);
  system[S_STATUS_LED_GPIO] = src.statusLedGpio;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, SystemSettings& dst) {
  SettingResult result;

  if (src[S_LOG_LEVEL].is<int>()) {
    const auto value = src[S_LOG_LEVEL].as<int>();

    if (value >= ESP_LOG_NONE && value <= ESP_LOG_VERBOSE) {
      result.accepted = true;
      if (value != dst.logLevel) {
        dst.logLevel = static_cast<esp_log_level_t>(value);
        result.changed = true;
      }
    }
  }

  if (src[S_SERIAL][S_ENABLED].is<bool>()) {
    const auto value = src[S_SERIAL][S_ENABLED].as<bool>();

    result.accepted = true;
    if (value != dst.serial.enabled) {
      dst.serial.enabled = value;
      result.changed = true;
    }
  }

  if (src[S_SERIAL][S_BAUDRATE].is<unsigned int>()) {
    const auto value = src[S_SERIAL][S_BAUDRATE].as<unsigned int>();

    if (value == 9600 || value == 19200 || value == 38400 || value == 57600 || value == 74880 || value == 115200) {
      result.accepted = true;
      if (value != dst.serial.baudrate) {
        dst.serial.baudrate = value;
        result.changed = true;
      }
    }
  }

  if (src[S_WEBSERIAL][S_ENABLED].is<bool>()) {
    const auto value = src[S_WEBSERIAL][S_ENABLED].as<bool>();

    result.accepted = true;
    if (value != dst.webSerial.enabled) {
      dst.webSerial.enabled = value;
      result.changed = true;
    }
  }

  if (src[S_NTP][S_SERVER].is<JsonString>()) {
    const JsonString value = src[S_NTP][S_SERVER].as<JsonString>();

    if (value.size() < sizeof(dst.ntp.server)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.ntp.server) != 0) {
        strcpy(dst.ntp.server, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_NTP][S_TIMEZONE].is<JsonString>()) {
    const JsonString value = src[S_NTP][S_TIMEZONE].as<JsonString>();

    if (value.size() < sizeof(dst.ntp.timezone)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.ntp.timezone) != 0) {
        strcpy(dst.ntp.timezone, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_UNIT_SYSTEM].is<uint8_t>()) {
    const auto value = src[S_UNIT_SYSTEM].as<uint8_t>();

    switch (value) {
      case static_cast<uint8_t>(UnitSystem::METRIC):
        result.accepted = true;
        if (dst.unitSystem != UnitSystem::METRIC) {
          dst.unitSystem = UnitSystem::METRIC;
          result.changed = true;
        }
        break;

      case static_cast<uint8_t>(UnitSystem::IMPERIAL):
        result.accepted = true;
        if (dst.unitSystem != UnitSystem::IMPERIAL) {
          dst.unitSystem = UnitSystem::IMPERIAL;
          result.changed = true;
        }
        break;

      default:
        break;
    }

  }

  if (!src[S_STATUS_LED_GPIO].isNull()) {
    if (src[S_STATUS_LED_GPIO].is<JsonString>() && src[S_STATUS_LED_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.statusLedGpio != GPIO_IS_NOT_CONFIGURED) {
        dst.statusLedGpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_STATUS_LED_GPIO].is<uint8_t>()) {
      const auto value = src[S_STATUS_LED_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED || GPIO_IS_VALID_OUTPUT(value)) {
        result.accepted = true;
        if (value != dst.statusLedGpio) {
          dst.statusLedGpio = value;
          result.changed = true;
        }
      }
    }
  }

  return result;
}

// Portal settings

JsonVariant toJson(const PortalSettings& src, JsonVariant dst) {
  auto portal = dst.to<JsonObject>();
  portal[S_AUTH] = src.auth;
  portal[S_LOGIN] = src.login;
  portal[S_PASSWORD] = src.password;
  portal[S_MDNS] = src.mdns;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, PortalSettings& dst) {
  SettingResult result;

  if (src[S_AUTH].is<bool>()) {
    const auto value = src[S_AUTH].as<bool>();

    result.accepted = true;
    if (value != dst.auth) {
      dst.auth = value;
      result.changed = true;
    }
  }

  if (src[S_LOGIN].is<JsonString>()) {
    const JsonString value = src[S_LOGIN].as<JsonString>();

    if (value.size() && value.size() < sizeof(dst.login)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.login) != 0) {
        strcpy(dst.login, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_PASSWORD].is<JsonString>()) {
    const JsonString value = src[S_PASSWORD].as<JsonString>();

    if (value.size() && value.size() < sizeof(dst.password)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.password) != 0) {
        strcpy(dst.password, value.c_str());
        result.changed = true;
      }
    }
  }

  if (dst.auth && (!strlen(dst.login) || !strlen(dst.password))) {
    dst.auth = false;
    result.changed = true;
  }

  if (src[S_MDNS].is<bool>()) {
    const auto value = src[S_MDNS].as<bool>();

    result.accepted = true;
    if (value != dst.mdns) {
      dst.mdns = value;
      result.changed = true;
    }
  }

  return result;
}

// OpenTherm settings

static SettingResult jsonToChannelDisablingAction(const JsonVariantConst src, ChannelDisablingAction& dst) {
  if (!src.is<uint8_t>()) {
    return {};
  }

  const uint8_t value = src.as<uint8_t>();
  switch (value) {
    case static_cast<uint8_t>(ChannelDisablingAction::OFF):
    case static_cast<uint8_t>(ChannelDisablingAction::ZERO_SETPOINT):
    case static_cast<uint8_t>(ChannelDisablingAction::OFF_AND_ZERO_SETPOINT):
      if (static_cast<uint8_t>(dst) != value) {
        dst = static_cast<ChannelDisablingAction>(value);
        return {.changed = true, .accepted = true};
      }
      return {.accepted = true};

    default:
      break;
  }

  return {};
}

JsonVariant toJson(const OpenThermSettings& src, JsonVariant dst) {
  auto opentherm = dst.to<JsonObject>();
  opentherm[S_IN_GPIO] = src.inGpio;
  opentherm[S_OUT_GPIO] = src.outGpio;
  opentherm[S_RX_LED_GPIO] = src.rxLedGpio;
  opentherm[S_MEMBER_ID] = src.memberId;
  opentherm[S_FLAGS] = src.flags;
  opentherm[S_HEATING_DISABLING_ACTION] = static_cast<uint8_t>(src.heatingDisablingAction);
  opentherm[S_DHW_DISABLING_ACTION] = static_cast<uint8_t>(src.dhwDisablingAction);
  opentherm[S_CH2_DISABLING_ACTION] = static_cast<uint8_t>(src.ch2DisablingAction);
  opentherm[S_MIN_POWER] = roundf(src.minPower, 2);
  opentherm[S_MAX_POWER] = roundf(src.maxPower, 2);

  auto otOptions = opentherm[S_OPTIONS].to<JsonObject>();
  otOptions[S_DHW_SUPPORT] = src.options.dhwSupport;
  otOptions[S_COOLING_SUPPORT] = src.options.coolingSupport;
  otOptions[S_SUMMER_WINTER_MODE] = src.options.summerWinterMode;
  otOptions[S_HEATING_STATE_TO_SUMMER_WINTER_MODE] = src.options.heatingStateToSummerWinterMode;
  otOptions[S_CH2_ALWAYS_ENABLED] = src.options.ch2AlwaysEnabled;
  otOptions[S_HEATING_TO_CH2] = src.options.heatingToCh2;
  otOptions[S_DHW_TO_CH2] = src.options.dhwToCh2;
  otOptions[S_DHW_BLOCKING] = src.options.dhwBlocking;
  otOptions[S_DHW_STATE_AS_DHW_BLOCKING] = src.options.dhwStateAsDhwBlocking;
  otOptions[S_MAX_TEMP_SYNC_WITH_TARGET_TEMP] = src.options.maxTempSyncWithTargetTemp;
  otOptions[S_GET_MIN_MAX_TEMP] = src.options.getMinMaxTemp;
  otOptions[S_IGNORE_DIAG_STATE] = src.options.ignoreDiagState;
  otOptions[S_AUTO_FAULT_RESET] = src.options.autoFaultReset;
  otOptions[S_AUTO_DIAG_RESET] = src.options.autoDiagReset;
  otOptions[S_SET_DATE_AND_TIME] = src.options.setDateAndTime;
  otOptions[S_ALWAYS_SEND_INDOOR_TEMP] = src.options.alwaysSendIndoorTemp;
  otOptions[S_NATIVE_OTC] = src.options.nativeOTC;
  otOptions[S_IMMERGAS_FIX] = src.options.immergasFix;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, OpenThermSettings& dst) {
  SettingResult result;

  if (!src[S_IN_GPIO].isNull()) {
    if (src[S_IN_GPIO].is<JsonString>() && src[S_IN_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.inGpio != GPIO_IS_NOT_CONFIGURED) {
        dst.inGpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_IN_GPIO].is<uint8_t>()) {
      const auto value = src[S_IN_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED || GPIO_IS_VALID_INPUT(value)) {
        result.accepted = true;
        if (value != dst.inGpio) {
          dst.inGpio = value;
          result.changed = true;
        }
      }
    }
  }

  if (!src[S_OUT_GPIO].isNull()) {
    if (src[S_OUT_GPIO].is<JsonString>() && src[S_OUT_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.outGpio != GPIO_IS_NOT_CONFIGURED) {
        dst.outGpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_OUT_GPIO].is<uint8_t>()) {
      const auto value = src[S_OUT_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED || GPIO_IS_VALID_OUTPUT(value)) {
        result.accepted = true;
        if (value != dst.outGpio) {
          dst.outGpio = value;
          result.changed = true;
        }
      }
    }
  }

  if (!src[S_RX_LED_GPIO].isNull()) {
    if (src[S_RX_LED_GPIO].is<JsonString>() && src[S_RX_LED_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.rxLedGpio != GPIO_IS_NOT_CONFIGURED) {
        dst.rxLedGpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_RX_LED_GPIO].is<uint8_t>()) {
      const auto value = src[S_RX_LED_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED || GPIO_IS_VALID_OUTPUT(value)) {
        result.accepted = true;
        if (value != dst.rxLedGpio) {
          dst.rxLedGpio = value;
          result.changed = true;
        }
      }
    }
  }

  if (src[S_MEMBER_ID].is<uint8_t>()) {
    const auto value = src[S_MEMBER_ID].as<uint8_t>();

    result.accepted = true;
    if (value != dst.memberId) {
      dst.memberId = value;
      result.changed = true;
    }
  }

  if (src[S_FLAGS].is<uint8_t>()) {
    const auto value = src[S_FLAGS].as<uint8_t>();

    result.accepted = true;
    if (value != dst.flags) {
      dst.flags = value;
      result.changed = true;
    }
  }

  const auto heatingDisablingActionResult = jsonToChannelDisablingAction(src[S_HEATING_DISABLING_ACTION], dst.heatingDisablingAction);
  result.changed = heatingDisablingActionResult.changed || result.changed;
  result.accepted = heatingDisablingActionResult.accepted || result.accepted;

  const auto dhwDisablingActionResult = jsonToChannelDisablingAction(src[S_DHW_DISABLING_ACTION], dst.dhwDisablingAction);
  result.changed = dhwDisablingActionResult.changed || result.changed;
  result.accepted = dhwDisablingActionResult.accepted || result.accepted;

  const auto ch2DisablingActionResult = jsonToChannelDisablingAction(src[S_CH2_DISABLING_ACTION], dst.ch2DisablingAction);
  result.changed = ch2DisablingActionResult.changed || result.changed;
  result.accepted = ch2DisablingActionResult.accepted || result.accepted;

  if (src[S_MIN_POWER].is<float>()) {
    const auto value = src[S_MIN_POWER].as<float>();

    if (value >= 0 && value <= 1000) {
      result.accepted = true;
      if (isFloatDifferent(value, dst.minPower, 0.0001f)) {
        dst.minPower = roundf(value, 2);
        result.changed = true;
      }
    }
  }

  if (src[S_MAX_POWER].is<float>()) {
    const auto value = src[S_MAX_POWER].as<float>();

    if (value >= 0 && value <= 1000) {
      result.accepted = true;
      if (isFloatDifferent(value, dst.maxPower, 0.0001f)) {
        dst.maxPower = roundf(value, 2);
        result.changed = true;
      }
    }
  }

  if (src[S_OPTIONS][S_DHW_SUPPORT].is<bool>()) {
    const auto value = src[S_OPTIONS][S_DHW_SUPPORT].as<bool>();

    result.accepted = true;
    if (value != dst.options.dhwSupport) {
      dst.options.dhwSupport = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_COOLING_SUPPORT].is<bool>()) {
    const auto value = src[S_OPTIONS][S_COOLING_SUPPORT].as<bool>();

    result.accepted = true;
    if (value != dst.options.coolingSupport) {
      dst.options.coolingSupport = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_SUMMER_WINTER_MODE].is<bool>()) {
    const auto value = src[S_OPTIONS][S_SUMMER_WINTER_MODE].as<bool>();

    result.accepted = true;
    if (value != dst.options.summerWinterMode) {
      dst.options.summerWinterMode = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_HEATING_STATE_TO_SUMMER_WINTER_MODE].is<bool>()) {
    const auto value = src[S_OPTIONS][S_HEATING_STATE_TO_SUMMER_WINTER_MODE].as<bool>();

    result.accepted = true;
    if (value != dst.options.heatingStateToSummerWinterMode) {
      dst.options.heatingStateToSummerWinterMode = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_CH2_ALWAYS_ENABLED].is<bool>()) {
    const auto value = src[S_OPTIONS][S_CH2_ALWAYS_ENABLED].as<bool>();

    result.accepted = true;
    if (value != dst.options.ch2AlwaysEnabled) {
      dst.options.ch2AlwaysEnabled = value;

      if (dst.options.ch2AlwaysEnabled) {
        dst.options.heatingToCh2 = false;
        dst.options.dhwToCh2 = false;
      }

      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_HEATING_TO_CH2].is<bool>()) {
    const auto value = src[S_OPTIONS][S_HEATING_TO_CH2].as<bool>();

    result.accepted = true;
    if (value != dst.options.heatingToCh2) {
      dst.options.heatingToCh2 = value;

      if (dst.options.heatingToCh2) {
        dst.options.ch2AlwaysEnabled = false;
        dst.options.dhwToCh2 = false;
      }

      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_DHW_TO_CH2].is<bool>()) {
    const auto value = src[S_OPTIONS][S_DHW_TO_CH2].as<bool>();

    result.accepted = true;
    if (value != dst.options.dhwToCh2) {
      dst.options.dhwToCh2 = value;

      if (dst.options.dhwToCh2) {
        dst.options.ch2AlwaysEnabled = false;
        dst.options.heatingToCh2 = false;
      }

      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_DHW_BLOCKING].is<bool>()) {
    const auto value = src[S_OPTIONS][S_DHW_BLOCKING].as<bool>();

    result.accepted = true;
    if (value != dst.options.dhwBlocking) {
      dst.options.dhwBlocking = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_DHW_STATE_AS_DHW_BLOCKING].is<bool>()) {
    const auto value = src[S_OPTIONS][S_DHW_STATE_AS_DHW_BLOCKING].as<bool>();

    result.accepted = true;
    if (value != dst.options.dhwStateAsDhwBlocking) {
      dst.options.dhwStateAsDhwBlocking = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_MAX_TEMP_SYNC_WITH_TARGET_TEMP].is<bool>()) {
    const auto value = src[S_OPTIONS][S_MAX_TEMP_SYNC_WITH_TARGET_TEMP].as<bool>();

    result.accepted = true;
    if (value != dst.options.maxTempSyncWithTargetTemp) {
      dst.options.maxTempSyncWithTargetTemp = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_GET_MIN_MAX_TEMP].is<bool>()) {
    const auto value = src[S_OPTIONS][S_GET_MIN_MAX_TEMP].as<bool>();

    result.accepted = true;
    if (value != dst.options.getMinMaxTemp) {
      dst.options.getMinMaxTemp = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_IGNORE_DIAG_STATE].is<bool>()) {
    const auto value = src[S_OPTIONS][S_IGNORE_DIAG_STATE].as<bool>();

    result.accepted = true;
    if (value != dst.options.ignoreDiagState) {
      dst.options.ignoreDiagState = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_AUTO_FAULT_RESET].is<bool>()) {
    const auto value = src[S_OPTIONS][S_AUTO_FAULT_RESET].as<bool>();

    result.accepted = true;
    if (value != dst.options.autoFaultReset) {
      dst.options.autoFaultReset = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_AUTO_DIAG_RESET].is<bool>()) {
    const auto value = src[S_OPTIONS][S_AUTO_DIAG_RESET].as<bool>();

    result.accepted = true;
    if (value != dst.options.autoDiagReset) {
      dst.options.autoDiagReset = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_SET_DATE_AND_TIME].is<bool>()) {
    const auto value = src[S_OPTIONS][S_SET_DATE_AND_TIME].as<bool>();

    result.accepted = true;
    if (value != dst.options.setDateAndTime) {
      dst.options.setDateAndTime = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_ALWAYS_SEND_INDOOR_TEMP].is<bool>()) {
    const auto value = src[S_OPTIONS][S_ALWAYS_SEND_INDOOR_TEMP].as<bool>();

    result.accepted = true;
    if (value != dst.options.alwaysSendIndoorTemp) {
      dst.options.alwaysSendIndoorTemp = value;
      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_NATIVE_OTC].is<bool>()) {
    const auto value = src[S_OPTIONS][S_NATIVE_OTC].as<bool>();

    result.accepted = true;
    if (value != dst.options.nativeOTC) {
      dst.options.nativeOTC = value;

      result.changed = true;
    }
  }

  if (src[S_OPTIONS][S_IMMERGAS_FIX].is<bool>()) {
    const auto value = src[S_OPTIONS][S_IMMERGAS_FIX].as<bool>();

    result.accepted = true;
    if (value != dst.options.immergasFix) {
      dst.options.immergasFix = value;
      result.changed = true;
    }
  }

  return result;
}

// MQTT settings

JsonVariant toJson(const MqttSettings& src, JsonVariant dst) {
  auto mqtt = dst.to<JsonObject>();
  mqtt[S_ENABLED] = src.enabled;
  mqtt[S_SERVER] = src.server;
  mqtt[S_PORT] = src.port;
  mqtt[S_USER] = src.user;
  mqtt[S_PASSWORD] = src.password;
  mqtt[S_PREFIX] = src.prefix;
  mqtt[S_INTERVAL] = src.interval;
  mqtt[S_HOME_ASSISTANT_DISCOVERY] = src.homeAssistantDiscovery;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, MqttSettings& dst) {
  SettingResult result;

  if (src[S_ENABLED].is<bool>()) {
    const auto value = src[S_ENABLED].as<bool>();

    result.accepted = true;
    if (value != dst.enabled) {
      dst.enabled = value;
      result.changed = true;
    }
  }

  if (src[S_SERVER].is<JsonString>()) {
    const JsonString value = src[S_SERVER].as<JsonString>();

    if (value.size() && value.size() < sizeof(dst.server)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.server) != 0) {
        strcpy(dst.server, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_PORT].is<unsigned short>()) {
    const auto value = src[S_PORT].as<unsigned short>();

    if (value > 0 && value <= 65535) {
      result.accepted = true;
      if (value != dst.port) {
        dst.port = value;
        result.changed = true;
      }
    }
  }

  if (src[S_USER].is<JsonString>()) {
    const JsonString value = src[S_USER].as<JsonString>();

    if (value.size() < sizeof(dst.user)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.user) != 0) {
        strcpy(dst.user, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_PASSWORD].is<JsonString>()) {
    const JsonString value = src[S_PASSWORD].as<JsonString>();

    if (value.size() < sizeof(dst.password)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.password) != 0) {
        strcpy(dst.password, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_PREFIX].is<JsonString>()) {
    const JsonString value = src[S_PREFIX].as<JsonString>();

    if (value.size() && value.size() < sizeof(dst.prefix)) {
      result.accepted = true;
      if (strcmp(value.c_str(), dst.prefix) != 0) {
        strcpy(dst.prefix, value.c_str());
        result.changed = true;
      }
    }
  }

  if (src[S_INTERVAL].is<unsigned short>()) {
    const auto value = src[S_INTERVAL].as<unsigned short>();

    if (value >= 3 && value <= 60) {
      result.accepted = true;
      if (value != dst.interval) {
        dst.interval = value;
        result.changed = true;
      }
    }
  }

  if (src[S_HOME_ASSISTANT_DISCOVERY].is<bool>()) {
    const auto value = src[S_HOME_ASSISTANT_DISCOVERY].as<bool>();

    result.accepted = true;
    if (value != dst.homeAssistantDiscovery) {
      dst.homeAssistantDiscovery = value;
      result.changed = true;
    }
  }

  return result;
}

// Emergency settings

static SettingResult setEmergencyTarget(EmergencySettings& dst, float value) {
  bool changed = false;

  bool noRegulators = !settings.opentherm.options.nativeOTC;
  float minTemp = noRegulators
    ? settings.heating.minTemp
    : THERMOSTAT_INDOOR_MIN_TEMP;
  float maxTemp = noRegulators
    ? settings.heating.maxTemp
    : THERMOSTAT_INDOOR_MAX_TEMP;

  bool valid = isValidTemp(
    value,
    UnitSystem::METRIC,
    minTemp,
    maxTemp,
    UnitSystem::METRIC
  );

  if (!valid) {
    value = noRegulators ? DEFAULT_HEATING_TARGET_TEMP : THERMOSTAT_INDOOR_DEFAULT_TEMP;
  }

  value = constrain(value, minTemp, maxTemp);

  if (isFloatDifferent(dst.target, value, 0.0001f)) {
    dst.target = roundf(value, 2);
    changed = true;
  }

  return {.changed = changed, .accepted = valid};
}

static SettingResult applySetting(EmergencySettings& dst, std::string_view path, float input) {
  if (path == S_TARGET) {
    return setEmergencyTarget(dst, input);
  }

  return {};
}

JsonVariant toJson(const EmergencySettings& src, JsonVariant dst) {
  auto emergency = dst.to<JsonObject>();
  emergency[S_TARGET] = roundf(convertTemp(src.target, UnitSystem::METRIC, settings.system.unitSystem), 2);
  emergency[S_TRESHOLD_TIME] = src.tresholdTime;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, EmergencySettings& dst) {
  SettingResult result;

  if (src[S_TRESHOLD_TIME].is<unsigned short>()) {
    const auto value = src[S_TRESHOLD_TIME].as<unsigned short>();

    if (value >= 60 && value <= 1800) {
      result.accepted = true;
      if (value != dst.tresholdTime) {
        dst.tresholdTime = value;
        result.changed = true;
      }
    }
  }

  float value = src[S_TARGET].is<float>()
    ? convertTemp(src[S_TARGET].as<float>(), settings.system.unitSystem, UnitSystem::METRIC)
    : dst.target;
  const auto fieldResult = applySetting(dst, S_TARGET, value);
  result.changed = fieldResult.changed || result.changed;
  if (src[S_TARGET].is<float>()) {
    result.accepted = fieldResult.accepted || result.accepted;
  }

  return result;
}

// Heating settings

static bool normalizeHeatingBounds(HeatingSettings& dst) {
  bool changed = false;

  if (dst.maxTemp < dst.minTemp) {
    dst.maxTemp = dst.minTemp;
    changed = true;
  }

  return changed;
}

static bool normalizeHeatingOverheatBounds(HeatingSettings& dst) {
  bool changed = false;

  if (dst.overheatProtection.highTemp < dst.overheatProtection.lowTemp) {
    dst.overheatProtection.highTemp = dst.overheatProtection.lowTemp;
    changed = true;
  }

  return changed;
}

static bool normalizeHeatingFreezeBounds(HeatingSettings& dst) {
  bool changed = false;

  if (dst.freezeProtection.highTemp < dst.freezeProtection.lowTemp) {
    dst.freezeProtection.highTemp = dst.freezeProtection.lowTemp;
    changed = true;
  }

  return changed;
}

static SettingResult setHeatingTarget(HeatingSettings& dst, float value) {
  bool changed = false;

  bool indoorTempControl = settings.equitherm.enabled || settings.pid.enabled || settings.opentherm.options.nativeOTC;
  float minTemp = indoorTempControl
    ? THERMOSTAT_INDOOR_MIN_TEMP
    : dst.minTemp;
  float maxTemp = indoorTempControl
    ? THERMOSTAT_INDOOR_MAX_TEMP
    : dst.maxTemp;

  bool valid = isValidTemp(
    value,
    UnitSystem::METRIC,
    minTemp,
    maxTemp,
    UnitSystem::METRIC
  );

  if (!valid) {
    value = indoorTempControl ? THERMOSTAT_INDOOR_DEFAULT_TEMP : DEFAULT_HEATING_TARGET_TEMP;
  }

  value = constrain(value, minTemp, maxTemp);

  if (isFloatDifferent(dst.target, value, 0.0001f)) {
    dst.target = roundf(value, 2);
    changed = true;
  }

  return {.changed = changed, .accepted = valid};
}

static SettingResult applySetting(HeatingSettings& dst, std::string_view path, bool value) {
  bool changed = false;

  if (path == S_ENABLED) {
    if (value != dst.enabled) {
      dst.enabled = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_TURBO) {
    if (value && (settings.opentherm.options.nativeOTC || (!settings.equitherm.enabled && !settings.pid.enabled))) {
      return {};
    }

    if (value != dst.turbo) {
      dst.turbo = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "hysteresis/enabled") {
    if (value != dst.hysteresis.enabled) {
      dst.hysteresis.enabled = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  return {};
}

static SettingResult applySetting(HeatingSettings& dst, std::string_view path, float input) {
  bool changed = false;

  if (path == "hysteresis/value") {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0.0f || value > 15.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.hysteresis.value, 0.0001f)) {
      dst.hysteresis.value = roundf(value, 2);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_TURBO_FACTOR) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 1.5f || value > 10.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.turboFactor, 0.0001f)) {
      dst.turboFactor = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_MIN_TEMP) {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value < vars.slave.heating.minTemp
        || value >= vars.slave.heating.maxTemp
        || value == dst.maxTemp) {
      return {};
    }

    if (value != dst.minTemp) {
      dst.minTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_MAX_TEMP) {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value <= vars.slave.heating.minTemp
        || value > vars.slave.heating.maxTemp
        || value == dst.minTemp) {
      return {};
    }

    if (value != dst.maxTemp) {
      dst.maxTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_MAX_MODULATION) {
    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value <= 0 || value > 100) {
      return {};
    }

    if (value != dst.maxModulation) {
      dst.maxModulation = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_INDOOR_TEMP_AVG_TYPE) {
    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    uint8_t value = static_cast<uint8_t>(input);

    switch (value) {
      case static_cast<uint8_t>(Sensors::AverageType::MEAN):
      case static_cast<uint8_t>(Sensors::AverageType::MINIMUM):
      case static_cast<uint8_t>(Sensors::AverageType::MAXIMUM):
        if (static_cast<uint8_t>(dst.indoorTempAvgType) != value) {
          dst.indoorTempAvgType = static_cast<Sensors::AverageType>(value);
          changed = true;
        }
        break;

      default:
        return {};
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_OUTDOOR_TEMP_AVG_TYPE) {
    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    uint8_t value = static_cast<uint8_t>(input);

    switch (value) {
      case static_cast<uint8_t>(Sensors::AverageType::MEAN):
      case static_cast<uint8_t>(Sensors::AverageType::MINIMUM):
      case static_cast<uint8_t>(Sensors::AverageType::MAXIMUM):
        if (static_cast<uint8_t>(dst.outdoorTempAvgType) != value) {
          dst.outdoorTempAvgType = static_cast<Sensors::AverageType>(value);
          changed = true;
        }
        break;

      default:
        return {};
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "overheat_protection/high_temp") {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value != 0 && !isValidTemp(value, UnitSystem::METRIC, 0.0f, 100.0f)) {
      return {};
    }

    if (value != dst.overheatProtection.highTemp) {
      dst.overheatProtection.highTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "overheat_protection/low_temp") {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value != 0 && !isValidTemp(value, UnitSystem::METRIC, 0.0f, 99.0f)) {
      return {};
    }

    if (value != dst.overheatProtection.lowTemp) {
      dst.overheatProtection.lowTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "freeze_protection/high_temp") {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 65535)) {
      return {};
    }

    unsigned short value = static_cast<unsigned short>(input);

    if (!isValidTemp(value, UnitSystem::METRIC, 1, 50)) {
      return {};
    }

    if (value != dst.freezeProtection.highTemp) {
      dst.freezeProtection.highTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "freeze_protection/low_temp") {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 65535)) {
      return {};
    }

    unsigned short value = static_cast<unsigned short>(input);

    if (!isValidTemp(value, UnitSystem::METRIC, 1, 30)) {
      return {};
    }

    if (value != dst.freezeProtection.lowTemp) {
      dst.freezeProtection.lowTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_TARGET) {
    return setHeatingTarget(dst, input);
  }

  return {};
}

JsonVariant toJson(const HeatingSettings& src, JsonVariant dst) {
  auto heating = dst.to<JsonObject>();
  heating[S_ENABLED] = src.enabled;
  heating[S_TURBO] = src.turbo;
  heating[S_TARGET] = roundf(convertTemp(src.target, UnitSystem::METRIC, settings.system.unitSystem), 2);
  heating[S_HYSTERESIS][S_ENABLED] = src.hysteresis.enabled;
  heating[S_HYSTERESIS][S_VALUE] = roundf(src.hysteresis.value, 3);
  heating[S_TURBO_FACTOR] = roundf(src.turboFactor, 3);
  heating[S_MIN_TEMP] = convertTemp(src.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
  heating[S_MAX_TEMP] = convertTemp(src.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);
  heating[S_MAX_MODULATION] = src.maxModulation;
  heating[S_INDOOR_TEMP_AVG_TYPE] = static_cast<uint8_t>(src.indoorTempAvgType);
  heating[S_OUTDOOR_TEMP_AVG_TYPE] = static_cast<uint8_t>(src.outdoorTempAvgType);

  auto heatingOverheatProtection = heating[S_OVERHEAT_PROTECTION].to<JsonObject>();
  heatingOverheatProtection[S_HIGH_TEMP] = src.overheatProtection.highTemp == 0
    ? 0
    : convertTemp(src.overheatProtection.highTemp, UnitSystem::METRIC, settings.system.unitSystem);
  heatingOverheatProtection[S_LOW_TEMP] = src.overheatProtection.lowTemp == 0
    ? 0
    : convertTemp(src.overheatProtection.lowTemp, UnitSystem::METRIC, settings.system.unitSystem);

  auto freezeProtection = heating[S_FREEZE_PROTECTION].to<JsonObject>();
  freezeProtection[S_HIGH_TEMP] = convertTemp(src.freezeProtection.highTemp, UnitSystem::METRIC, settings.system.unitSystem);
  freezeProtection[S_LOW_TEMP] = convertTemp(src.freezeProtection.lowTemp, UnitSystem::METRIC, settings.system.unitSystem);

  return dst;
}

SettingResult fromJson(JsonVariantConst src, HeatingSettings& dst) {
  SettingResult result;

  if (src[S_ENABLED].is<bool>()) {
    const auto value = src[S_ENABLED].as<bool>();
    const auto fieldResult = applySetting(dst, S_ENABLED, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_TURBO].is<bool>()) {
    const auto value = src[S_TURBO].as<bool>();
    const auto fieldResult = applySetting(dst, S_TURBO, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_HYSTERESIS][S_ENABLED].is<bool>()) {
    const auto value = src[S_HYSTERESIS][S_ENABLED].as<bool>();
    const auto fieldResult = applySetting(dst, "hysteresis/enabled", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_HYSTERESIS][S_VALUE].is<float>()) {
    const auto value = src[S_HYSTERESIS][S_VALUE].as<float>();
    const auto fieldResult = applySetting(dst, "hysteresis/value", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_TURBO_FACTOR].is<float>()) {
    const auto value = src[S_TURBO_FACTOR].as<float>();
    const auto fieldResult = applySetting(dst, S_TURBO_FACTOR, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_MIN_TEMP].is<float>()) {
    float value = convertTemp(src[S_MIN_TEMP].as<float>(), settings.system.unitSystem, UnitSystem::METRIC);
    const auto fieldResult = applySetting(dst, S_MIN_TEMP, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_MAX_TEMP].is<float>()) {
    float value = convertTemp(src[S_MAX_TEMP].as<float>(), settings.system.unitSystem, UnitSystem::METRIC);
    const auto fieldResult = applySetting(dst, S_MAX_TEMP, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  result.changed = normalizeHeatingBounds(dst) || result.changed;


  if (src[S_MAX_MODULATION].is<uint8_t>()) {
    const auto value = src[S_MAX_MODULATION].as<uint8_t>();
    const auto fieldResult = applySetting(dst, S_MAX_MODULATION, static_cast<float>(value));
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_INDOOR_TEMP_AVG_TYPE].is<uint8_t>()) {
    const auto value = src[S_INDOOR_TEMP_AVG_TYPE].as<uint8_t>();
    const auto fieldResult = applySetting(dst, S_INDOOR_TEMP_AVG_TYPE, static_cast<float>(value));
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_OUTDOOR_TEMP_AVG_TYPE].is<uint8_t>()) {
    const auto value = src[S_OUTDOOR_TEMP_AVG_TYPE].as<uint8_t>();
    const auto fieldResult = applySetting(dst, S_OUTDOOR_TEMP_AVG_TYPE, static_cast<float>(value));
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_OVERHEAT_PROTECTION][S_HIGH_TEMP].is<float>()) {
    float value = src[S_OVERHEAT_PROTECTION][S_HIGH_TEMP].as<float>();
    if (value != 0) {
      value = convertTemp(value, settings.system.unitSystem, UnitSystem::METRIC);
    }

    const auto fieldResult = applySetting(dst, "overheat_protection/high_temp", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_OVERHEAT_PROTECTION][S_LOW_TEMP].is<float>()) {
    float value = src[S_OVERHEAT_PROTECTION][S_LOW_TEMP].as<float>();
    if (value != 0) {
      value = convertTemp(value, settings.system.unitSystem, UnitSystem::METRIC);
    }

    const auto fieldResult = applySetting(dst, "overheat_protection/low_temp", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  result.changed = normalizeHeatingOverheatBounds(dst) || result.changed;

  if (src[S_FREEZE_PROTECTION][S_HIGH_TEMP].is<float>()) {
    float value = convertTemp(src[S_FREEZE_PROTECTION][S_HIGH_TEMP].as<float>(), settings.system.unitSystem, UnitSystem::METRIC);
    const auto fieldResult = applySetting(dst, "freeze_protection/high_temp", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_FREEZE_PROTECTION][S_LOW_TEMP].is<float>()) {
    float value = convertTemp(src[S_FREEZE_PROTECTION][S_LOW_TEMP].as<float>(), settings.system.unitSystem, UnitSystem::METRIC);
    const auto fieldResult = applySetting(dst, "freeze_protection/low_temp", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  result.changed = normalizeHeatingFreezeBounds(dst) || result.changed;

  float value = src[S_TARGET].is<float>()
    ? convertTemp(src[S_TARGET].as<float>(), settings.system.unitSystem, UnitSystem::METRIC)
    : dst.target;
  const auto fieldResult = applySetting(dst, S_TARGET, value);
  result.changed = fieldResult.changed || result.changed;
  if (src[S_TARGET].is<float>()) {
    result.accepted = fieldResult.accepted || result.accepted;
  }

  return result;
}

// DHW settings

static bool normalizeDhwBounds(DhwSettings& dst) {
  bool changed = false;

  if (dst.maxTemp < dst.minTemp) {
    dst.maxTemp = dst.minTemp;
    changed = true;
  }

  return changed;
}

static bool normalizeDhwOverheatBounds(DhwSettings& dst) {
  bool changed = false;

  if (dst.overheatProtection.highTemp < dst.overheatProtection.lowTemp) {
    dst.overheatProtection.highTemp = dst.overheatProtection.lowTemp;
    changed = true;
  }

  return changed;
}

static SettingResult setDhwTarget(DhwSettings& dst, float value) {
  bool changed = false;

  bool valid = isValidTemp(
    value,
    UnitSystem::METRIC,
    dst.minTemp,
    dst.maxTemp,
    UnitSystem::METRIC
  );

  if (!valid) {
    value = DEFAULT_DHW_TARGET_TEMP;
  }

  value = constrain(value, dst.minTemp, dst.maxTemp);

  if (isFloatDifferent(dst.target, value, 0.0001f)) {
    dst.target = value;
    changed = true;
  }

  return {.changed = changed, .accepted = valid};
}

static SettingResult applySetting(DhwSettings& dst, std::string_view path, bool value) {
  bool changed = false;

  if (path == S_ENABLED) {
    if (value != dst.enabled) {
      dst.enabled = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  return {};
}

static SettingResult applySetting(DhwSettings& dst, std::string_view path, float input) {
  bool changed = false;

  if (path == S_MIN_TEMP) {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value < vars.slave.dhw.minTemp
        || value >= vars.slave.dhw.maxTemp) {
      return {};
    }

    if (value != dst.minTemp) {
      dst.minTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_MAX_TEMP) {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value <= vars.slave.dhw.minTemp
        || value > vars.slave.dhw.maxTemp) {
      return {};
    }

    if (value != dst.maxTemp) {
      dst.maxTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_MAX_MODULATION) {
    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value <= 0 || value > 100) {
      return {};
    }

    if (value != dst.maxModulation) {
      dst.maxModulation = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "overheat_protection/high_temp") {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value != 0 && !isValidTemp(value, UnitSystem::METRIC, 0.0f, 100.0f)) {
      return {};
    }

    if (value != dst.overheatProtection.highTemp) {
      dst.overheatProtection.highTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "overheat_protection/low_temp") {
    input = std::round(input);

    if (!isSettingInteger(input, 0, 255)) {
      return {};
    }

    unsigned char value = static_cast<unsigned char>(input);

    if (value != 0 && !isValidTemp(value, UnitSystem::METRIC, 0.0f, 99.0f)) {
      return {};
    }

    if (value != dst.overheatProtection.lowTemp) {
      dst.overheatProtection.lowTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_TARGET) {
    return setDhwTarget(dst, input);
  }

  return {};
}

JsonVariant toJson(const DhwSettings& src, JsonVariant dst) {
  auto dhw = dst.to<JsonObject>();
  dhw[S_ENABLED] = src.enabled;
  dhw[S_TARGET] = roundf(convertTemp(src.target, UnitSystem::METRIC, settings.system.unitSystem), 1);
  dhw[S_MIN_TEMP] = convertTemp(src.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
  dhw[S_MAX_TEMP] = convertTemp(src.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);
  dhw[S_MAX_MODULATION] = src.maxModulation;

  auto dhwOverheatProtection = dhw[S_OVERHEAT_PROTECTION].to<JsonObject>();
  dhwOverheatProtection[S_HIGH_TEMP] = src.overheatProtection.highTemp == 0
    ? 0
    : convertTemp(src.overheatProtection.highTemp, UnitSystem::METRIC, settings.system.unitSystem);
  dhwOverheatProtection[S_LOW_TEMP] = src.overheatProtection.lowTemp == 0
    ? 0
    : convertTemp(src.overheatProtection.lowTemp, UnitSystem::METRIC, settings.system.unitSystem);

  return dst;
}

SettingResult fromJson(JsonVariantConst src, DhwSettings& dst) {
  SettingResult result;

  if (src[S_ENABLED].is<bool>()) {
    const auto value = src[S_ENABLED].as<bool>();
    const auto fieldResult = applySetting(dst, S_ENABLED, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_MIN_TEMP].is<float>()) {
    float value = convertTemp(src[S_MIN_TEMP].as<float>(), settings.system.unitSystem, UnitSystem::METRIC);
    const auto fieldResult = applySetting(dst, S_MIN_TEMP, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_MAX_TEMP].is<float>()) {
    float value = convertTemp(src[S_MAX_TEMP].as<float>(), settings.system.unitSystem, UnitSystem::METRIC);
    const auto fieldResult = applySetting(dst, S_MAX_TEMP, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  result.changed = normalizeDhwBounds(dst) || result.changed;

  if (src[S_MAX_MODULATION].is<uint8_t>()) {
    const auto value = src[S_MAX_MODULATION].as<uint8_t>();
    const auto fieldResult = applySetting(dst, S_MAX_MODULATION, static_cast<float>(value));
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_OVERHEAT_PROTECTION][S_HIGH_TEMP].is<float>()) {
    float value = src[S_OVERHEAT_PROTECTION][S_HIGH_TEMP].as<float>();
    if (value != 0) {
      value = convertTemp(value, settings.system.unitSystem, UnitSystem::METRIC);
    }

    const auto fieldResult = applySetting(dst, "overheat_protection/high_temp", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_OVERHEAT_PROTECTION][S_LOW_TEMP].is<float>()) {
    float value = src[S_OVERHEAT_PROTECTION][S_LOW_TEMP].as<float>();
    if (value != 0) {
      value = convertTemp(value, settings.system.unitSystem, UnitSystem::METRIC);
    }

    const auto fieldResult = applySetting(dst, "overheat_protection/low_temp", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  result.changed = normalizeDhwOverheatBounds(dst) || result.changed;

  float value = src[S_TARGET].is<float>()
    ? convertTemp(src[S_TARGET].as<float>(), settings.system.unitSystem, UnitSystem::METRIC)
    : dst.target;
  const auto fieldResult = applySetting(dst, S_TARGET, value);
  result.changed = fieldResult.changed || result.changed;
  if (src[S_TARGET].is<float>()) {
    result.accepted = fieldResult.accepted || result.accepted;
  }

  return result;
}

// PID settings

static bool normalizePidBounds(PidSettings& dst) {
  bool changed = false;

  if (dst.maxTemp < dst.minTemp) {
    dst.maxTemp = dst.minTemp;
    changed = true;
  }

  return changed;
}

static SettingResult applySetting(PidSettings& dst, std::string_view path, bool value) {
  bool changed = false;

  if (path == S_ENABLED) {
    if (!settings.opentherm.options.nativeOTC) {
      if (value != dst.enabled) {
        dst.enabled = value;
        changed = true;
      }

    } else if (dst.enabled) {
      dst.enabled = false;
      changed = true;
    }

    return {.changed = changed, .accepted = !value || !settings.opentherm.options.nativeOTC};
  }

  if (path == "deadband/enabled") {
    if (value != dst.deadband.enabled) {
      dst.deadband.enabled = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  return {};
}

static SettingResult applySetting(PidSettings& dst, std::string_view path, float input) {
  bool changed = false;

  if (path == S_P_FACTOR) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value <= 0 || value > 1000) {
      return {};
    }

    if (isFloatDifferent(value, dst.p_factor, 0.0001f)) {
      dst.p_factor = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_I_FACTOR) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0 || value > 100) {
      return {};
    }

    if (isFloatDifferent(value, dst.i_factor, 0.0001f)) {
      dst.i_factor = roundf(value, 4);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_D_FACTOR) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0 || value > 100000) {
      return {};
    }

    if (isFloatDifferent(value, dst.d_factor, 0.0001f)) {
      dst.d_factor = roundf(value, 1);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_DT) {
    if (!isSettingInteger(input, 0, 65535)) {
      return {};
    }

    unsigned short value = static_cast<unsigned short>(input);

    if (value < 30 || value > 1800) {
      return {};
    }

    if (value != dst.dt) {
      dst.dt = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_MIN_TEMP) {
    if (!settings.equitherm.enabled) {
      input = std::round(input);
    }

    if (!isSettingInteger(input, -32768, 32767)) {
      return {};
    }

    short value = static_cast<short>(input);

    if (settings.equitherm.enabled) {
      if (value < -99.9f || value > 99.9f) {
        return {};
      }

    } else if (!isValidTemp(value, UnitSystem::METRIC, 0.0f)) {
      return {};
    }

    if (value != dst.minTemp) {
      dst.minTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_MAX_TEMP) {
    if (!settings.equitherm.enabled) {
      input = std::round(input);
    }

    if (!isSettingInteger(input, -32768, 32767)) {
      return {};
    }

    short value = static_cast<short>(input);

    if (settings.equitherm.enabled) {
      if (value < 0.1f || value > 99.9f) {
        return {};
      }

    } else if (!isValidTemp(value, UnitSystem::METRIC)) {
      return {};
    }

    if (value != dst.maxTemp) {
      dst.maxTemp = value;
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "deadband/p_multiplier") {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0 || value > 1) {
      return {};
    }

    if (isFloatDifferent(value, dst.deadband.p_multiplier, 0.0001f)) {
      dst.deadband.p_multiplier = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "deadband/i_multiplier") {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0 || value > 1) {
      return {};
    }

    if (isFloatDifferent(value, dst.deadband.i_multiplier, 0.0001f)) {
      dst.deadband.i_multiplier = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "deadband/d_multiplier") {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0 || value > 1) {
      return {};
    }

    if (isFloatDifferent(value, dst.deadband.d_multiplier, 0.0001f)) {
      dst.deadband.d_multiplier = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "deadband/threshold_high") {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0.0f || value > 5.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.deadband.thresholdHigh, 0.0001f)) {
      dst.deadband.thresholdHigh = roundf(value, 2);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == "deadband/threshold_low") {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0.0f || value > 5.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.deadband.thresholdLow, 0.0001f)) {
      dst.deadband.thresholdLow = roundf(value, 2);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  return {};
}

JsonVariant toJson(const PidSettings& src, JsonVariant dst) {
  auto pid = dst.to<JsonObject>();
  pid[S_ENABLED] = src.enabled;
  pid[S_P_FACTOR] = roundf(src.p_factor, 3);
  pid[S_I_FACTOR] = roundf(src.i_factor, 4);
  pid[S_D_FACTOR] = roundf(src.d_factor, 1);
  pid[S_DT] = src.dt;
  pid[S_MIN_TEMP] = settings.equitherm.enabled
    ? src.minTemp
    : convertTemp(src.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
  pid[S_MAX_TEMP] = settings.equitherm.enabled
    ? src.maxTemp
    : convertTemp(src.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);

  auto pidDeadband = pid[S_DEADBAND].to<JsonObject>();
  pidDeadband[S_ENABLED] = src.deadband.enabled;
  pidDeadband[S_P_MULTIPLIER] = src.deadband.p_multiplier;
  pidDeadband[S_I_MULTIPLIER] = src.deadband.i_multiplier;
  pidDeadband[S_D_MULTIPLIER] = src.deadband.d_multiplier;
  pidDeadband[S_THRESHOLD_HIGH] = src.deadband.thresholdHigh;
  pidDeadband[S_THRESHOLD_LOW] = src.deadband.thresholdLow;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, PidSettings& dst) {
  SettingResult result;

  if (src[S_ENABLED].is<bool>()) {
    const auto value = src[S_ENABLED].as<bool>();
    const auto fieldResult = applySetting(dst, S_ENABLED, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_P_FACTOR].is<float>()) {
    const auto value = src[S_P_FACTOR].as<float>();
    const auto fieldResult = applySetting(dst, S_P_FACTOR, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_I_FACTOR].is<float>()) {
    const auto value = src[S_I_FACTOR].as<float>();
    const auto fieldResult = applySetting(dst, S_I_FACTOR, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_D_FACTOR].is<float>()) {
    const auto value = src[S_D_FACTOR].as<float>();
    const auto fieldResult = applySetting(dst, S_D_FACTOR, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_DT].is<unsigned short>()) {
    const auto value = src[S_DT].as<unsigned short>();
    const auto fieldResult = applySetting(dst, S_DT, static_cast<float>(value));
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_MIN_TEMP].is<float>()) {
    float value = src[S_MIN_TEMP].as<float>();
    if (!settings.equitherm.enabled) {
      value = convertTemp(value, settings.system.unitSystem, UnitSystem::METRIC);
    }

    const auto fieldResult = applySetting(dst, S_MIN_TEMP, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_MAX_TEMP].is<float>()) {
    float value = src[S_MAX_TEMP].as<float>();
    if (!settings.equitherm.enabled) {
      value = convertTemp(value, settings.system.unitSystem, UnitSystem::METRIC);
    }

    const auto fieldResult = applySetting(dst, S_MAX_TEMP, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  result.changed = normalizePidBounds(dst) || result.changed;

  if (src[S_DEADBAND][S_ENABLED].is<bool>()) {
    const auto value = src[S_DEADBAND][S_ENABLED].as<bool>();
    const auto fieldResult = applySetting(dst, "deadband/enabled", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_DEADBAND][S_P_MULTIPLIER].is<float>()) {
    const auto value = src[S_DEADBAND][S_P_MULTIPLIER].as<float>();
    const auto fieldResult = applySetting(dst, "deadband/p_multiplier", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_DEADBAND][S_I_MULTIPLIER].is<float>()) {
    const auto value = src[S_DEADBAND][S_I_MULTIPLIER].as<float>();
    const auto fieldResult = applySetting(dst, "deadband/i_multiplier", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_DEADBAND][S_D_MULTIPLIER].is<float>()) {
    const auto value = src[S_DEADBAND][S_D_MULTIPLIER].as<float>();
    const auto fieldResult = applySetting(dst, "deadband/d_multiplier", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_DEADBAND][S_THRESHOLD_HIGH].is<float>()) {
    const auto value = src[S_DEADBAND][S_THRESHOLD_HIGH].as<float>();
    const auto fieldResult = applySetting(dst, "deadband/threshold_high", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_DEADBAND][S_THRESHOLD_LOW].is<float>()) {
    const auto value = src[S_DEADBAND][S_THRESHOLD_LOW].as<float>();
    const auto fieldResult = applySetting(dst, "deadband/threshold_low", value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  return result;
}

// Equitherm settings

static SettingResult applySetting(EquithermSettings& dst, std::string_view path, bool value) {
  bool changed = false;

  if (path == S_ENABLED) {
    if (!settings.opentherm.options.nativeOTC) {
      if (value != dst.enabled) {
        dst.enabled = value;
        changed = true;
      }

    } else if (dst.enabled) {
      dst.enabled = false;
      changed = true;
    }

    return {.changed = changed, .accepted = !value || !settings.opentherm.options.nativeOTC};
  }

  return {};
}

static SettingResult applySetting(EquithermSettings& dst, std::string_view path, float input) {
  bool changed = false;

  if (path == S_SLOPE) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0.001f || value > 10.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.slope, 0.0001f)) {
      dst.slope = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_EXPONENT) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value <= 0.01f || value > 2.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.exponent, 0.0001f)) {
      dst.exponent = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_SHIFT) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < -15.0f || value > 15.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.shift, 0.0001f)) {
      dst.shift = roundf(value, 2);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  if (path == S_TARGET_DIFF_FACTOR) {
    float value = input;

    if (!std::isfinite(value)) {
      return {};
    }

    if (value < 0.0f || value > 10.0f) {
      return {};
    }

    if (isFloatDifferent(value, dst.targetDiffFactor, 0.0001f)) {
      dst.targetDiffFactor = roundf(value, 3);
      changed = true;
    }

    return {.changed = changed, .accepted = true};
  }

  return {};
}

JsonVariant toJson(const EquithermSettings& src, JsonVariant dst) {
  auto equitherm = dst.to<JsonObject>();
  equitherm[S_ENABLED] = src.enabled;
  equitherm[S_SLOPE] = roundf(src.slope, 3);
  equitherm[S_EXPONENT] = roundf(src.exponent, 3);
  equitherm[S_SHIFT] = roundf(src.shift, 2);
  equitherm[S_TARGET_DIFF_FACTOR] = roundf(src.targetDiffFactor, 3);

  return dst;
}

SettingResult fromJson(JsonVariantConst src, EquithermSettings& dst) {
  SettingResult result;

  if (src[S_ENABLED].is<bool>()) {
    const auto value = src[S_ENABLED].as<bool>();
    const auto fieldResult = applySetting(dst, S_ENABLED, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_SLOPE].is<float>()) {
    const auto value = src[S_SLOPE].as<float>();
    const auto fieldResult = applySetting(dst, S_SLOPE, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_EXPONENT].is<float>()) {
    const auto value = src[S_EXPONENT].as<float>();
    const auto fieldResult = applySetting(dst, S_EXPONENT, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_SHIFT].is<float>()) {
    const auto value = src[S_SHIFT].as<float>();
    const auto fieldResult = applySetting(dst, S_SHIFT, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  if (src[S_TARGET_DIFF_FACTOR].is<float>()) {
    const auto value = src[S_TARGET_DIFF_FACTOR].as<float>();
    const auto fieldResult = applySetting(dst, S_TARGET_DIFF_FACTOR, value);
    result.changed = fieldResult.changed || result.changed;
    result.accepted = fieldResult.accepted || result.accepted;
  }

  return result;
}

// External pump settings

JsonVariant toJson(const ExternalPumpSettings& src, JsonVariant dst) {
  auto externalPump = dst.to<JsonObject>();
  externalPump[S_USE] = src.use;
  externalPump[S_GPIO] = src.gpio;
  externalPump[S_INVERT_STATE] = src.invertState;
  externalPump[S_POST_CIRCULATION_TIME] = roundf(src.postCirculationTime / 60, 0);
  externalPump[S_ANTI_STUCK_INTERVAL] = roundf(src.antiStuckInterval / 86400, 0);
  externalPump[S_ANTI_STUCK_TIME] = roundf(src.antiStuckTime / 60, 0);

  return dst;
}

SettingResult fromJson(JsonVariantConst src, ExternalPumpSettings& dst) {
  SettingResult result;

  if (src[S_USE].is<bool>()) {
    const auto value = src[S_USE].as<bool>();

    result.accepted = true;
    if (value != dst.use) {
      dst.use = value;
      result.changed = true;
    }
  }

  if (!src[S_GPIO].isNull()) {
    if (src[S_GPIO].is<JsonString>() && src[S_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.gpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_GPIO].is<uint8_t>()) {
      const auto value = src[S_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED || GPIO_IS_VALID_OUTPUT(value)) {
        result.accepted = true;
        if (value != dst.gpio) {
          dst.gpio = value;
          result.changed = true;
        }
      }
    }
  }

  if (src[S_INVERT_STATE].is<bool>()) {
    const auto value = src[S_INVERT_STATE].as<bool>();

    result.accepted = true;
    if (value != dst.invertState) {
      dst.invertState = value;
      result.changed = true;
    }
  }

  if (src[S_POST_CIRCULATION_TIME].is<unsigned short>()) {
    unsigned short value = src[S_POST_CIRCULATION_TIME].as<unsigned short>();

    if (value <= 120) {
      value = value * 60;

      result.accepted = true;
      if (value != dst.postCirculationTime) {
        dst.postCirculationTime = value;
        result.changed = true;
      }
    }
  }

  if (src[S_ANTI_STUCK_INTERVAL].is<unsigned int>()) {
    unsigned int value = src[S_ANTI_STUCK_INTERVAL].as<unsigned int>();

    if (value <= 366) {
      value = value * 86400;

      result.accepted = true;
      if (value != dst.antiStuckInterval) {
        dst.antiStuckInterval = value;
        result.changed = true;
      }
    }
  }

  if (src[S_ANTI_STUCK_TIME].is<unsigned short>()) {
    unsigned short value = src[S_ANTI_STUCK_TIME].as<unsigned short>();

    if (value <= 20) {
      value = value * 60;

      result.accepted = true;
      if (value != dst.antiStuckTime) {
        dst.antiStuckTime = value;
        result.changed = true;
      }
    }
  }

  return result;
}

// Cascade control settings

JsonVariant toJson(const CascadeControlSettings& src, JsonVariant dst) {
  auto cascadeControl = dst.to<JsonObject>();
  auto cascadeControlInput = cascadeControl[S_INPUT].to<JsonObject>();
  cascadeControlInput[S_ENABLED] = src.input.enabled;
  cascadeControlInput[S_GPIO] = src.input.gpio;
  cascadeControlInput[S_INVERT_STATE] = src.input.invertState;
  cascadeControlInput[S_THRESHOLD_TIME] = src.input.thresholdTime;

  auto cascadeControlOutput = cascadeControl[S_OUTPUT].to<JsonObject>();
  cascadeControlOutput[S_ENABLED] = src.output.enabled;
  cascadeControlOutput[S_GPIO] = src.output.gpio;
  cascadeControlOutput[S_INVERT_STATE] = src.output.invertState;
  cascadeControlOutput[S_THRESHOLD_TIME] = src.output.thresholdTime;
  cascadeControlOutput[S_ON_FAULT] = src.output.onFault;
  cascadeControlOutput[S_ON_LOSS_CONNECTION] = src.output.onLossConnection;
  cascadeControlOutput[S_ON_ENABLED_HEATING] = src.output.onEnabledHeating;

  return dst;
}

SettingResult fromJson(JsonVariantConst src, CascadeControlSettings& dst) {
  SettingResult result;

  if (src[S_INPUT][S_ENABLED].is<bool>()) {
    const auto value = src[S_INPUT][S_ENABLED].as<bool>();

    result.accepted = true;
    if (value != dst.input.enabled) {
      dst.input.enabled = value;
      result.changed = true;
    }
  }

  if (!src[S_INPUT][S_GPIO].isNull()) {
    if (src[S_INPUT][S_GPIO].is<JsonString>() && src[S_INPUT][S_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.input.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.input.gpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_INPUT][S_GPIO].is<uint8_t>()) {
      const auto value = src[S_INPUT][S_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED || GPIO_IS_VALID_INPUT(value)) {
        result.accepted = true;
        if (value != dst.input.gpio) {
          dst.input.gpio = value;
          result.changed = true;
        }
      }
    }
  }

  if (src[S_INPUT][S_INVERT_STATE].is<bool>()) {
    const auto value = src[S_INPUT][S_INVERT_STATE].as<bool>();

    result.accepted = true;
    if (value != dst.input.invertState) {
      dst.input.invertState = value;
      result.changed = true;
    }
  }

  if (src[S_INPUT][S_THRESHOLD_TIME].is<unsigned short>()) {
    const auto value = src[S_INPUT][S_THRESHOLD_TIME].as<unsigned short>();

    if (value >= 5 && value <= 600) {
      result.accepted = true;
      if (value != dst.input.thresholdTime) {
        dst.input.thresholdTime = value;
        result.changed = true;
      }
    }
  }

  if (src[S_OUTPUT][S_ENABLED].is<bool>()) {
    const auto value = src[S_OUTPUT][S_ENABLED].as<bool>();

    result.accepted = true;
    if (value != dst.output.enabled) {
      dst.output.enabled = value;
      result.changed = true;
    }
  }

  if (!src[S_OUTPUT][S_GPIO].isNull()) {
    if (src[S_OUTPUT][S_GPIO].is<JsonString>() && src[S_OUTPUT][S_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.output.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.output.gpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_OUTPUT][S_GPIO].is<uint8_t>()) {
      const auto value = src[S_OUTPUT][S_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED || GPIO_IS_VALID_OUTPUT(value)) {
        result.accepted = true;
        if (value != dst.output.gpio) {
          dst.output.gpio = value;
          result.changed = true;
        }
      }
    }
  }

  if (src[S_OUTPUT][S_INVERT_STATE].is<bool>()) {
    const auto value = src[S_OUTPUT][S_INVERT_STATE].as<bool>();

    result.accepted = true;
    if (value != dst.output.invertState) {
      dst.output.invertState = value;
      result.changed = true;
    }
  }

  if (src[S_OUTPUT][S_THRESHOLD_TIME].is<unsigned short>()) {
    const auto value = src[S_OUTPUT][S_THRESHOLD_TIME].as<unsigned short>();

    if (value >= 5 && value <= 600) {
      result.accepted = true;
      if (value != dst.output.thresholdTime) {
        dst.output.thresholdTime = value;
        result.changed = true;
      }
    }
  }

  if (src[S_OUTPUT][S_ON_FAULT].is<bool>()) {
    const auto value = src[S_OUTPUT][S_ON_FAULT].as<bool>();

    result.accepted = true;
    if (value != dst.output.onFault) {
      dst.output.onFault = value;
      result.changed = true;
    }
  }

  if (src[S_OUTPUT][S_ON_LOSS_CONNECTION].is<bool>()) {
    const auto value = src[S_OUTPUT][S_ON_LOSS_CONNECTION].as<bool>();

    result.accepted = true;
    if (value != dst.output.onLossConnection) {
      dst.output.onLossConnection = value;
      result.changed = true;
    }
  }

  if (src[S_OUTPUT][S_ON_ENABLED_HEATING].is<bool>()) {
    const auto value = src[S_OUTPUT][S_ON_ENABLED_HEATING].as<bool>();

    result.accepted = true;
    if (value != dst.output.onEnabledHeating) {
      dst.output.onEnabledHeating = value;
      result.changed = true;
    }
  }

  return result;
}

// MQTT paths and cross-section normalization

SettingResult applySetting(Settings& dst, std::string_view path, bool value) {
  const auto separator = path.find('/');
  if (separator == std::string_view::npos) {
    return {};
  }

  const auto section = path.substr(0, separator);
  path.remove_prefix(separator + 1);

  if (section == S_EQUITHERM) {
    return applySetting(dst.equitherm, path, value);
  }

  if (section == S_PID) {
    return applySetting(dst.pid, path, value);
  }

  if (section == S_HEATING) {
    return applySetting(dst.heating, path, value);
  }

  if (section == S_DHW) {
    return applySetting(dst.dhw, path, value);
  }

  return {};
}

SettingResult applySetting(Settings& dst, std::string_view path, float input) {
  const auto separator = path.find('/');
  if (separator == std::string_view::npos) {
    return {};
  }

  const auto section = path.substr(0, separator);
  path.remove_prefix(separator + 1);

  if (section == S_EQUITHERM) {
    return applySetting(dst.equitherm, path, input);
  }

  if (section == S_PID) {
    return applySetting(dst.pid, path, input);
  }

  if (section == S_HEATING) {
    return applySetting(dst.heating, path, input);
  }

  if (section == S_DHW) {
    return applySetting(dst.dhw, path, input);
  }

  if (section == S_EMERGENCY) {
    return applySetting(dst.emergency, path, input);
  }

  return {};
}

static bool normalizeRegulatorModes(Settings& dst) {
  bool changed = false;

  if (dst.opentherm.options.nativeOTC) {
    if (dst.equitherm.enabled) {
      dst.equitherm.enabled = false;
      changed = true;
    }
    if (dst.pid.enabled) {
      dst.pid.enabled = false;
      changed = true;
    }
  }

  return changed;
}

bool normalizeSettings(Settings& dst) {
  bool changed = false;

  changed = normalizeRegulatorModes(dst) || changed;

  changed = normalizePidBounds(dst.pid) || changed;
  changed = normalizeHeatingBounds(dst.heating) || changed;
  changed = normalizeHeatingOverheatBounds(dst.heating) || changed;
  changed = normalizeHeatingFreezeBounds(dst.heating) || changed;
  changed = normalizeDhwBounds(dst.dhw) || changed;
  changed = normalizeDhwOverheatBounds(dst.dhw) || changed;

  changed = setEmergencyTarget(dst.emergency, dst.emergency.target).changed || changed;
  changed = setHeatingTarget(dst.heating, dst.heating.target).changed || changed;
  changed = setDhwTarget(dst.dhw, dst.dhw.target).changed || changed;

  return changed;
}

// Sensor settings

JsonVariant toJson(const uint8_t sensorId, const Sensors::Settings& src, JsonVariant dst) {
  dst[S_ID] = sensorId;
  dst[S_ENABLED] = src.enabled;
  // Snapshot storage is temporary; the pointer adapter copies the name.
  dst[S_NAME] = static_cast<const char*>(src.name);
  dst[S_SLUG] = static_cast<const char*>(src.slug);
  dst[S_PURPOSE] = static_cast<uint8_t>(src.purpose);
  dst[S_TYPE] = static_cast<uint8_t>(src.type);
  dst[S_GPIO] = src.gpio;

  if (src.type == Sensors::Type::DALLAS_TEMP) {
    char addr[24];
    sprintf(
      addr,
      "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
      src.address[0], src.address[1], src.address[2], src.address[3],
      src.address[4], src.address[5], src.address[6], src.address[7]
    );
    dst[S_ADDRESS] = addr;

  } else if (src.type == Sensors::Type::BLUETOOTH) {
    char addr[18];
    sprintf(
      addr,
      "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
      src.address[0], src.address[1], src.address[2],
      src.address[3], src.address[4], src.address[5]
    );
    dst[S_ADDRESS] = addr;

  } else {
    dst[S_ADDRESS] = "";
  }

  dst[S_OFFSET] = roundf(src.offset, 3);
  dst[S_FACTOR] = roundf(src.factor, 3);
  dst[S_FILTERING] = src.filtering;
  dst[S_FILTERING_FACTOR] = roundf(src.filteringFactor, 3);
  return dst;
}

SettingResult fromJson(const JsonVariantConst src, Sensors::Settings& dst) {
  SettingResult result;

  // enabled
  if (src[S_ENABLED].is<bool>()) {
    const auto value = src[S_ENABLED].as<bool>();
    result.accepted = true;

    if (value != dst.enabled) {
      dst.enabled = value;
      result.changed = true;
    }
  }

  // name
  if (src[S_NAME].is<JsonString>()) {
    const auto raw = src[S_NAME].as<JsonString>();
    const auto value = Sensors::cleanName(std::string_view(raw.c_str(), raw.size()));

    if (value.size() < sizeof(dst.name) && value != dst.name) {
      const auto slug = Sensors::makeSlug(value);
      bool unique = true;
      if (Sensors::settings != nullptr && !slug.empty()) {
        // The caller holds Sensors::mutex while checking configured sensor slugs.
        for (uint8_t id = 0; id <= Sensors::getMaxSensorId(); id++) {
          if (&Sensors::settings[id] != &dst && slug == Sensors::settings[id].slug) {
            unique = false;
            break;
          }
        }
      }

      if (unique) {
        result.accepted = true;
        strcpy(dst.name, value.c_str());
        strcpy(dst.slug, slug.c_str());
        result.changed = true;
      }

    } else if (value.size() < sizeof(dst.name)) {
      result.accepted = true;
    }
  }

  if (dst.enabled && !dst.slug[0]) {
    dst.enabled = false;
    result.changed = true;
  }

  // purpose
  if (src[S_PURPOSE].is<uint8_t>()) {
    const auto value = src[S_PURPOSE].as<uint8_t>();

    switch (value) {
      case static_cast<uint8_t>(Sensors::Purpose::OUTDOOR_TEMP):
      case static_cast<uint8_t>(Sensors::Purpose::INDOOR_TEMP):
      case static_cast<uint8_t>(Sensors::Purpose::HEATING_TEMP):
      case static_cast<uint8_t>(Sensors::Purpose::HEATING_RETURN_TEMP):
      case static_cast<uint8_t>(Sensors::Purpose::DHW_TEMP):
      case static_cast<uint8_t>(Sensors::Purpose::DHW_RETURN_TEMP):
      case static_cast<uint8_t>(Sensors::Purpose::DHW_FLOW_RATE):
      case static_cast<uint8_t>(Sensors::Purpose::EXHAUST_TEMP):
      case static_cast<uint8_t>(Sensors::Purpose::MODULATION_LEVEL):

      case static_cast<uint8_t>(Sensors::Purpose::NUMBER):
      case static_cast<uint8_t>(Sensors::Purpose::POWER_FACTOR):
      case static_cast<uint8_t>(Sensors::Purpose::POWER):
      case static_cast<uint8_t>(Sensors::Purpose::FAN_SPEED):
      case static_cast<uint8_t>(Sensors::Purpose::CO2):
      case static_cast<uint8_t>(Sensors::Purpose::PRESSURE):
      case static_cast<uint8_t>(Sensors::Purpose::HUMIDITY):
      case static_cast<uint8_t>(Sensors::Purpose::TEMPERATURE):
      case static_cast<uint8_t>(Sensors::Purpose::NOT_CONFIGURED):
        result.accepted = true;
        if (static_cast<uint8_t>(dst.purpose) != value) {
          dst.purpose = static_cast<Sensors::Purpose>(value);
          result.changed = true;
        }
        break;

      default:
        break;
    }
  }

  // type
  if (src[S_TYPE].is<uint8_t>()) {
    const auto value = src[S_TYPE].as<uint8_t>();

    switch (value) {
      case static_cast<uint8_t>(Sensors::Type::OT_OUTDOOR_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_HEATING_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_HEATING_RETURN_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_DHW_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_DHW_TEMP2):
      case static_cast<uint8_t>(Sensors::Type::OT_DHW_FLOW_RATE):
      case static_cast<uint8_t>(Sensors::Type::OT_CH2_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_EXHAUST_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_HEAT_EXCHANGER_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_PRESSURE):
      case static_cast<uint8_t>(Sensors::Type::OT_MODULATION_LEVEL):
      case static_cast<uint8_t>(Sensors::Type::OT_CURRENT_POWER):
      case static_cast<uint8_t>(Sensors::Type::OT_EXHAUST_CO2):
      case static_cast<uint8_t>(Sensors::Type::OT_EXHAUST_FAN_SPEED):
      case static_cast<uint8_t>(Sensors::Type::OT_SUPPLY_FAN_SPEED):
      case static_cast<uint8_t>(Sensors::Type::OT_SOLAR_STORAGE_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_SOLAR_COLLECTOR_TEMP):
      case static_cast<uint8_t>(Sensors::Type::OT_FAN_SPEED_SETPOINT):
      case static_cast<uint8_t>(Sensors::Type::OT_FAN_SPEED_CURRENT):

      case static_cast<uint8_t>(Sensors::Type::OT_BURNER_STARTS):
      case static_cast<uint8_t>(Sensors::Type::OT_DHW_BURNER_STARTS):
      case static_cast<uint8_t>(Sensors::Type::OT_HEATING_PUMP_STARTS):
      case static_cast<uint8_t>(Sensors::Type::OT_DHW_PUMP_STARTS):
      case static_cast<uint8_t>(Sensors::Type::OT_BURNER_HOURS):
      case static_cast<uint8_t>(Sensors::Type::OT_DHW_BURNER_HOURS):
      case static_cast<uint8_t>(Sensors::Type::OT_HEATING_PUMP_HOURS):
      case static_cast<uint8_t>(Sensors::Type::OT_DHW_PUMP_HOURS):
      case static_cast<uint8_t>(Sensors::Type::OT_COOLING_HOURS):

      case static_cast<uint8_t>(Sensors::Type::NTC_10K_TEMP):
      case static_cast<uint8_t>(Sensors::Type::DALLAS_TEMP):
      case static_cast<uint8_t>(Sensors::Type::BLUETOOTH):
      case static_cast<uint8_t>(Sensors::Type::DHT11):
      case static_cast<uint8_t>(Sensors::Type::DHT22):
      case static_cast<uint8_t>(Sensors::Type::HEATING_SETPOINT_TEMP):
      case static_cast<uint8_t>(Sensors::Type::MANUAL):
      case static_cast<uint8_t>(Sensors::Type::NOT_CONFIGURED):
        result.accepted = true;
        if (static_cast<uint8_t>(dst.type) != value) {
          dst.type = static_cast<Sensors::Type>(value);
          result.changed = true;
        }
        break;

      default:
        break;
    }
  }

  // gpio
  if (!src[S_GPIO].isNull()) {
    if (dst.type != Sensors::Type::DALLAS_TEMP && dst.type != Sensors::Type::NTC_10K_TEMP &&
        dst.type != Sensors::Type::DHT11 && dst.type != Sensors::Type::DHT22) {
      if (dst.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.gpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_GPIO].is<JsonString>() && src[S_GPIO].as<JsonString>().size() == 0) {
      result.accepted = true;
      if (dst.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.gpio = GPIO_IS_NOT_CONFIGURED;
        result.changed = true;
      }

    } else if (src[S_GPIO].is<uint8_t>()) {
      const auto value = src[S_GPIO].as<uint8_t>();

      if (value == GPIO_IS_NOT_CONFIGURED
          || (GPIO_IS_VALID_INPUT(value)
              && (dst.type == Sensors::Type::NTC_10K_TEMP || GPIO_IS_VALID_OUTPUT(value)))) {
        result.accepted = true;
        if (value != dst.gpio) {
          dst.gpio = value;
          result.changed = true;
        }
      }
    }
  }

  // address
  if (src[S_ADDRESS].is<JsonString>()) {
    const JsonString value = src[S_ADDRESS].as<JsonString>();

    if (dst.type == Sensors::Type::DALLAS_TEMP) {
      uint8_t tmp[8];
      int parsed = sscanf(
        value.c_str(),
        "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &tmp[0], &tmp[1], &tmp[2], &tmp[3],
        &tmp[4], &tmp[5], &tmp[6], &tmp[7]
      );

      if (parsed == 8) {
        result.accepted = true;
        for (uint8_t i = 0; i < parsed; i++) {
          if (dst.address[i] != tmp[i]) {
            dst.address[i] = tmp[i];
            result.changed = true;
          }
        }

      } else {
        // reset
        for (uint8_t i = 0; i < sizeof(dst.address); i++) {
          dst.address[i] = 0x00;
        }

        result.changed = true;
      }

    } else if (dst.type == Sensors::Type::BLUETOOTH) {
      uint8_t tmp[6];
      int parsed = sscanf(
        value.c_str(),
        "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &tmp[0], &tmp[1], &tmp[2],
        &tmp[3], &tmp[4], &tmp[5]
      );

      if (parsed == 6) {
        result.accepted = true;
        for (uint8_t i = 0; i < parsed; i++) {
          if (dst.address[i] != tmp[i]) {
            dst.address[i] = tmp[i];
            result.changed = true;
          }
        }

      } else {
        // reset
        for (uint8_t i = 0; i < sizeof(dst.address); i++) {
          dst.address[i] = 0x00;
        }

        result.changed = true;
      }
    }
  }

  // offset
  if (src[S_OFFSET].is<float>()) {
    const auto value = src[S_OFFSET].as<float>();

    if (value >= -20.0f && value <= 20.0f) {
      result.accepted = true;
      if (isFloatDifferent(value, dst.offset, 0.0001f)) {
        dst.offset = roundf(value, 2);
        result.changed = true;
      }
    }
  }

  // factor
  if (src[S_FACTOR].is<float>()) {
    const auto value = src[S_FACTOR].as<float>();

    if (value > 0.09f && value <= 100.0f) {
      result.accepted = true;
      if (isFloatDifferent(value, dst.factor, 0.0001f)) {
        dst.factor = roundf(value, 3);
        result.changed = true;
      }
    }
  }

  // filtering
  if (src[S_FILTERING].is<bool>()) {
    const auto value = src[S_FILTERING].as<bool>();
    result.accepted = true;

    if (value != dst.filtering) {
      dst.filtering = value;
      result.changed = true;
    }
  }

  // filtering factor
  if (src[S_FILTERING_FACTOR].is<float>()) {
    const auto value = src[S_FILTERING_FACTOR].as<float>();

    if (value >= 0.001f && value <= 1.0f) {
      result.accepted = true;
      if (isFloatDifferent(value, dst.filteringFactor, 0.0001f)) {
        dst.filteringFactor = roundf(value, 3);
        result.changed = true;
      }
    }
  }

  return result;
}

// Sensor measurements

JsonVariant toJson(const Sensors::Settings& sSensor, const Sensors::Result& rSensor, JsonVariant dst) {
  dst[S_CONNECTED] = rSensor.connected;
  dst[S_SIGNAL_QUALITY] = rSensor.signalQuality;

  if (sSensor.type == Sensors::Type::BLUETOOTH) {
    dst[S_TEMPERATURE] = roundf(convertTemp(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::TEMPERATURE)], UnitSystem::METRIC, settings.system.unitSystem), 3);
    dst[S_HUMIDITY] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::HUMIDITY)], 3);
    dst[S_BATTERY] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::BATTERY)], 1);
    dst[S_RSSI] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::RSSI)], 0);

  } else if (sSensor.type == Sensors::Type::DHT11 || sSensor.type == Sensors::Type::DHT22) {
    dst[S_TEMPERATURE] = roundf(convertTemp(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::TEMPERATURE)], UnitSystem::METRIC, settings.system.unitSystem), 3);
    dst[S_HUMIDITY] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::HUMIDITY)], 3);

  } else {
    dst[S_VALUE] = roundf(Sensors::convertValue(sSensor, Sensors::ValueType::PRIMARY, rSensor.values[static_cast<uint8_t>(Sensors::ValueType::PRIMARY)], UnitSystem::METRIC, settings.system.unitSystem), 3);
  }
  return dst;
}

JsonVariant toJson(const uint8_t sensorId, JsonVariant dst) {
  if (const auto snapshot = Sensors::getSnapshot(sensorId)) {
    return toJson(snapshot->settings, snapshot->result, dst);
  }
  return dst;
}

// Runtime variables and actions

JsonVariant toJson(const Variables& src, JsonVariant dst) {
  auto slave = dst[S_SLAVE].to<JsonObject>();
  slave[S_MEMBER_ID] = src.slave.memberId;
  slave[S_FLAGS] = src.slave.flags;
  slave[S_TYPE] = src.slave.type;
  slave[S_APP_VERSION] = src.slave.appVersion;
  slave[S_PROTOCOL_VERSION] = src.slave.protocolVersion;
  slave[S_CONNECTED] = src.slave.connected;
  slave[S_FLAME] = src.slave.flame;

  auto sCooling = slave[S_COOLING].to<JsonObject>();
  sCooling[S_ACTIVE] = src.slave.cooling.active;
  sCooling[S_SETPOINT] = src.slave.cooling.setpoint;

  auto sModulation = slave[S_MODULATION].to<JsonObject>();
  sModulation[S_MIN] = src.slave.modulation.min;
  sModulation[S_MAX] = src.slave.modulation.max;

  auto sPower = slave[S_POWER].to<JsonObject>();
  sPower[S_MIN] = roundf(src.slave.power.min, 2);
  sPower[S_MAX] = roundf(src.slave.power.max, 2);

  auto sHeating = slave[S_HEATING].to<JsonObject>();
  sHeating[S_ACTIVE] = src.slave.heating.active;
  sHeating[S_MIN_TEMP] = convertTemp(src.slave.heating.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
  sHeating[S_MAX_TEMP] = convertTemp(src.slave.heating.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);

  auto sDhw = slave[S_DHW].to<JsonObject>();
  sDhw[S_ACTIVE] = src.slave.dhw.active;
  sDhw[S_MIN_TEMP] = convertTemp(src.slave.dhw.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
  sDhw[S_MAX_TEMP] = convertTemp(src.slave.dhw.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);

  auto sFault = slave[S_FAULT].to<JsonObject>();
  sFault[S_ACTIVE] = src.slave.fault.active;
  sFault[S_CODE] = src.slave.fault.code;

  auto sDiag = slave[S_DIAG].to<JsonObject>();
  sDiag[S_ACTIVE] = src.slave.diag.active;
  sDiag[S_CODE] = src.slave.diag.code;


  auto master = dst[S_MASTER].to<JsonObject>();
  auto mHeating = master[S_HEATING].to<JsonObject>();
  mHeating[S_ENABLED] = src.master.heating.enabled;
  mHeating[S_BLOCKING] = src.master.heating.blocking;
  mHeating[S_INDOOR_TEMP_CONTROL] = src.master.heating.indoorTempControl;
  mHeating[S_OVERHEAT] = src.master.heating.overheat;
  mHeating[S_FREEZING] = src.master.heating.freezing;
  mHeating[S_SETPOINT_TEMP] = roundf((src.master.heating.setpointTemp == 0 ? 0 : convertTemp(src.master.heating.setpointTemp, UnitSystem::METRIC, settings.system.unitSystem)), 2);
  mHeating[S_TARGET_TEMP] = roundf((src.master.heating.targetTemp == 0 ? 0 : convertTemp(src.master.heating.targetTemp, UnitSystem::METRIC, settings.system.unitSystem)), 2);
  mHeating[S_CURRENT_TEMP] = roundf(convertTemp(src.master.heating.currentTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  mHeating[S_RETURN_TEMP] = roundf(convertTemp(src.master.heating.returnTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  mHeating[S_INDOOR_TEMP] = roundf(convertTemp(src.master.heating.indoorTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  mHeating[S_OUTDOOR_TEMP] = roundf(convertTemp(src.master.heating.outdoorTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  mHeating[S_MIN_TEMP] = roundf(convertTemp(src.master.heating.minTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  mHeating[S_MAX_TEMP] = roundf(convertTemp(src.master.heating.maxTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);

  auto mDhw = master[S_DHW].to<JsonObject>();
  mDhw[S_ENABLED] = src.master.dhw.enabled;
  mDhw[S_OVERHEAT] = src.master.dhw.overheat;
  mDhw[S_TARGET_TEMP] = roundf((src.master.dhw.targetTemp == 0 ? 0 : convertTemp(src.master.dhw.targetTemp, UnitSystem::METRIC, settings.system.unitSystem)), 2);
  mDhw[S_CURRENT_TEMP] = roundf(convertTemp(src.master.dhw.currentTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  mDhw[S_RETURN_TEMP] = roundf(convertTemp(src.master.dhw.returnTemp, UnitSystem::METRIC, settings.system.unitSystem), 2);
  mDhw[S_MIN_TEMP] = convertTemp(settings.dhw.minTemp, UnitSystem::METRIC, settings.system.unitSystem);
  mDhw[S_MAX_TEMP] = convertTemp(settings.dhw.maxTemp, UnitSystem::METRIC, settings.system.unitSystem);

  master[S_NETWORK][S_CONNECTED] = isNetworkConnected();
  master[S_NETWORK][S_RSSI] = src.network.rssi;
  master[S_MQTT][S_CONNECTED] = isMqttConnected();
  master[S_EMERGENCY][S_STATE] = src.emergency.state;
  master[S_EXTERNAL_PUMP][S_STATE] = src.externalPump.state;

  auto mCascadeControl = master[S_CASCADE_CONTROL].to<JsonObject>();
  mCascadeControl[S_INPUT] = src.cascadeControl.input;
  mCascadeControl[S_OUTPUT] = src.cascadeControl.output;

  master[S_UPTIME] = millis() / 1000;
  return dst;
}

static SettingResult applyAction(Variables& dst, std::string_view action, bool value) {
  SettingResult result;
  if (action == "restart") {
    if (value) {
      restartTask.resume(15000);
      result.changed = true;
    }

    result.accepted = true;
    return result;
  }

  if (action == "reset_fault") {
    if (value && !dst.actions.resetFault) {
      dst.actions.resetFault = true;
      result.changed = true;
    }

    result.accepted = true;
    return result;
  }

  if (action == "reset_diagnostic") {
    if (value && !dst.actions.resetDiagnostic) {
      dst.actions.resetDiagnostic = true;
      result.changed = true;
    }

    result.accepted = true;
    return result;
  }

  return result;
}

bool applyAction(std::string_view action, bool value) {
  if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
    return false;
  }

  return applyAction(vars, action, value).accepted;
}

SettingResult fromJson(const JsonVariantConst src, Variables& dst) {
  SettingResult result;

  // Admit the whole request before one of its actions schedules a restart.
  if (vars.states.upgrading || restartTask.scheduled() || restartTask.active()) {
    return result;
  }

  if (src[S_ACTIONS][S_RESTART].is<bool>()) {
    const auto actionResult = applyAction(dst, "restart", src[S_ACTIONS][S_RESTART].as<bool>());
    result.changed = actionResult.changed || result.changed;
    result.accepted = actionResult.accepted || result.accepted;
  }

  if (src[S_ACTIONS][S_RESET_FAULT].is<bool>()) {
    const auto actionResult = applyAction(dst, "reset_fault", src[S_ACTIONS][S_RESET_FAULT].as<bool>());
    result.changed = actionResult.changed || result.changed;
    result.accepted = actionResult.accepted || result.accepted;
  }

  if (src[S_ACTIONS][S_RESET_DIAGNOSTIC].is<bool>()) {
    const auto actionResult = applyAction(dst, "reset_diagnostic", src[S_ACTIONS][S_RESET_DIAGNOSTIC].as<bool>());
    result.changed = actionResult.changed || result.changed;
    result.accepted = actionResult.accepted || result.accepted;
  }

  return result;
}
