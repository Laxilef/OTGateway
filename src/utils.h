#include <Arduino.h>

String getChipId(const char* prefix = nullptr, const char* suffix = nullptr) {
  String chipId;
  chipId.reserve(
    6
    + (prefix != nullptr ? strlen(prefix) : 0)
    + (suffix != nullptr ? strlen(suffix) : 0)
  );
  
  if (prefix != nullptr) {
    chipId.concat(prefix);
  }

  uint32_t cid = 0;
  #if defined(ARDUINO_ARCH_ESP8266)
  cid = ESP.getChipId();
  #elif defined(ARDUINO_ARCH_ESP32)
  // https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/ChipID/GetChipID/GetChipID.ino
  for (uint8_t i = 0; i < 17; i = i + 8) {
    cid |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  #endif

  chipId += String(cid, HEX);

  if (suffix != nullptr) {
    chipId.concat(suffix);
  }

  chipId.trim();
  return chipId;
}

bool isLeapYear(short year) {
  if (year % 4 != 0) {
    return false;
  }

  if (year % 100 != 0) {
    return true;
  }

  return (year % 400) == 0;
}

// convert UTC tm time to time_t epoch time
// source: https://github.com/cyberman54/ESP32-Paxcounter/blob/master/src/timekeeper.cpp
time_t mkgmtime(const struct tm *ptm) {
  const int SecondsPerMinute = 60;
  const int SecondsPerHour = 3600;
  const int SecondsPerDay = 86400;
  const int DaysOfMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  time_t secs = 0;
  // tm_year is years since 1900
  int year = ptm->tm_year + 1900;
  for (int y = 1970; y < year; ++y) {
    secs += (isLeapYear(y) ? 366 : 365) * SecondsPerDay;
  }
  // tm_mon is month from 0..11
  for (int m = 0; m < ptm->tm_mon; ++m) {
    secs += DaysOfMonth[m] * SecondsPerDay;
    if (m == 1 && isLeapYear(year))
      secs += SecondsPerDay;
  }
  secs += (ptm->tm_mday - 1) * SecondsPerDay;
  secs += ptm->tm_hour * SecondsPerHour;
  secs += ptm->tm_min * SecondsPerMinute;
  secs += ptm->tm_sec;
  return secs;
}

inline bool isDigit(const char* ptr) {
  char* endPtr;
  auto tmp = strtol(ptr, &endPtr, 10);
  return *endPtr == 0;
}

inline float liter2gallon(float value) {
  return value / 4.546091879f;
}

inline float gallon2liter(float value) {
  return value * 4.546091879f;
}

float convertVolume(float value, const UnitSystem unitFrom, const UnitSystem unitTo) {
  if (unitFrom == UnitSystem::METRIC && unitTo == UnitSystem::IMPERIAL) {
    value = liter2gallon(value);

  } else if (unitFrom == UnitSystem::IMPERIAL && unitTo == UnitSystem::METRIC) {
    value = gallon2liter(value);
  }

  return value;
}

inline float bar2psi(float value) {
  return value * 14.5038f;
}

inline float psi2bar(float value) {
  return value / 14.5038f;
}

float convertPressure(float value, const UnitSystem unitFrom, const UnitSystem unitTo) {
  if (unitFrom == UnitSystem::METRIC && unitTo == UnitSystem::IMPERIAL) {
    value = bar2psi(value);

  } else if (unitFrom == UnitSystem::IMPERIAL && unitTo == UnitSystem::METRIC) {
    value = psi2bar(value);
  }

  return value;
}

inline float c2f(float value) {
  return (9.0f / 5.0f) * value + 32.0f;
}

inline float f2c(float value) {
  return (value - 32.0f) * (5.0f / 9.0f);
}

float convertTemp(float value, const UnitSystem unitFrom, const UnitSystem unitTo) {
  if (unitFrom == UnitSystem::METRIC && unitTo == UnitSystem::IMPERIAL) {
    value = c2f(value);

  } else if (unitFrom == UnitSystem::IMPERIAL && unitTo == UnitSystem::METRIC) {
    value = f2c(value);
  }

  return value;
}

inline bool isValidTemp(const float value, UnitSystem unit, const float min = 0.1f, const float max = 99.9f, const UnitSystem minMaxUnit = UnitSystem::METRIC) {
  return value >= convertTemp(min, minMaxUnit, unit) && value <= convertTemp(max, minMaxUnit, unit);
}

float roundf(float value, uint8_t decimals = 2) {
  if (decimals == 0) {
    return (int)(value + 0.5f);

  } else if (abs(value) < 0.00000001f) {
    return 0.0f;
  }

  float multiplier = pow10(decimals);
  value += 0.5f / multiplier * (value < 0.0f ? -1.0f : 1.0f);
  return (int)(value * multiplier) / multiplier;
}

inline size_t getTotalHeap() {
  #if defined(ARDUINO_ARCH_ESP32)
  return ESP.getHeapSize();
  #elif defined(ARDUINO_ARCH_ESP8266)
  return 81920;
  #else
  return 99999;
  #endif
}

size_t getFreeHeap(bool getMinValue = false) {
  #if defined(ARDUINO_ARCH_ESP32)
  return getMinValue ? ESP.getMinFreeHeap() : ESP.getFreeHeap();

  #elif defined(ARDUINO_ARCH_ESP8266)
  static size_t minValue = 0;
  size_t value = ESP.getFreeHeap();

  if (value < minValue || minValue == 0) {
    minValue = value;
  }

  return getMinValue ? minValue : value;
  #else
  return 0;
  #endif
}

size_t getMaxFreeBlockHeap(bool getMinValue = false) {
  static size_t minValue = 0;
  size_t value = 0;

  #if defined(ARDUINO_ARCH_ESP32)
  value = ESP.getMaxAllocHeap();

  size_t minHeapValue = getFreeHeap(true);
  if (minHeapValue < minValue || minValue == 0) {
    minValue = minHeapValue;
  }
  #elif defined(ARDUINO_ARCH_ESP8266)
  value = ESP.getMaxFreeBlockSize();
  #endif

  if (value < minValue || minValue == 0) {
    minValue = value;
  }

  return getMinValue ? minValue : value;
}

inline uint8_t getHeapFrag() {
  return 100 - getMaxFreeBlockHeap() * 100.0 / getFreeHeap();
}

String getResetReason() {
  String value;

  #if defined(ARDUINO_ARCH_ESP8266)
  value = ESP.getResetReason();
  #elif defined(ARDUINO_ARCH_ESP32)
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      value = F("Reset due to power-on event");
      break;

    case ESP_RST_EXT:
      value = F("Reset by external pin");
      break;

    case ESP_RST_SW:
      value = F("Software reset via esp_restart");
      break;

    case ESP_RST_PANIC:
      value = F("Software reset due to exception/panic");
      break;

    case ESP_RST_INT_WDT:
      value = F("Reset (software or hardware) due to interrupt watchdog");
      break;

    case ESP_RST_TASK_WDT:
      value = F("Reset due to task watchdog");
      break;

    case ESP_RST_WDT:
      value = F("Reset due to other watchdogs");
      break;

    case ESP_RST_DEEPSLEEP:
      value = F("Reset after exiting deep sleep mode");
      break;

    case ESP_RST_BROWNOUT:
      value = F("Brownout reset (software or hardware)");
      break;

    case ESP_RST_SDIO:
      value = F("Reset over SDIO");
      break;

    case ESP_RST_UNKNOWN:
    default:
      value = F("unknown");
      break;

  }
  #else
  value = F("unknown");
  #endif

  return value;
}

template <class T>
void arr2str(String& str, T arr[], size_t length) {
  char buffer[12];
  for (size_t i = 0; i < length; i++) {
    auto addr = arr[i];
    if (!addr) {
      continue;
    }

    sprintf(buffer, "0x%08X ", addr);
    str.concat(buffer);
  }

  str.trim();
}

void networkSettingsToJson(const NetworkSettings& src, JsonVariant dst) {
  dst[FPSTR(S_HOSTNAME)] = src.hostname;

  dst[FPSTR(S_USE_DHCP)] = src.useDhcp;
  dst[FPSTR(S_STATIC_CONFIG)][FPSTR(S_IP)] = src.staticConfig.ip;
  dst[FPSTR(S_STATIC_CONFIG)][FPSTR(S_GATEWAY)] = src.staticConfig.gateway;
  dst[FPSTR(S_STATIC_CONFIG)][FPSTR(S_SUBNET)] = src.staticConfig.subnet;
  dst[FPSTR(S_STATIC_CONFIG)][FPSTR(S_DNS)] = src.staticConfig.dns;

  dst[FPSTR(S_AP)][FPSTR(S_SSID)] = src.ap.ssid;
  dst[FPSTR(S_AP)][FPSTR(S_PASSWORD)] = src.ap.password;
  dst[FPSTR(S_AP)][FPSTR(S_CHANNEL)] = src.ap.channel;

  dst[FPSTR(S_STA)][FPSTR(S_SSID)] = src.sta.ssid;
  dst[FPSTR(S_STA)][FPSTR(S_PASSWORD)] = src.sta.password;
  dst[FPSTR(S_STA)][FPSTR(S_CHANNEL)] = src.sta.channel;
}

bool jsonToNetworkSettings(const JsonVariantConst src, NetworkSettings& dst) {
  bool changed = false;

  // hostname
  if (!src[FPSTR(S_HOSTNAME)].isNull()) {
    String value = src[FPSTR(S_HOSTNAME)].as<String>();

    if (value.length() < sizeof(dst.hostname) && !value.equals(dst.hostname)) {
      strcpy(dst.hostname, value.c_str());
      changed = true;
    }
  }

  // use dhcp
  if (src[FPSTR(S_USE_DHCP)].is<bool>()) {
    dst.useDhcp = src[FPSTR(S_USE_DHCP)].as<bool>();
    changed = true;
  }


  // static config
  if (!src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_IP)].isNull()) {
    String value = src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_IP)].as<String>();

    if (value.length() < sizeof(dst.staticConfig.ip) && !value.equals(dst.staticConfig.ip)) {
      strcpy(dst.staticConfig.ip, value.c_str());
      changed = true;
    }
  }

  if (!src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_GATEWAY)].isNull()) {
    String value = src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_GATEWAY)].as<String>();

    if (value.length() < sizeof(dst.staticConfig.gateway) && !value.equals(dst.staticConfig.gateway)) {
      strcpy(dst.staticConfig.gateway, value.c_str());
      changed = true;
    }
  }

  if (!src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_SUBNET)].isNull()) {
    String value = src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_SUBNET)].as<String>();

    if (value.length() < sizeof(dst.staticConfig.subnet) && !value.equals(dst.staticConfig.subnet)) {
      strcpy(dst.staticConfig.subnet, value.c_str());
      changed = true;
    }
  }

  if (!src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_DNS)].isNull()) {
    String value = src[FPSTR(S_STATIC_CONFIG)][FPSTR(S_DNS)].as<String>();

    if (value.length() < sizeof(dst.staticConfig.dns) && !value.equals(dst.staticConfig.dns)) {
      strcpy(dst.staticConfig.dns, value.c_str());
      changed = true;
    }
  }


  // ap
  if (!src[FPSTR(S_AP)][FPSTR(S_SSID)].isNull()) {
    String value = src[FPSTR(S_AP)][FPSTR(S_SSID)].as<String>();

    if (value.length() < sizeof(dst.ap.ssid) && !value.equals(dst.ap.ssid)) {
      strcpy(dst.ap.ssid, value.c_str());
      changed = true;
    }
  }

  if (!src[FPSTR(S_AP)][FPSTR(S_PASSWORD)].isNull()) {
    String value = src[FPSTR(S_AP)][FPSTR(S_PASSWORD)].as<String>();

    if (value.length() < sizeof(dst.ap.password) && !value.equals(dst.ap.password)) {
      strcpy(dst.ap.password, value.c_str());
      changed = true;
    }
  }

  if (!src[FPSTR(S_AP)][FPSTR(S_CHANNEL)].isNull()) {
    unsigned char value = src[FPSTR(S_AP)][FPSTR(S_CHANNEL)].as<unsigned char>();

    if (value >= 0 && value < 12) {
      dst.ap.channel = value;
      changed = true;
    }
  }


  // sta
  if (!src[FPSTR(S_STA)][FPSTR(S_SSID)].isNull()) {
    String value = src[FPSTR(S_STA)][FPSTR(S_SSID)].as<String>();

    if (value.length() < sizeof(dst.sta.ssid) && !value.equals(dst.sta.ssid)) {
      strcpy(dst.sta.ssid, value.c_str());
      changed = true;
    }
  }

  if (!src[FPSTR(S_STA)][FPSTR(S_PASSWORD)].isNull()) {
    String value = src[FPSTR(S_STA)][FPSTR(S_PASSWORD)].as<String>();

    if (value.length() < sizeof(dst.sta.password) && !value.equals(dst.sta.password)) {
      strcpy(dst.sta.password, value.c_str());
      changed = true;
    }
  }

  if (!src[FPSTR(S_STA)][FPSTR(S_CHANNEL)].isNull()) {
    unsigned char value = src[FPSTR(S_STA)][FPSTR(S_CHANNEL)].as<unsigned char>();

    if (value >= 0 && value < 12) {
      dst.sta.channel = value;
      changed = true;
    }
  }

  return changed;
}

void settingsToJson(const Settings& src, JsonVariant dst, bool safe = false) {
  if (!safe) {
    auto system = dst[FPSTR(S_SYSTEM)].to<JsonObject>();
    system[FPSTR(S_LOG_LEVEL)] = static_cast<uint8_t>(src.system.logLevel);

    auto serial = system[FPSTR(S_SERIAL)].to<JsonObject>();
    serial[FPSTR(S_ENABLED)] = src.system.serial.enabled;
    serial[FPSTR(S_BAUDRATE)] = src.system.serial.baudrate;

    auto telnet = system[FPSTR(S_TELNET)].to<JsonObject>();
    telnet[FPSTR(S_ENABLED)] = src.system.telnet.enabled;
    telnet[FPSTR(S_PORT)] = src.system.telnet.port;

    auto ntp = system[FPSTR(S_NTP)].to<JsonObject>();
    ntp[FPSTR(S_SERVER)] = src.system.ntp.server;
    ntp[FPSTR(S_TIMEZONE)] = src.system.ntp.timezone;

    system[FPSTR(S_UNIT_SYSTEM)] = static_cast<uint8_t>(src.system.unitSystem);
    system[FPSTR(S_STATUS_LED_GPIO)] = src.system.statusLedGpio;

    auto portal = dst[FPSTR(S_PORTAL)].to<JsonObject>();
    portal[FPSTR(S_AUTH)] = src.portal.auth;
    portal[FPSTR(S_LOGIN)] = src.portal.login;
    portal[FPSTR(S_PASSWORD)] = src.portal.password;
    portal[FPSTR(S_MDNS)] = src.portal.mdns;

    auto opentherm = dst[FPSTR(S_OPENTHERM)].to<JsonObject>();
    opentherm[FPSTR(S_UNIT_SYSTEM)] = static_cast<uint8_t>(src.opentherm.unitSystem);
    opentherm[FPSTR(S_IN_GPIO)] = src.opentherm.inGpio;
    opentherm[FPSTR(S_OUT_GPIO)] = src.opentherm.outGpio;
    opentherm[FPSTR(S_RX_LED_GPIO)] = src.opentherm.rxLedGpio;
    opentherm[FPSTR(S_MEMBER_ID)] = src.opentherm.memberId;
    opentherm[FPSTR(S_FLAGS)] = src.opentherm.flags;
    opentherm[FPSTR(S_MIN_POWER)] = roundf(src.opentherm.minPower, 2);
    opentherm[FPSTR(S_MAX_POWER)] = roundf(src.opentherm.maxPower, 2);

    auto otOptions = opentherm[FPSTR(S_OPTIONS)].to<JsonObject>();
    otOptions[FPSTR(S_DHW_SUPPORT)] = src.opentherm.options.dhwSupport;
    otOptions[FPSTR(S_COOLING_SUPPORT)] = src.opentherm.options.coolingSupport;
    otOptions[FPSTR(S_SUMMER_WINTER_MODE)] = src.opentherm.options.summerWinterMode;
    otOptions[FPSTR(S_HEATING_STATE_TO_SUMMER_WINTER_MODE)] = src.opentherm.options.heatingStateToSummerWinterMode;
    otOptions[FPSTR(S_CH2_ALWAYS_ENABLED)] = src.opentherm.options.ch2AlwaysEnabled;
    otOptions[FPSTR(S_HEATING_TO_CH2)] = src.opentherm.options.heatingToCh2;
    otOptions[FPSTR(S_DHW_TO_CH2)] = src.opentherm.options.dhwToCh2;
    otOptions[FPSTR(S_DHW_BLOCKING)] = src.opentherm.options.dhwBlocking;
    otOptions[FPSTR(S_DHW_STATE_AS_DHW_BLOCKING)] = src.opentherm.options.dhwStateAsDhwBlocking;
    otOptions[FPSTR(S_MAX_TEMP_SYNC_WITH_TARGET_TEMP)] = src.opentherm.options.maxTempSyncWithTargetTemp;
    otOptions[FPSTR(S_GET_MIN_MAX_TEMP)] = src.opentherm.options.getMinMaxTemp;
    otOptions[FPSTR(S_IGNORE_DIAG_STATE)] = src.opentherm.options.ignoreDiagState;
    otOptions[FPSTR(S_AUTO_FAULT_RESET)] = src.opentherm.options.autoFaultReset;
    otOptions[FPSTR(S_AUTO_DIAG_RESET)] = src.opentherm.options.autoDiagReset;
    otOptions[FPSTR(S_SET_DATE_AND_TIME)] = src.opentherm.options.setDateAndTime;
    otOptions[FPSTR(S_NATIVE_HEATING_CONTROL)] = src.opentherm.options.nativeHeatingControl;
    otOptions[FPSTR(S_IMMERGAS_FIX)] = src.opentherm.options.immergasFix;

    auto mqtt = dst[FPSTR(S_MQTT)].to<JsonObject>();
    mqtt[FPSTR(S_ENABLED)] = src.mqtt.enabled;
    mqtt[FPSTR(S_SERVER)] = src.mqtt.server;
    mqtt[FPSTR(S_PORT)] = src.mqtt.port;
    mqtt[FPSTR(S_USER)] = src.mqtt.user;
    mqtt[FPSTR(S_PASSWORD)] = src.mqtt.password;
    mqtt[FPSTR(S_PREFIX)] = src.mqtt.prefix;
    mqtt[FPSTR(S_INTERVAL)] = src.mqtt.interval;
    mqtt[FPSTR(S_HOME_ASSISTANT_DISCOVERY)] = src.mqtt.homeAssistantDiscovery;

    auto emergency = dst[FPSTR(S_EMERGENCY)].to<JsonObject>();
    emergency[FPSTR(S_TARGET)] = roundf(src.emergency.target, 2);
    emergency[FPSTR(S_TRESHOLD_TIME)] = src.emergency.tresholdTime;
  }
  
  auto heating = dst[FPSTR(S_HEATING)].to<JsonObject>();
  heating[FPSTR(S_ENABLED)] = src.heating.enabled;
  heating[FPSTR(S_TURBO)] = src.heating.turbo;
  heating[FPSTR(S_TARGET)] = roundf(src.heating.target, 2);
  heating[FPSTR(S_HYSTERESIS)] = roundf(src.heating.hysteresis, 3);
  heating[FPSTR(S_TURBO_FACTOR)] = roundf(src.heating.turboFactor, 3);
  heating[FPSTR(S_MIN_TEMP)] = src.heating.minTemp;
  heating[FPSTR(S_MAX_TEMP)] = src.heating.maxTemp;
  heating[FPSTR(S_MAX_MODULATION)] = src.heating.maxModulation;

  auto heatingOverheatProtection = heating[FPSTR(S_OVERHEAT_PROTECTION)].to<JsonObject>();
  heatingOverheatProtection[FPSTR(S_HIGH_TEMP)] = src.heating.overheatProtection.highTemp;
  heatingOverheatProtection[FPSTR(S_LOW_TEMP)] = src.heating.overheatProtection.lowTemp;

  auto freezeProtection = heating[FPSTR(S_FREEZE_PROTECTION)].to<JsonObject>();
  freezeProtection[FPSTR(S_LOW_TEMP)] = src.heating.freezeProtection.lowTemp;
  freezeProtection[FPSTR(S_THRESHOLD_TIME)] = src.heating.freezeProtection.thresholdTime;

  auto dhw = dst[FPSTR(S_DHW)].to<JsonObject>();
  dhw[FPSTR(S_ENABLED)] = src.dhw.enabled;
  dhw[FPSTR(S_TARGET)] = roundf(src.dhw.target, 1);
  dhw[FPSTR(S_MIN_TEMP)] = src.dhw.minTemp;
  dhw[FPSTR(S_MAX_TEMP)] = src.dhw.maxTemp;
  dhw[FPSTR(S_MAX_MODULATION)] = src.dhw.maxModulation;

  auto dhwOverheatProtection = dhw[FPSTR(S_OVERHEAT_PROTECTION)].to<JsonObject>();
  dhwOverheatProtection[FPSTR(S_HIGH_TEMP)] = src.dhw.overheatProtection.highTemp;
  dhwOverheatProtection[FPSTR(S_LOW_TEMP)] = src.dhw.overheatProtection.lowTemp;

  auto equitherm = dst[FPSTR(S_EQUITHERM)].to<JsonObject>();
  equitherm[FPSTR(S_ENABLED)] = src.equitherm.enabled;
  equitherm[FPSTR(S_N_FACTOR)] = roundf(src.equitherm.n_factor, 3);
  equitherm[FPSTR(S_K_FACTOR)] = roundf(src.equitherm.k_factor, 3);
  equitherm[FPSTR(S_T_FACTOR)] = roundf(src.equitherm.t_factor, 3);

  auto pid = dst[FPSTR(S_PID)].to<JsonObject>();
  pid[FPSTR(S_ENABLED)] = src.pid.enabled;
  pid[FPSTR(S_P_FACTOR)] = roundf(src.pid.p_factor, 3);
  pid[FPSTR(S_I_FACTOR)] = roundf(src.pid.i_factor, 4);
  pid[FPSTR(S_D_FACTOR)] = roundf(src.pid.d_factor, 1);
  pid[FPSTR(S_DT)] = src.pid.dt;
  pid[FPSTR(S_MIN_TEMP)] = src.pid.minTemp;
  pid[FPSTR(S_MAX_TEMP)] = src.pid.maxTemp;

  auto pidDeadband = pid[FPSTR(S_DEADBAND)].to<JsonObject>();
  pidDeadband[FPSTR(S_ENABLED)] = src.pid.deadband.enabled;
  pidDeadband[FPSTR(S_P_MULTIPLIER)] = src.pid.deadband.p_multiplier;
  pidDeadband[FPSTR(S_I_MULTIPLIER)] = src.pid.deadband.i_multiplier;
  pidDeadband[FPSTR(S_D_MULTIPLIER)] = src.pid.deadband.d_multiplier;
  pidDeadband[FPSTR(S_THRESHOLD_HIGH)] = src.pid.deadband.thresholdHigh;
  pidDeadband[FPSTR(S_THRESHOLD_LOW)] = src.pid.deadband.thresholdLow;

  if (!safe) {
    auto externalPump = dst[FPSTR(S_EXTERNAL_PUMP)].to<JsonObject>();
    externalPump[FPSTR(S_USE)] = src.externalPump.use;
    externalPump[FPSTR(S_GPIO)] = src.externalPump.gpio;
    externalPump[FPSTR(S_INVERT_STATE)] = src.externalPump.invertState;
    externalPump[FPSTR(S_POST_CIRCULATION_TIME)] = roundf(src.externalPump.postCirculationTime / 60, 0);
    externalPump[FPSTR(S_ANTI_STUCK_INTERVAL)] = roundf(src.externalPump.antiStuckInterval / 86400, 0);
    externalPump[FPSTR(S_ANTI_STUCK_TIME)] = roundf(src.externalPump.antiStuckTime / 60, 0);

    auto cascadeControlInput = dst[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)].to<JsonObject>();
    cascadeControlInput[FPSTR(S_ENABLED)] = src.cascadeControl.input.enabled;
    cascadeControlInput[FPSTR(S_GPIO)] = src.cascadeControl.input.gpio;
    cascadeControlInput[FPSTR(S_INVERT_STATE)] = src.cascadeControl.input.invertState;
    cascadeControlInput[FPSTR(S_THRESHOLD_TIME)] = src.cascadeControl.input.thresholdTime;

    auto cascadeControlOutput = dst[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)].to<JsonObject>();
    cascadeControlOutput[FPSTR(S_ENABLED)] = src.cascadeControl.output.enabled;
    cascadeControlOutput[FPSTR(S_GPIO)] = src.cascadeControl.output.gpio;
    cascadeControlOutput[FPSTR(S_INVERT_STATE)] = src.cascadeControl.output.invertState;
    cascadeControlOutput[FPSTR(S_THRESHOLD_TIME)] = src.cascadeControl.output.thresholdTime;
    cascadeControlOutput[FPSTR(S_ON_FAULT)] = src.cascadeControl.output.onFault;
    cascadeControlOutput[FPSTR(S_ON_LOSS_CONNECTION)] = src.cascadeControl.output.onLossConnection;
    cascadeControlOutput[FPSTR(S_ON_ENABLED_HEATING)] = src.cascadeControl.output.onEnabledHeating;
  }
}

inline void safeSettingsToJson(const Settings& src, JsonVariant dst) {
  settingsToJson(src, dst, true);
}

bool jsonToSettings(const JsonVariantConst src, Settings& dst, bool safe = false) {
  bool changed = false;

  if (!safe) {
    // system
    if (!src[FPSTR(S_SYSTEM)][FPSTR(S_LOG_LEVEL)].isNull()) {
      uint8_t value = src[FPSTR(S_SYSTEM)][FPSTR(S_LOG_LEVEL)].as<uint8_t>();

      if (value != dst.system.logLevel && value >= TinyLogger::Level::SILENT && value <= TinyLogger::Level::VERBOSE) {
        dst.system.logLevel = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_SYSTEM)][FPSTR(S_SERIAL)][FPSTR(S_ENABLED)].is<bool>()) {
      bool value = src[FPSTR(S_SYSTEM)][FPSTR(S_SERIAL)][FPSTR(S_ENABLED)].as<bool>();

      if (value != dst.system.serial.enabled) {
        dst.system.serial.enabled = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_SYSTEM)][FPSTR(S_SERIAL)][FPSTR(S_BAUDRATE)].isNull()) {
      unsigned int value = src[FPSTR(S_SYSTEM)][FPSTR(S_SERIAL)][FPSTR(S_BAUDRATE)].as<unsigned int>();

      if (value == 9600 || value == 19200 || value == 38400 || value == 57600 || value == 74880 || value == 115200) {
        if (value != dst.system.serial.baudrate) {
          dst.system.serial.baudrate = value;
          changed = true;
        }
      }
    }

    if (src[FPSTR(S_SYSTEM)][FPSTR(S_TELNET)][FPSTR(S_ENABLED)].is<bool>()) {
      bool value = src[FPSTR(S_SYSTEM)][FPSTR(S_TELNET)][FPSTR(S_ENABLED)].as<bool>();

      if (value != dst.system.telnet.enabled) {
        dst.system.telnet.enabled = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_SYSTEM)][FPSTR(S_TELNET)][FPSTR(S_PORT)].isNull()) {
      unsigned short value = src[FPSTR(S_SYSTEM)][FPSTR(S_TELNET)][FPSTR(S_PORT)].as<unsigned short>();

      if (value > 0 && value <= 65535 && value != dst.system.telnet.port) {
        dst.system.telnet.port = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_SYSTEM)][FPSTR(S_NTP)][FPSTR(S_SERVER)].isNull()) {
      String value = src[FPSTR(S_SYSTEM)][FPSTR(S_NTP)][FPSTR(S_SERVER)].as<String>();

      if (value.length() < sizeof(dst.system.ntp.server) && !value.equals(dst.system.ntp.server)) {
        strcpy(dst.system.ntp.server, value.c_str());
        changed = true;
      }
    }

    if (!src[FPSTR(S_SYSTEM)][FPSTR(S_NTP)][FPSTR(S_TIMEZONE)].isNull()) {
      String value = src[FPSTR(S_SYSTEM)][FPSTR(S_NTP)][FPSTR(S_TIMEZONE)].as<String>();

      if (value.length() < sizeof(dst.system.ntp.timezone) && !value.equals(dst.system.ntp.timezone)) {
        strcpy(dst.system.ntp.timezone, value.c_str());
        changed = true;
      }
    }

    if (!src[FPSTR(S_SYSTEM)][FPSTR(S_UNIT_SYSTEM)].isNull()) {
      uint8_t value = src[FPSTR(S_SYSTEM)][FPSTR(S_UNIT_SYSTEM)].as<unsigned char>();
      UnitSystem prevUnitSystem = dst.system.unitSystem;

      switch (value) {
        case static_cast<uint8_t>(UnitSystem::METRIC):
          if (dst.system.unitSystem != UnitSystem::METRIC) {
            dst.system.unitSystem = UnitSystem::METRIC;
            changed = true;
          }
          break;

        case static_cast<uint8_t>(UnitSystem::IMPERIAL):
          if (dst.system.unitSystem != UnitSystem::IMPERIAL) {
            dst.system.unitSystem = UnitSystem::IMPERIAL;
            changed = true;
          }
          break;

        default:
          break;
      }

      // convert temps
      if (dst.system.unitSystem != prevUnitSystem) {
        dst.emergency.target = convertTemp(dst.emergency.target, prevUnitSystem, dst.system.unitSystem);
        dst.heating.target = convertTemp(dst.heating.target, prevUnitSystem, dst.system.unitSystem);
        dst.heating.minTemp = convertTemp(dst.heating.minTemp, prevUnitSystem, dst.system.unitSystem);
        dst.heating.maxTemp = convertTemp(dst.heating.maxTemp, prevUnitSystem, dst.system.unitSystem);
        dst.dhw.target = convertTemp(dst.dhw.target, prevUnitSystem, dst.system.unitSystem);
        dst.dhw.minTemp = convertTemp(dst.dhw.minTemp, prevUnitSystem, dst.system.unitSystem);
        dst.dhw.maxTemp = convertTemp(dst.dhw.maxTemp, prevUnitSystem, dst.system.unitSystem);
        dst.pid.minTemp = convertTemp(dst.pid.minTemp, prevUnitSystem, dst.system.unitSystem);
        dst.pid.maxTemp = convertTemp(dst.pid.maxTemp, prevUnitSystem, dst.system.unitSystem);
      }
    }

    if (!src[FPSTR(S_SYSTEM)][FPSTR(S_STATUS_LED_GPIO)].isNull()) {
      if (src[FPSTR(S_SYSTEM)][FPSTR(S_STATUS_LED_GPIO)].is<JsonString>() && src[FPSTR(S_SYSTEM)][FPSTR(S_STATUS_LED_GPIO)].as<JsonString>().size() == 0) {
        if (dst.system.statusLedGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.system.statusLedGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src[FPSTR(S_SYSTEM)][FPSTR(S_STATUS_LED_GPIO)].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.system.statusLedGpio) {
          dst.system.statusLedGpio = value;
          changed = true;
        }
      }
    }


    // portal
    if (src[FPSTR(S_PORTAL)][FPSTR(S_AUTH)].is<bool>()) {
      bool value = src[FPSTR(S_PORTAL)][FPSTR(S_AUTH)].as<bool>();

      if (value != dst.portal.auth) {
        dst.portal.auth = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_PORTAL)][FPSTR(S_LOGIN)].isNull()) {
      String value = src[FPSTR(S_PORTAL)][FPSTR(S_LOGIN)].as<String>();

      if (value.length() < sizeof(dst.portal.login) && !value.equals(dst.portal.login)) {
        strcpy(dst.portal.login, value.c_str());
        changed = true;
      }
    }

    if (!src[FPSTR(S_PORTAL)][FPSTR(S_PASSWORD)].isNull()) {
      String value = src[FPSTR(S_PORTAL)][FPSTR(S_PASSWORD)].as<String>();

      if (value.length() < sizeof(dst.portal.password) && !value.equals(dst.portal.password)) {
        strcpy(dst.portal.password, value.c_str());
        changed = true;
      }
    }

    if (dst.portal.auth && (!strlen(dst.portal.login) || !strlen(dst.portal.password))) {
      dst.portal.auth = false;
      changed = true;
    }

    if (src[FPSTR(S_PORTAL)][FPSTR(S_MDNS)].is<bool>()) {
      bool value = src[FPSTR(S_PORTAL)][FPSTR(S_MDNS)].as<bool>();

      if (value != dst.portal.mdns) {
        dst.portal.mdns = value;
        changed = true;
      }
    }


    // opentherm
    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_UNIT_SYSTEM)].isNull()) {
      uint8_t value = src[FPSTR(S_OPENTHERM)][FPSTR(S_UNIT_SYSTEM)].as<unsigned char>();

      switch (value) {
        case static_cast<uint8_t>(UnitSystem::METRIC):
          if (dst.opentherm.unitSystem != UnitSystem::METRIC) {
            dst.opentherm.unitSystem = UnitSystem::METRIC;
            changed = true;
          }
          break;

        case static_cast<uint8_t>(UnitSystem::IMPERIAL):
          if (dst.opentherm.unitSystem != UnitSystem::IMPERIAL) {
            dst.opentherm.unitSystem = UnitSystem::IMPERIAL;
            changed = true;
          }
          break;

        default:
          break;
      }
    }

    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_IN_GPIO)].isNull()) {
      if (src[FPSTR(S_OPENTHERM)][FPSTR(S_IN_GPIO)].is<JsonString>() && src[FPSTR(S_OPENTHERM)][FPSTR(S_IN_GPIO)].as<JsonString>().size() == 0) {
        if (dst.opentherm.inGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.opentherm.inGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src[FPSTR(S_OPENTHERM)][FPSTR(S_IN_GPIO)].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.opentherm.inGpio) {
          dst.opentherm.inGpio = value;
          changed = true;
        }
      }
    }

    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_OUT_GPIO)].isNull()) {
      if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OUT_GPIO)].is<JsonString>() && src[FPSTR(S_OPENTHERM)][FPSTR(S_OUT_GPIO)].as<JsonString>().size() == 0) {
        if (dst.opentherm.outGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.opentherm.outGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OUT_GPIO)].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.opentherm.outGpio) {
          dst.opentherm.outGpio = value;
          changed = true;
        }
      }
    }

    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_RX_LED_GPIO)].isNull()) {
      if (src[FPSTR(S_OPENTHERM)][FPSTR(S_RX_LED_GPIO)].is<JsonString>() && src[FPSTR(S_OPENTHERM)][FPSTR(S_RX_LED_GPIO)].as<JsonString>().size() == 0) {
        if (dst.opentherm.rxLedGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.opentherm.rxLedGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src[FPSTR(S_OPENTHERM)][FPSTR(S_RX_LED_GPIO)].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.opentherm.rxLedGpio) {
          dst.opentherm.rxLedGpio = value;
          changed = true;
        }
      }
    }

    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_MEMBER_ID)].isNull()) {
      auto value = src[FPSTR(S_OPENTHERM)][FPSTR(S_MEMBER_ID)].as<uint8_t>();

      if (value != dst.opentherm.memberId) {
        dst.opentherm.memberId = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_FLAGS)].isNull()) {
      auto value = src[FPSTR(S_OPENTHERM)][FPSTR(S_FLAGS)].as<uint8_t>();

      if (value != dst.opentherm.flags) {
        dst.opentherm.flags = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_MIN_POWER)].isNull()) {
      float value = src[FPSTR(S_OPENTHERM)][FPSTR(S_MIN_POWER)].as<float>();

      if (value >= 0 && value <= 1000 && fabsf(value - dst.opentherm.minPower) > 0.0001f) {
        dst.opentherm.minPower = roundf(value, 2);
        changed = true;
      }
    }

    if (!src[FPSTR(S_OPENTHERM)][FPSTR(S_MAX_POWER)].isNull()) {
      float value = src[FPSTR(S_OPENTHERM)][FPSTR(S_MAX_POWER)].as<float>();

      if (value >= 0 && value <= 1000 && fabsf(value - dst.opentherm.maxPower) > 0.0001f) {
        dst.opentherm.maxPower = roundf(value, 2);
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_SUPPORT)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_SUPPORT)].as<bool>();

      if (value != dst.opentherm.options.dhwSupport) {
        dst.opentherm.options.dhwSupport = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_COOLING_SUPPORT)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_COOLING_SUPPORT)].as<bool>();

      if (value != dst.opentherm.options.coolingSupport) {
        dst.opentherm.options.coolingSupport = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_SUMMER_WINTER_MODE)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_SUMMER_WINTER_MODE)].as<bool>();

      if (value != dst.opentherm.options.summerWinterMode) {
        dst.opentherm.options.summerWinterMode = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_HEATING_STATE_TO_SUMMER_WINTER_MODE)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_HEATING_STATE_TO_SUMMER_WINTER_MODE)].as<bool>();

      if (value != dst.opentherm.options.heatingStateToSummerWinterMode) {
        dst.opentherm.options.heatingStateToSummerWinterMode = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_CH2_ALWAYS_ENABLED)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_CH2_ALWAYS_ENABLED)].as<bool>();

      if (value != dst.opentherm.options.ch2AlwaysEnabled) {
        dst.opentherm.options.ch2AlwaysEnabled = value;

        if (dst.opentherm.options.ch2AlwaysEnabled) {
          dst.opentherm.options.heatingToCh2 = false;
          dst.opentherm.options.dhwToCh2 = false;
        }

        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_HEATING_TO_CH2)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_HEATING_TO_CH2)].as<bool>();

      if (value != dst.opentherm.options.heatingToCh2) {
        dst.opentherm.options.heatingToCh2 = value;

        if (dst.opentherm.options.heatingToCh2) {
          dst.opentherm.options.ch2AlwaysEnabled = false;
          dst.opentherm.options.dhwToCh2 = false;
        }

        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_TO_CH2)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_TO_CH2)].as<bool>();

      if (value != dst.opentherm.options.dhwToCh2) {
        dst.opentherm.options.dhwToCh2 = value;

        if (dst.opentherm.options.dhwToCh2) {
          dst.opentherm.options.ch2AlwaysEnabled = false;
          dst.opentherm.options.heatingToCh2 = false;
        }

        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_BLOCKING)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_BLOCKING)].as<bool>();

      if (value != dst.opentherm.options.dhwBlocking) {
        dst.opentherm.options.dhwBlocking = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_STATE_AS_DHW_BLOCKING)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_DHW_STATE_AS_DHW_BLOCKING)].as<bool>();

      if (value != dst.opentherm.options.dhwStateAsDhwBlocking) {
        dst.opentherm.options.dhwStateAsDhwBlocking = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_MAX_TEMP_SYNC_WITH_TARGET_TEMP)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_MAX_TEMP_SYNC_WITH_TARGET_TEMP)].as<bool>();

      if (value != dst.opentherm.options.maxTempSyncWithTargetTemp) {
        dst.opentherm.options.maxTempSyncWithTargetTemp = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_GET_MIN_MAX_TEMP)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_GET_MIN_MAX_TEMP)].as<bool>();

      if (value != dst.opentherm.options.getMinMaxTemp) {
        dst.opentherm.options.getMinMaxTemp = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_IGNORE_DIAG_STATE)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_IGNORE_DIAG_STATE)].as<bool>();

      if (value != dst.opentherm.options.ignoreDiagState) {
        dst.opentherm.options.ignoreDiagState = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_AUTO_FAULT_RESET)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_AUTO_FAULT_RESET)].as<bool>();

      if (value != dst.opentherm.options.autoFaultReset) {
        dst.opentherm.options.autoFaultReset = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_AUTO_DIAG_RESET)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_AUTO_DIAG_RESET)].as<bool>();

      if (value != dst.opentherm.options.autoDiagReset) {
        dst.opentherm.options.autoDiagReset = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_SET_DATE_AND_TIME)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_SET_DATE_AND_TIME)].as<bool>();

      if (value != dst.opentherm.options.setDateAndTime) {
        dst.opentherm.options.setDateAndTime = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_NATIVE_HEATING_CONTROL)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_NATIVE_HEATING_CONTROL)].as<bool>();

      if (value != dst.opentherm.options.nativeHeatingControl) {
        dst.opentherm.options.nativeHeatingControl = value;

        if (value) {
          dst.equitherm.enabled = false;
          dst.pid.enabled = false;
        }

        changed = true;
      }
    }

    if (src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_IMMERGAS_FIX)].is<bool>()) {
      bool value = src[FPSTR(S_OPENTHERM)][FPSTR(S_OPTIONS)][FPSTR(S_IMMERGAS_FIX)].as<bool>();

      if (value != dst.opentherm.options.immergasFix) {
        dst.opentherm.options.immergasFix = value;
        changed = true;
      }
    }


    // mqtt
    if (src[FPSTR(S_MQTT)][FPSTR(S_ENABLED)].is<bool>()) {
      bool value = src[FPSTR(S_MQTT)][FPSTR(S_ENABLED)].as<bool>();

      if (value != dst.mqtt.enabled) {
        dst.mqtt.enabled = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_MQTT)][FPSTR(S_SERVER)].isNull()) {
      String value = src[FPSTR(S_MQTT)][FPSTR(S_SERVER)].as<String>();

      if (value.length() < sizeof(dst.mqtt.server) && !value.equals(dst.mqtt.server)) {
        strcpy(dst.mqtt.server, value.c_str());
        changed = true;
      }
    }

    if (!src[FPSTR(S_MQTT)][FPSTR(S_PORT)].isNull()) {
      unsigned short value = src[FPSTR(S_MQTT)][FPSTR(S_PORT)].as<unsigned short>();

      if (value > 0 && value <= 65535 && value != dst.mqtt.port) {
        dst.mqtt.port = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_MQTT)][FPSTR(S_USER)].isNull()) {
      String value = src[FPSTR(S_MQTT)][FPSTR(S_USER)].as<String>();

      if (value.length() < sizeof(dst.mqtt.user) && !value.equals(dst.mqtt.user)) {
        strcpy(dst.mqtt.user, value.c_str());
        changed = true;
      }
    }

    if (!src[FPSTR(S_MQTT)][FPSTR(S_PASSWORD)].isNull()) {
      String value = src[FPSTR(S_MQTT)][FPSTR(S_PASSWORD)].as<String>();

      if (value.length() < sizeof(dst.mqtt.password) && !value.equals(dst.mqtt.password)) {
        strcpy(dst.mqtt.password, value.c_str());
        changed = true;
      }
    }

    if (!src[FPSTR(S_MQTT)][FPSTR(S_PREFIX)].isNull()) {
      String value = src[FPSTR(S_MQTT)][FPSTR(S_PREFIX)].as<String>();

      if (value.length() < sizeof(dst.mqtt.prefix) && !value.equals(dst.mqtt.prefix)) {
        strcpy(dst.mqtt.prefix, value.c_str());
        changed = true;
      }
    }

    if (!src[FPSTR(S_MQTT)][FPSTR(S_INTERVAL)].isNull()) {
      unsigned short value = src[FPSTR(S_MQTT)][FPSTR(S_INTERVAL)].as<unsigned short>();

      if (value >= 3 && value <= 60 && value != dst.mqtt.interval) {
        dst.mqtt.interval = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_MQTT)][FPSTR(S_HOME_ASSISTANT_DISCOVERY)].is<bool>()) {
      bool value = src[FPSTR(S_MQTT)][FPSTR(S_HOME_ASSISTANT_DISCOVERY)].as<bool>();

      if (value != dst.mqtt.homeAssistantDiscovery) {
        dst.mqtt.homeAssistantDiscovery = value;
        changed = true;
      }
    }


    // emergency
    if (!src[FPSTR(S_EMERGENCY)][FPSTR(S_TRESHOLD_TIME)].isNull()) {
      unsigned short value = src[FPSTR(S_EMERGENCY)][FPSTR(S_TRESHOLD_TIME)].as<unsigned short>();

      if (value >= 60 && value <= 1800 && value != dst.emergency.tresholdTime) {
        dst.emergency.tresholdTime = value;
        changed = true;
      }
    }
  }


  // equitherm
  if (src[FPSTR(S_EQUITHERM)][FPSTR(S_ENABLED)].is<bool>()) {
    bool value = src[FPSTR(S_EQUITHERM)][FPSTR(S_ENABLED)].as<bool>();

    if (!dst.opentherm.options.nativeHeatingControl) {
      if (value != dst.equitherm.enabled) {
        dst.equitherm.enabled = value;
        changed = true;
      }

    } else if (dst.equitherm.enabled) {
      dst.equitherm.enabled = false;
      changed = true;
    }
  }

  if (!src[FPSTR(S_EQUITHERM)][FPSTR(S_N_FACTOR)].isNull()) {
    float value = src[FPSTR(S_EQUITHERM)][FPSTR(S_N_FACTOR)].as<float>();

    if (value > 0 && value <= 10 && fabsf(value - dst.equitherm.n_factor) > 0.0001f) {
      dst.equitherm.n_factor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src[FPSTR(S_EQUITHERM)][FPSTR(S_K_FACTOR)].isNull()) {
    float value = src[FPSTR(S_EQUITHERM)][FPSTR(S_K_FACTOR)].as<float>();

    if (value >= 0 && value <= 10 && fabsf(value - dst.equitherm.k_factor) > 0.0001f) {
      dst.equitherm.k_factor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src[FPSTR(S_EQUITHERM)][FPSTR(S_T_FACTOR)].isNull()) {
    float value = src[FPSTR(S_EQUITHERM)][FPSTR(S_T_FACTOR)].as<float>();

    if (value >= 0 && value <= 10 && fabsf(value - dst.equitherm.t_factor) > 0.0001f) {
      dst.equitherm.t_factor = roundf(value, 3);
      changed = true;
    }
  }


  // pid
  if (src[FPSTR(S_PID)][FPSTR(S_ENABLED)].is<bool>()) {
    bool value = src[FPSTR(S_PID)][FPSTR(S_ENABLED)].as<bool>();

    if (!dst.opentherm.options.nativeHeatingControl) {
      if (value != dst.pid.enabled) {
        dst.pid.enabled = value;
        changed = true;
      }

    } else if (dst.pid.enabled) {
      dst.pid.enabled = false;
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_P_FACTOR)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_P_FACTOR)].as<float>();

    if (value > 0 && value <= 1000 && fabsf(value - dst.pid.p_factor) > 0.0001f) {
      dst.pid.p_factor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_I_FACTOR)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_I_FACTOR)].as<float>();

    if (value >= 0 && value <= 100 && fabsf(value - dst.pid.i_factor) > 0.0001f) {
      dst.pid.i_factor = roundf(value, 4);
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_D_FACTOR)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_D_FACTOR)].as<float>();

    if (value >= 0 && value <= 100000 && fabsf(value - dst.pid.d_factor) > 0.0001f) {
      dst.pid.d_factor = roundf(value, 1);
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_DT)].isNull()) {
    unsigned short value = src[FPSTR(S_PID)][FPSTR(S_DT)].as<unsigned short>();

    if (value >= 30 && value <= 1800 && value != dst.pid.dt) {
      dst.pid.dt = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_MIN_TEMP)].isNull()) {
    short value = src[FPSTR(S_PID)][FPSTR(S_MIN_TEMP)].as<short>();

    if (isValidTemp(value, dst.system.unitSystem, dst.equitherm.enabled ? -99.9f : 0.0f) && value != dst.pid.minTemp) {
      dst.pid.minTemp = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_MAX_TEMP)].isNull()) {
    short value = src[FPSTR(S_PID)][FPSTR(S_MAX_TEMP)].as<short>();

    if (isValidTemp(value, dst.system.unitSystem) && value != dst.pid.maxTemp) {
      dst.pid.maxTemp = value;
      changed = true;
    }
  }

  if (dst.pid.maxTemp < dst.pid.minTemp) {
    dst.pid.maxTemp = dst.pid.minTemp;
    changed = true;
  }

  if (src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_ENABLED)].is<bool>()) {
    bool value = src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_ENABLED)].as<bool>();

    if (value != dst.pid.deadband.enabled) {
      dst.pid.deadband.enabled = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_P_MULTIPLIER)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_P_MULTIPLIER)].as<float>();

    if (value >= 0 && value <= 1 && fabsf(value - dst.pid.deadband.p_multiplier) > 0.0001f) {
      dst.pid.deadband.p_multiplier = roundf(value, 3);
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_I_MULTIPLIER)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_I_MULTIPLIER)].as<float>();

    if (value >= 0 && value <= 1 && fabsf(value - dst.pid.deadband.i_multiplier) > 0.0001f) {
      dst.pid.deadband.i_multiplier = roundf(value, 3);
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_D_MULTIPLIER)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_D_MULTIPLIER)].as<float>();

    if (value >= 0 && value <= 1 && fabsf(value - dst.pid.deadband.d_multiplier) > 0.0001f) {
      dst.pid.deadband.d_multiplier = roundf(value, 3);
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_THRESHOLD_HIGH)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_THRESHOLD_HIGH)].as<float>();

    if (value >= 0.0f && value <= 5.0f && fabsf(value - dst.pid.deadband.thresholdHigh) > 0.0001f) {
      dst.pid.deadband.thresholdHigh = roundf(value, 2);
      changed = true;
    }
  }

  if (!src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_THRESHOLD_LOW)].isNull()) {
    float value = src[FPSTR(S_PID)][FPSTR(S_DEADBAND)][FPSTR(S_THRESHOLD_LOW)].as<float>();

    if (value >= 0.0f && value <= 5.0f && fabsf(value - dst.pid.deadband.thresholdLow) > 0.0001f) {
      dst.pid.deadband.thresholdLow = roundf(value, 2);
      changed = true;
    }
  }


  // heating
  if (src[FPSTR(S_HEATING)][FPSTR(S_ENABLED)].is<bool>()) {
    bool value = src[FPSTR(S_HEATING)][FPSTR(S_ENABLED)].as<bool>();

    if (value != dst.heating.enabled) {
      dst.heating.enabled = value;
      changed = true;
    }
  }

  if (src[FPSTR(S_HEATING)][FPSTR(S_TURBO)].is<bool>()) {
    bool value = src[FPSTR(S_HEATING)][FPSTR(S_TURBO)].as<bool>();

    if (value != dst.heating.turbo) {
      dst.heating.turbo = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_HYSTERESIS)].isNull()) {
    float value = src[FPSTR(S_HEATING)][FPSTR(S_HYSTERESIS)].as<float>();

    if (value >= 0.0f && value <= 15.0f && fabsf(value - dst.heating.hysteresis) > 0.0001f) {
      dst.heating.hysteresis = roundf(value, 2);
      changed = true;
    }
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_TURBO_FACTOR)].isNull()) {
    float value = src[FPSTR(S_HEATING)][FPSTR(S_TURBO_FACTOR)].as<float>();

    if (value >= 1.5f && value <= 10.0f && fabsf(value - dst.heating.turboFactor) > 0.0001f) {
      dst.heating.turboFactor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_MIN_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_HEATING)][FPSTR(S_MIN_TEMP)].as<unsigned char>();

    if (value != dst.heating.minTemp && value >= vars.slave.heating.minTemp && value < vars.slave.heating.maxTemp && value != dst.heating.maxTemp) {
      dst.heating.minTemp = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_MAX_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_HEATING)][FPSTR(S_MAX_TEMP)].as<unsigned char>();

    if (value != dst.heating.maxTemp && value > vars.slave.heating.minTemp && value <= vars.slave.heating.maxTemp && value != dst.heating.minTemp) {
      dst.heating.maxTemp = value;
      changed = true;
    }
  }

  if (dst.heating.maxTemp < dst.heating.minTemp) {
    dst.heating.maxTemp = dst.heating.minTemp;
    changed = true;
  }


  if (!src[FPSTR(S_HEATING)][FPSTR(S_MAX_MODULATION)].isNull()) {
    unsigned char value = src[FPSTR(S_HEATING)][FPSTR(S_MAX_MODULATION)].as<unsigned char>();

    if (value > 0 && value <= 100 && value != dst.heating.maxModulation) {
      dst.heating.maxModulation = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_HIGH_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_HEATING)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_HIGH_TEMP)].as<unsigned char>();

    if (isValidTemp(value, dst.system.unitSystem, 0.0f, 100.0f) && value != dst.heating.overheatProtection.highTemp) {
      dst.heating.overheatProtection.highTemp = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_LOW_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_HEATING)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_LOW_TEMP)].as<unsigned char>();

    if (isValidTemp(value, dst.system.unitSystem, 0.0f, 99.0f) && value != dst.heating.overheatProtection.lowTemp) {
      dst.heating.overheatProtection.lowTemp = value;
      changed = true;
    }
  }

  if (dst.heating.overheatProtection.highTemp < dst.heating.overheatProtection.lowTemp) {
    dst.heating.overheatProtection.highTemp = dst.heating.overheatProtection.lowTemp;
    changed = true;
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_FREEZE_PROTECTION)][FPSTR(S_LOW_TEMP)].isNull()) {
    unsigned short value = src[FPSTR(S_HEATING)][FPSTR(S_FREEZE_PROTECTION)][FPSTR(S_LOW_TEMP)].as<uint8_t>();

    if (isValidTemp(value, dst.system.unitSystem, 1, 30) && value != dst.heating.freezeProtection.lowTemp) {
      dst.heating.freezeProtection.lowTemp = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_HEATING)][FPSTR(S_FREEZE_PROTECTION)][FPSTR(S_THRESHOLD_TIME)].isNull()) {
    unsigned short value = src[FPSTR(S_HEATING)][FPSTR(S_FREEZE_PROTECTION)][FPSTR(S_THRESHOLD_TIME)].as<unsigned short>();

    if (value >= 30 && value <= 1800) {
      if (value != dst.heating.freezeProtection.thresholdTime) {
        dst.heating.freezeProtection.thresholdTime = value;
        changed = true;
      }
    }
  }


  // dhw
  if (src[FPSTR(S_DHW)][FPSTR(S_ENABLED)].is<bool>()) {
    bool value = src[FPSTR(S_DHW)][FPSTR(S_ENABLED)].as<bool>();

    if (value != dst.dhw.enabled) {
      dst.dhw.enabled = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_DHW)][FPSTR(S_MIN_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_DHW)][FPSTR(S_MIN_TEMP)].as<unsigned char>();

    if (value >= vars.slave.dhw.minTemp && value < vars.slave.dhw.maxTemp && value != dst.dhw.minTemp) {
      dst.dhw.minTemp = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_DHW)][FPSTR(S_MAX_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_DHW)][FPSTR(S_MAX_TEMP)].as<unsigned char>();

    if (value > vars.slave.dhw.minTemp && value <= vars.slave.dhw.maxTemp && value != dst.dhw.maxTemp) {
      dst.dhw.maxTemp = value;
      changed = true;
    }
  }

  if (dst.dhw.maxTemp < dst.dhw.minTemp) {
    dst.dhw.maxTemp = dst.dhw.minTemp;
    changed = true;
  }

  if (!src[FPSTR(S_DHW)][FPSTR(S_MAX_MODULATION)].isNull()) {
    unsigned char value = src[FPSTR(S_DHW)][FPSTR(S_MAX_MODULATION)].as<unsigned char>();

    if (value > 0 && value <= 100 && value != dst.dhw.maxModulation) {
      dst.dhw.maxModulation = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_DHW)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_HIGH_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_DHW)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_HIGH_TEMP)].as<unsigned char>();

    if (isValidTemp(value, dst.system.unitSystem, 0.0f, 100.0f) && value != dst.dhw.overheatProtection.highTemp) {
      dst.dhw.overheatProtection.highTemp = value;
      changed = true;
    }
  }

  if (!src[FPSTR(S_DHW)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_LOW_TEMP)].isNull()) {
    unsigned char value = src[FPSTR(S_DHW)][FPSTR(S_OVERHEAT_PROTECTION)][FPSTR(S_LOW_TEMP)].as<unsigned char>();

    if (isValidTemp(value, dst.system.unitSystem, 0.0f, 99.0f) && value != dst.dhw.overheatProtection.lowTemp) {
      dst.dhw.overheatProtection.lowTemp = value;
      changed = true;
    }
  }

  if (dst.dhw.overheatProtection.highTemp < dst.dhw.overheatProtection.lowTemp) {
    dst.dhw.overheatProtection.highTemp = dst.dhw.overheatProtection.lowTemp;
    changed = true;
  }


  if (!safe) {
    // external pump
    if (src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_USE)].is<bool>()) {
      bool value = src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_USE)].as<bool>();

      if (value != dst.externalPump.use) {
        dst.externalPump.use = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_GPIO)].isNull()) {
      if (src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_GPIO)].is<JsonString>() && src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_GPIO)].as<JsonString>().size() == 0) {
        if (dst.externalPump.gpio != GPIO_IS_NOT_CONFIGURED) {
          dst.externalPump.gpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_GPIO)].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.externalPump.gpio) {
          dst.externalPump.gpio = value;
          changed = true;
        }
      }
    }

    if (src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_INVERT_STATE)].is<bool>()) {
      bool value = src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_INVERT_STATE)].as<bool>();

      if (value != dst.externalPump.invertState) {
        dst.externalPump.invertState = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_POST_CIRCULATION_TIME)].isNull()) {
      unsigned short value = src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_POST_CIRCULATION_TIME)].as<unsigned short>();

      if (value >= 0 && value <= 120) {
        value = value * 60;

        if (value != dst.externalPump.postCirculationTime) {
          dst.externalPump.postCirculationTime = value;
          changed = true;
        }
      }
    }

    if (!src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_ANTI_STUCK_INTERVAL)].isNull()) {
      unsigned int value = src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_ANTI_STUCK_INTERVAL)].as<unsigned int>();

      if (value >= 0 && value <= 366) {
        value = value * 86400;

        if (value != dst.externalPump.antiStuckInterval) {
          dst.externalPump.antiStuckInterval = value;
          changed = true;
        }
      }
    }

    if (!src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_ANTI_STUCK_TIME)].isNull()) {
      unsigned short value = src[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_ANTI_STUCK_TIME)].as<unsigned short>();

      if (value >= 0 && value <= 20) {
        value = value * 60;

        if (value != dst.externalPump.antiStuckTime) {
          dst.externalPump.antiStuckTime = value;
          changed = true;
        }
      }
    }


    // cascade control
    if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_ENABLED)].is<bool>()) {
      bool value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_ENABLED)].as<bool>();

      if (value != dst.cascadeControl.input.enabled) {
        dst.cascadeControl.input.enabled = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_GPIO)].isNull()) {
      if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_GPIO)].is<JsonString>() && src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_GPIO)].as<JsonString>().size() == 0) {
        if (dst.cascadeControl.input.gpio != GPIO_IS_NOT_CONFIGURED) {
          dst.cascadeControl.input.gpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_GPIO)].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.cascadeControl.input.gpio) {
          dst.cascadeControl.input.gpio = value;
          changed = true;
        }
      }
    }

    if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_INVERT_STATE)].is<bool>()) {
      bool value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_INVERT_STATE)].as<bool>();

      if (value != dst.cascadeControl.input.invertState) {
        dst.cascadeControl.input.invertState = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_THRESHOLD_TIME)].isNull()) {
      unsigned short value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_INPUT)][FPSTR(S_THRESHOLD_TIME)].as<unsigned short>();

      if (value >= 5 && value <= 600) {
        if (value != dst.cascadeControl.input.thresholdTime) {
          dst.cascadeControl.input.thresholdTime = value;
          changed = true;
        }
      }
    }

    if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ENABLED)].is<bool>()) {
      bool value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ENABLED)].as<bool>();

      if (value != dst.cascadeControl.output.enabled) {
        dst.cascadeControl.output.enabled = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_GPIO)].isNull()) {
      if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_GPIO)].is<JsonString>() && src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_GPIO)].as<JsonString>().size() == 0) {
        if (dst.cascadeControl.output.gpio != GPIO_IS_NOT_CONFIGURED) {
          dst.cascadeControl.output.gpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_GPIO)].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.cascadeControl.output.gpio) {
          dst.cascadeControl.output.gpio = value;
          changed = true;
        }
      }
    }

    if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_INVERT_STATE)].is<bool>()) {
      bool value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_INVERT_STATE)].as<bool>();

      if (value != dst.cascadeControl.output.invertState) {
        dst.cascadeControl.output.invertState = value;
        changed = true;
      }
    }

    if (!src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_THRESHOLD_TIME)].isNull()) {
      unsigned short value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_THRESHOLD_TIME)].as<unsigned short>();

      if (value >= 5 && value <= 600) {
        if (value != dst.cascadeControl.output.thresholdTime) {
          dst.cascadeControl.output.thresholdTime = value;
          changed = true;
        }
      }
    }

    if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ON_FAULT)].is<bool>()) {
      bool value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ON_FAULT)].as<bool>();

      if (value != dst.cascadeControl.output.onFault) {
        dst.cascadeControl.output.onFault = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ON_LOSS_CONNECTION)].is<bool>()) {
      bool value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ON_LOSS_CONNECTION)].as<bool>();

      if (value != dst.cascadeControl.output.onLossConnection) {
        dst.cascadeControl.output.onLossConnection = value;
        changed = true;
      }
    }

    if (src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ON_ENABLED_HEATING)].is<bool>()) {
      bool value = src[FPSTR(S_CASCADE_CONTROL)][FPSTR(S_OUTPUT)][FPSTR(S_ON_ENABLED_HEATING)].as<bool>();

      if (value != dst.cascadeControl.output.onEnabledHeating) {
        dst.cascadeControl.output.onEnabledHeating = value;
        changed = true;
      }
    }
  }

  // force check emergency target
  {
    float value = !src[FPSTR(S_EMERGENCY)][FPSTR(S_TARGET)].isNull() ? src[FPSTR(S_EMERGENCY)][FPSTR(S_TARGET)].as<float>() : dst.emergency.target;
    bool noRegulators = !dst.opentherm.options.nativeHeatingControl;
    bool valid = isValidTemp(
      value,
      dst.system.unitSystem,
      noRegulators ? dst.heating.minTemp : THERMOSTAT_INDOOR_MIN_TEMP,
      noRegulators ? dst.heating.maxTemp : THERMOSTAT_INDOOR_MAX_TEMP,
      noRegulators ? dst.system.unitSystem : UnitSystem::METRIC
    );

    if (!valid) {
      value = convertTemp(
        noRegulators ? DEFAULT_HEATING_TARGET_TEMP : THERMOSTAT_INDOOR_DEFAULT_TEMP,
        UnitSystem::METRIC,
        dst.system.unitSystem
      );
    }

    if (fabsf(dst.emergency.target - value) > 0.0001f) {
      dst.emergency.target = roundf(value, 2);
      changed = true;
    }
  }

  // force check heating target
  {
    bool indoorTempControl = dst.equitherm.enabled || dst.pid.enabled || dst.opentherm.options.nativeHeatingControl;
    float minTemp = indoorTempControl ? THERMOSTAT_INDOOR_MIN_TEMP : dst.heating.minTemp;
    float maxTemp = indoorTempControl ? THERMOSTAT_INDOOR_MAX_TEMP : dst.heating.maxTemp;

    float value = !src[FPSTR(S_HEATING)][FPSTR(S_TARGET)].isNull() 
      ? src[FPSTR(S_HEATING)][FPSTR(S_TARGET)].as<float>() 
      : dst.heating.target;
    bool valid = isValidTemp(
      value,
      dst.system.unitSystem,
      minTemp,
      maxTemp,
      dst.system.unitSystem
    );

    if (!valid) {
      value = convertTemp(
        indoorTempControl ? THERMOSTAT_INDOOR_DEFAULT_TEMP : DEFAULT_HEATING_TARGET_TEMP,
        UnitSystem::METRIC,
        dst.system.unitSystem
      );
    }

    if (fabsf(dst.heating.target - value) > 0.0001f) {
      dst.heating.target = roundf(value, 2);
      changed = true;
    }
  }

  // force check dhw target
  {
    float value = !src[FPSTR(S_DHW)][FPSTR(S_TARGET)].isNull() 
      ? src[FPSTR(S_DHW)][FPSTR(S_TARGET)].as<float>() 
      : dst.dhw.target;
    bool valid = isValidTemp(
      value,
      dst.system.unitSystem,
      dst.dhw.minTemp,
      dst.dhw.maxTemp,
      dst.system.unitSystem
    );

    if (!valid) {
      value = convertTemp(DEFAULT_DHW_TARGET_TEMP, UnitSystem::METRIC, dst.system.unitSystem);
    }

    if (fabsf(dst.dhw.target - value) > 0.0001f) {
      dst.dhw.target = value;
      changed = true;
    }
  }

  return changed;
}

inline bool safeJsonToSettings(const JsonVariantConst src, Settings& dst) {
  return jsonToSettings(src, dst, true);
}

void sensorSettingsToJson(const uint8_t sensorId, const Sensors::Settings& src, JsonVariant dst) {
  dst[FPSTR(S_ID)] = sensorId;
  dst[FPSTR(S_ENABLED)] = src.enabled;
  dst[FPSTR(S_NAME)] = src.name;
  dst[FPSTR(S_PURPOSE)] = static_cast<uint8_t>(src.purpose);
  dst[FPSTR(S_TYPE)] = static_cast<uint8_t>(src.type);
  dst[FPSTR(S_GPIO)] = src.gpio;

  if (src.type == Sensors::Type::DALLAS_TEMP) {
    char addr[24];
    sprintf_P(
      addr,
      PSTR("%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx"),
      src.address[0], src.address[1], src.address[2], src.address[3],
      src.address[4], src.address[5], src.address[6], src.address[7]
    );
    dst[FPSTR(S_ADDRESS)] = String(addr);

  } else if (src.type == Sensors::Type::BLUETOOTH) {
    char addr[18];
    sprintf_P(
      addr,
      PSTR("%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx"),
      src.address[0], src.address[1], src.address[2],
      src.address[3], src.address[4], src.address[5]
    );
    dst[FPSTR(S_ADDRESS)] = String(addr);

  } else {
    dst[FPSTR(S_ADDRESS)] = "";
  }

  dst[FPSTR(S_OFFSET)] = roundf(src.offset, 3);
  dst[FPSTR(S_FACTOR)] = roundf(src.factor, 3);
  dst[FPSTR(S_FILTERING)] = src.filtering;
  dst[FPSTR(S_FILTERING_FACTOR)] = roundf(src.filteringFactor, 3);
}

bool jsonToSensorSettings(const uint8_t sensorId, const JsonVariantConst src, Sensors::Settings& dst) {
  if (sensorId > Sensors::getMaxSensorId()) {
    return false;
  }

  bool changed = false;

  // enabled
  if (src[FPSTR(S_ENABLED)].is<bool>()) {
    auto value = src[FPSTR(S_ENABLED)].as<bool>();

    if (value != dst.enabled) {
      dst.enabled = value;
      changed = true;
    }
  }

  // name
  if (!src[FPSTR(S_NAME)].isNull()) {
    auto value = src[FPSTR(S_NAME)].as<String>();
    Sensors::cleanName(value);

    if (value.length() < sizeof(dst.name) && !value.equals(dst.name)) {
      strcpy(dst.name, value.c_str());
      changed = true;
    }
  }

  // purpose
  if (!src[FPSTR(S_PURPOSE)].isNull()) {
    uint8_t value = src[FPSTR(S_PURPOSE)].as<uint8_t>();

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
        if (static_cast<uint8_t>(dst.purpose) != value) {
          dst.purpose = static_cast<Sensors::Purpose>(value);
          changed = true;
        }
        break;

      default:
        break;
    }
  }

  // type
  if (!src[FPSTR(S_TYPE)].isNull()) {
    uint8_t value = src[FPSTR(S_TYPE)].as<uint8_t>();

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

      case static_cast<uint8_t>(Sensors::Type::NTC_10K_TEMP):
      case static_cast<uint8_t>(Sensors::Type::DALLAS_TEMP):
      case static_cast<uint8_t>(Sensors::Type::BLUETOOTH):
      case static_cast<uint8_t>(Sensors::Type::HEATING_SETPOINT_TEMP):
      case static_cast<uint8_t>(Sensors::Type::MANUAL):
      case static_cast<uint8_t>(Sensors::Type::NOT_CONFIGURED):
        if (static_cast<uint8_t>(dst.type) != value) {
          dst.type = static_cast<Sensors::Type>(value);
          changed = true;
        }
        break;

      default:
        break;
    }
  }

  // gpio
  if (!src[FPSTR(S_GPIO)].isNull()) {
    if (dst.type != Sensors::Type::DALLAS_TEMP && dst.type != Sensors::Type::NTC_10K_TEMP) {
      if (dst.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }

    } else if (src[FPSTR(S_GPIO)].is<JsonString>() && src[FPSTR(S_GPIO)].as<JsonString>().size() == 0) {
      if (dst.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }

    } else {
      unsigned char value = src[FPSTR(S_GPIO)].as<unsigned char>();

      if (GPIO_IS_VALID(value) && value != dst.gpio) {
        dst.gpio = value;
        changed = true;
      }
    }
  }

  // address
  if (!src[FPSTR(S_ADDRESS)].isNull()) {
    String value = src[FPSTR(S_ADDRESS)].as<String>();

    if (dst.type == Sensors::Type::DALLAS_TEMP) {
      uint8_t tmp[8];
      int parsed = sscanf(
        value.c_str(),
        "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &tmp[0], &tmp[1], &tmp[2], &tmp[3],
        &tmp[4], &tmp[5], &tmp[6], &tmp[7]
      );

      if (parsed == 8) {
        for (uint8_t i = 0; i < parsed; i++) {
          if (dst.address[i] != tmp[i]) {
            dst.address[i] = tmp[i];
            changed = true;
          }
        }

      } else {
        // reset
        for (uint8_t i = 0; i < sizeof(dst.address); i++) {
          dst.address[i] = 0x00;
        }

        changed = true;
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
        for (uint8_t i = 0; i < parsed; i++) {
          if (dst.address[i] != tmp[i]) {
            dst.address[i] = tmp[i];
            changed = true;
          }
        }

      } else {
        // reset
        for (uint8_t i = 0; i < sizeof(dst.address); i++) {
          dst.address[i] = 0x00;
        }
        
        changed = true;
      }
    }
  }

  // offset
  if (!src[FPSTR(S_OFFSET)].isNull()) {
    float value = src[FPSTR(S_OFFSET)].as<float>();

    if (value >= -20.0f && value <= 20.0f && fabsf(value - dst.offset) > 0.0001f) {
      dst.offset = roundf(value, 2);
      changed = true;
    }
  }

  // factor
  if (!src[FPSTR(S_FACTOR)].isNull()) {
    float value = src[FPSTR(S_FACTOR)].as<float>();

    if (value > 0.09f && value <= 100.0f && fabsf(value - dst.factor) > 0.0001f) {
      dst.factor = roundf(value, 3);
      changed = true;
    }
  }

  // filtering
  if (src[FPSTR(S_FILTERING)].is<bool>()) {
    auto value = src[FPSTR(S_FILTERING)].as<bool>();

    if (value != dst.filtering) {
      dst.filtering = value;
      changed = true;
    }
  }

  // filtering factor
  if (!src[FPSTR(S_FILTERING_FACTOR)].isNull()) {
    float value = src[FPSTR(S_FILTERING_FACTOR)].as<float>();

    if (value > 0 && value <= 1 && fabsf(value - dst.filteringFactor) > 0.0001f) {
      dst.filteringFactor = roundf(value, 3);
      changed = true;
    }
  }

  return changed;
}

void sensorResultToJson(const uint8_t sensorId, JsonVariant dst) {
  if (!Sensors::isValidSensorId(sensorId)) {
    return;
  }

  auto& sSensor = Sensors::settings[sensorId];
  auto& rSensor = Sensors::results[sensorId];

  //dst[FPSTR(S_ID)] = sensorId;
  dst[FPSTR(S_CONNECTED)] = rSensor.connected;
  dst[FPSTR(S_SIGNAL_QUALITY)] = rSensor.signalQuality;

  if (sSensor.type == Sensors::Type::BLUETOOTH) {
    dst[FPSTR(S_TEMPERATURE)] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::TEMPERATURE)], 3);
    dst[FPSTR(S_HUMIDITY)] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::HUMIDITY)], 3);
    dst[FPSTR(S_BATTERY)] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::BATTERY)], 1);
    dst[FPSTR(S_RSSI)] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::RSSI)], 0);

  } else {
    dst[FPSTR(S_VALUE)] = roundf(rSensor.values[static_cast<uint8_t>(Sensors::ValueType::PRIMARY)], 3);
  }
}

bool jsonToSensorResult(const uint8_t sensorId, const JsonVariantConst src) {
  if (!Sensors::isValidSensorId(sensorId)) {
    return false;
  }

  auto& sSensor = Sensors::settings[sensorId];
  if (!sSensor.enabled || sSensor.type != Sensors::Type::MANUAL) {
    return false;
  }

  auto& dst = Sensors::results[sensorId];
  bool changed = false;

  // value
  if (!src[FPSTR(S_VALUE)].isNull()) {
    float value = src[FPSTR(S_VALUE)].as<float>();

    uint8_t vType = static_cast<uint8_t>(Sensors::ValueType::PRIMARY);
    if (fabsf(value - dst.values[vType]) > 0.0001f) {
      dst.values[vType] = roundf(value, 2);
      changed = true;
    }
  }

  return changed;
}

void varsToJson(const Variables& src, JsonVariant dst) {
  auto slave = dst[FPSTR(S_SLAVE)].to<JsonObject>();
  slave[FPSTR(S_MEMBER_ID)] = src.slave.memberId;
  slave[FPSTR(S_FLAGS)] = src.slave.flags;
  slave[FPSTR(S_TYPE)] = src.slave.type;
  slave[FPSTR(S_APP_VERSION)] = src.slave.appVersion;
  slave[FPSTR(S_PROTOCOL_VERSION)] = src.slave.appVersion;
  slave[FPSTR(S_CONNECTED)] = src.slave.connected;
  slave[FPSTR(S_FLAME)] = src.slave.flame;

  auto sCooling = slave[FPSTR(S_COOLING)].to<JsonObject>();
  sCooling[FPSTR(S_ACTIVE)] = src.slave.cooling.active;
  sCooling[FPSTR(S_SETPOINT)] = src.slave.cooling.setpoint;

  auto sModulation = slave[FPSTR(S_MODULATION)].to<JsonObject>();
  sModulation[FPSTR(S_MIN)] = src.slave.modulation.min;
  sModulation[FPSTR(S_MAX)] = src.slave.modulation.max;

  auto sPower = slave[FPSTR(S_POWER)].to<JsonObject>();
  sPower[FPSTR(S_MIN)] = roundf(src.slave.power.min, 2);
  sPower[FPSTR(S_MAX)] = roundf(src.slave.power.max, 2);

  auto sHeating = slave[FPSTR(S_HEATING)].to<JsonObject>();
  sHeating[FPSTR(S_ACTIVE)] = src.slave.heating.active;
  sHeating[FPSTR(S_MIN_TEMP)] = src.slave.heating.minTemp;
  sHeating[FPSTR(S_MAX_TEMP)] = src.slave.heating.maxTemp;

  auto sDhw = slave[FPSTR(S_DHW)].to<JsonObject>();
  sDhw[FPSTR(S_ACTIVE)] = src.slave.dhw.active;
  sDhw[FPSTR(S_MIN_TEMP)] = src.slave.dhw.minTemp;
  sDhw[FPSTR(S_MAX_TEMP)] = src.slave.dhw.maxTemp;

  auto sFault = slave[FPSTR(S_FAULT)].to<JsonObject>();
  sFault[FPSTR(S_ACTIVE)] = src.slave.fault.active;
  sFault[FPSTR(S_CODE)] = src.slave.fault.code;

  auto sDiag = slave[FPSTR(S_DIAG)].to<JsonObject>();
  sDiag[FPSTR(S_ACTIVE)] = src.slave.diag.active;
  sDiag[FPSTR(S_CODE)] = src.slave.diag.code;


  auto master = dst[FPSTR(S_MASTER)].to<JsonObject>();
  auto mHeating = master[FPSTR(S_HEATING)].to<JsonObject>();
  mHeating[FPSTR(S_ENABLED)] = src.master.heating.enabled;
  mHeating[FPSTR(S_BLOCKING)] = src.master.heating.blocking;
  mHeating[FPSTR(S_INDOOR_TEMP_CONTROL)] = src.master.heating.indoorTempControl;
  mHeating[FPSTR(S_OVERHEAT)] = src.master.heating.overheat;
  mHeating[FPSTR(S_SETPOINT_TEMP)] = roundf(src.master.heating.setpointTemp, 2);
  mHeating[FPSTR(S_TARGET_TEMP)] = roundf(src.master.heating.targetTemp, 2);
  mHeating[FPSTR(S_CURRENT_TEMP)] = roundf(src.master.heating.currentTemp, 2);
  mHeating[FPSTR(S_RETURN_TEMP)] = roundf(src.master.heating.returnTemp, 2);
  mHeating[FPSTR(S_INDOOR_TEMP)] = roundf(src.master.heating.indoorTemp, 2);
  mHeating[FPSTR(S_OUTDOOR_TEMP)] = roundf(src.master.heating.outdoorTemp, 2);
  mHeating[FPSTR(S_MIN_TEMP)] = roundf(src.master.heating.minTemp, 2);
  mHeating[FPSTR(S_MAX_TEMP)] = roundf(src.master.heating.maxTemp, 2);

  auto mDhw = master[FPSTR(S_DHW)].to<JsonObject>();
  mDhw[FPSTR(S_ENABLED)] = src.master.dhw.enabled;
  mDhw[FPSTR(S_OVERHEAT)] = src.master.dhw.overheat;
  mDhw[FPSTR(S_TARGET_TEMP)] = roundf(src.master.dhw.targetTemp, 2);
  mDhw[FPSTR(S_CURRENT_TEMP)] = roundf(src.master.dhw.currentTemp, 2);
  mDhw[FPSTR(S_RETURN_TEMP)] = roundf(src.master.dhw.returnTemp, 2);
  mDhw[FPSTR(S_MIN_TEMP)] = settings.dhw.minTemp;
  mDhw[FPSTR(S_MAX_TEMP)] = settings.dhw.maxTemp;

  master[FPSTR(S_NETWORK)][FPSTR(S_CONNECTED)] = src.network.connected;
  master[FPSTR(S_NETWORK)][FPSTR(S_RSSI)] = src.network.rssi;
  master[FPSTR(S_MQTT)][FPSTR(S_CONNECTED)] = src.mqtt.connected;
  master[FPSTR(S_EMERGENCY)][FPSTR(S_STATE)] = src.emergency.state;
  master[FPSTR(S_EXTERNAL_PUMP)][FPSTR(S_STATE)] = src.externalPump.state;

  auto mCascadeControl = master[FPSTR(S_CASCADE_CONTROL)].to<JsonObject>();
  mCascadeControl[FPSTR(S_INPUT)] = src.cascadeControl.input;
  mCascadeControl[FPSTR(S_OUTPUT)] = src.cascadeControl.output;

  master[FPSTR(S_UPTIME)] = millis() / 1000;
}

bool jsonToVars(const JsonVariantConst src, Variables& dst) {
  bool changed = false;

  // actions
  if (src[FPSTR(S_ACTIONS)][FPSTR(S_RESTART)].is<bool>() && src[FPSTR(S_ACTIONS)][FPSTR(S_RESTART)].as<bool>()) {
    dst.actions.restart = true;
  }

  if (src[FPSTR(S_ACTIONS)][FPSTR(S_RESET_FAULT)].is<bool>() && src[FPSTR(S_ACTIONS)][FPSTR(S_RESET_FAULT)].as<bool>()) {
    dst.actions.resetFault = true;
  }

  if (src[FPSTR(S_ACTIONS)][FPSTR(S_RESET_DIAGNOSTIC)].is<bool>() && src[FPSTR(S_ACTIONS)][FPSTR(S_RESET_DIAGNOSTIC)].as<bool>()) {
    dst.actions.resetDiagnostic = true;
  }

  return changed;
}