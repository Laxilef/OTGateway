#include <Arduino.h>

inline bool isDigit(const char* ptr) {
  char* endPtr;
  strtol(ptr, &endPtr, 10);
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
    return (int)(value + 0.5);

  } else if (abs(value) < 0.00000001) {
    return 0.0;
  }

  float multiplier = pow10(decimals);
  value += 0.5 / multiplier * (value < 0 ? -1 : 1);
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
  dst["hostname"] = src.hostname;

  dst["useDhcp"] = src.useDhcp;
  dst["staticConfig"]["ip"] = src.staticConfig.ip;
  dst["staticConfig"]["gateway"] = src.staticConfig.gateway;
  dst["staticConfig"]["subnet"] = src.staticConfig.subnet;
  dst["staticConfig"]["dns"] = src.staticConfig.dns;

  dst["ap"]["ssid"] = src.ap.ssid;
  dst["ap"]["password"] = src.ap.password;
  dst["ap"]["channel"] = src.ap.channel;

  dst["sta"]["ssid"] = src.sta.ssid;
  dst["sta"]["password"] = src.sta.password;
  dst["sta"]["channel"] = src.sta.channel;
}

bool jsonToNetworkSettings(const JsonVariantConst src, NetworkSettings& dst) {
  bool changed = false;

  // hostname
  if (!src["hostname"].isNull()) {
    String value = src["hostname"].as<String>();

    if (value.length() < sizeof(dst.hostname) && !value.equals(dst.hostname)) {
      strcpy(dst.hostname, value.c_str());
      changed = true;
    }
  }

  // use dhcp
  if (src["useDhcp"].is<bool>()) {
    dst.useDhcp = src["useDhcp"].as<bool>();
    changed = true;
  }


  // static config
  if (!src["staticConfig"]["ip"].isNull()) {
    String value = src["staticConfig"]["ip"].as<String>();

    if (value.length() < sizeof(dst.staticConfig.ip) && !value.equals(dst.staticConfig.ip)) {
      strcpy(dst.staticConfig.ip, value.c_str());
      changed = true;
    }
  }

  if (!src["staticConfig"]["gateway"].isNull()) {
    String value = src["staticConfig"]["gateway"].as<String>();

    if (value.length() < sizeof(dst.staticConfig.gateway) && !value.equals(dst.staticConfig.gateway)) {
      strcpy(dst.staticConfig.gateway, value.c_str());
      changed = true;
    }
  }

  if (!src["staticConfig"]["subnet"].isNull()) {
    String value = src["staticConfig"]["subnet"].as<String>();

    if (value.length() < sizeof(dst.staticConfig.subnet) && !value.equals(dst.staticConfig.subnet)) {
      strcpy(dst.staticConfig.subnet, value.c_str());
      changed = true;
    }
  }

  if (!src["staticConfig"]["dns"].isNull()) {
    String value = src["staticConfig"]["dns"].as<String>();

    if (value.length() < sizeof(dst.staticConfig.dns) && !value.equals(dst.staticConfig.dns)) {
      strcpy(dst.staticConfig.dns, value.c_str());
      changed = true;
    }
  }


  // ap
  if (!src["ap"]["ssid"].isNull()) {
    String value = src["ap"]["ssid"].as<String>();

    if (value.length() < sizeof(dst.ap.ssid) && !value.equals(dst.ap.ssid)) {
      strcpy(dst.ap.ssid, value.c_str());
      changed = true;
    }
  }

  if (!src["ap"]["password"].isNull()) {
    String value = src["ap"]["password"].as<String>();

    if (value.length() < sizeof(dst.ap.password) && !value.equals(dst.ap.password)) {
      strcpy(dst.ap.password, value.c_str());
      changed = true;
    }
  }

  if (!src["ap"]["channel"].isNull()) {
    unsigned char value = src["ap"]["channel"].as<unsigned char>();

    if (value >= 0 && value < 12) {
      dst.ap.channel = value;
      changed = true;
    }
  }


  // sta
  if (!src["sta"]["ssid"].isNull()) {
    String value = src["sta"]["ssid"].as<String>();

    if (value.length() < sizeof(dst.sta.ssid) && !value.equals(dst.sta.ssid)) {
      strcpy(dst.sta.ssid, value.c_str());
      changed = true;
    }
  }

  if (!src["sta"]["password"].isNull()) {
    String value = src["sta"]["password"].as<String>();

    if (value.length() < sizeof(dst.sta.password) && !value.equals(dst.sta.password)) {
      strcpy(dst.sta.password, value.c_str());
      changed = true;
    }
  }

  if (!src["sta"]["channel"].isNull()) {
    unsigned char value = src["sta"]["channel"].as<unsigned char>();

    if (value >= 0 && value < 12) {
      dst.sta.channel = value;
      changed = true;
    }
  }

  return changed;
}

void settingsToJson(const Settings& src, JsonVariant dst, bool safe = false) {
  if (!safe) {
    dst["system"]["logLevel"] = static_cast<uint8_t>(src.system.logLevel);
    dst["system"]["serial"]["enable"] = src.system.serial.enabled;
    dst["system"]["serial"]["baudrate"] = src.system.serial.baudrate;
    dst["system"]["telnet"]["enable"] = src.system.telnet.enabled;
    dst["system"]["telnet"]["port"] = src.system.telnet.port;
    dst["system"]["unitSystem"] = static_cast<uint8_t>(src.system.unitSystem);
    dst["system"]["statusLedGpio"] = src.system.statusLedGpio;

    dst["portal"]["auth"] = src.portal.auth;
    dst["portal"]["login"] = src.portal.login;
    dst["portal"]["password"] = src.portal.password;

    dst["opentherm"]["unitSystem"] = static_cast<uint8_t>(src.opentherm.unitSystem);
    dst["opentherm"]["inGpio"] = src.opentherm.inGpio;
    dst["opentherm"]["outGpio"] = src.opentherm.outGpio;
    dst["opentherm"]["rxLedGpio"] = src.opentherm.rxLedGpio;
    dst["opentherm"]["memberId"] = src.opentherm.memberId;
    dst["opentherm"]["flags"] = src.opentherm.flags;
    dst["opentherm"]["maxModulation"] = src.opentherm.maxModulation;
    dst["opentherm"]["minPower"] = roundf(src.opentherm.minPower, 2);
    dst["opentherm"]["maxPower"] = roundf(src.opentherm.maxPower, 2);
    dst["opentherm"]["dhwPresent"] = src.opentherm.dhwPresent;
    dst["opentherm"]["summerWinterMode"] = src.opentherm.summerWinterMode;
    dst["opentherm"]["heatingCh2Enabled"] = src.opentherm.heatingCh2Enabled;
    dst["opentherm"]["heatingCh1ToCh2"] = src.opentherm.heatingCh1ToCh2;
    dst["opentherm"]["dhwToCh2"] = src.opentherm.dhwToCh2;
    dst["opentherm"]["dhwBlocking"] = src.opentherm.dhwBlocking;
    dst["opentherm"]["modulationSyncWithHeating"] = src.opentherm.modulationSyncWithHeating;
    dst["opentherm"]["getMinMaxTemp"] = src.opentherm.getMinMaxTemp;
    dst["opentherm"]["nativeHeatingControl"] = src.opentherm.nativeHeatingControl;
    dst["opentherm"]["immergasFix"] = src.opentherm.immergasFix;

    dst["mqtt"]["enable"] = src.mqtt.enabled;
    dst["mqtt"]["server"] = src.mqtt.server;
    dst["mqtt"]["port"] = src.mqtt.port;
    dst["mqtt"]["user"] = src.mqtt.user;
    dst["mqtt"]["password"] = src.mqtt.password;
    dst["mqtt"]["prefix"] = src.mqtt.prefix;
    dst["mqtt"]["interval"] = src.mqtt.interval;
    dst["mqtt"]["homeAssistantDiscovery"] = src.mqtt.homeAssistantDiscovery;

    dst["emergency"]["target"] = roundf(src.emergency.target, 2);
    dst["emergency"]["tresholdTime"] = src.emergency.tresholdTime;
  }

  dst["heating"]["enable"] = src.heating.enabled;
  dst["heating"]["turbo"] = src.heating.turbo;
  dst["heating"]["target"] = roundf(src.heating.target, 2);
  dst["heating"]["hysteresis"] = roundf(src.heating.hysteresis, 3);
  dst["heating"]["turboFactor"] = roundf(src.heating.turboFactor, 3);
  dst["heating"]["minTemp"] = src.heating.minTemp;
  dst["heating"]["maxTemp"] = src.heating.maxTemp;

  dst["dhw"]["enable"] = src.dhw.enabled;
  dst["dhw"]["target"] = roundf(src.dhw.target, 1);
  dst["dhw"]["minTemp"] = src.dhw.minTemp;
  dst["dhw"]["maxTemp"] = src.dhw.maxTemp;

  dst["equitherm"]["enable"] = src.equitherm.enabled;
  dst["equitherm"]["n_factor"] = roundf(src.equitherm.n_factor, 3);
  dst["equitherm"]["k_factor"] = roundf(src.equitherm.k_factor, 3);
  dst["equitherm"]["t_factor"] = roundf(src.equitherm.t_factor, 3);

  dst["pid"]["enable"] = src.pid.enabled;
  dst["pid"]["p_factor"] = roundf(src.pid.p_factor, 3);
  dst["pid"]["i_factor"] = roundf(src.pid.i_factor, 4);
  dst["pid"]["d_factor"] = roundf(src.pid.d_factor, 1);
  dst["pid"]["dt"] = src.pid.dt;
  dst["pid"]["minTemp"] = src.pid.minTemp;
  dst["pid"]["maxTemp"] = src.pid.maxTemp;

  if (!safe) {
    dst["externalPump"]["use"] = src.externalPump.use;
    dst["externalPump"]["gpio"] = src.externalPump.gpio;
    dst["externalPump"]["postCirculationTime"] = roundf(src.externalPump.postCirculationTime / 60, 0);
    dst["externalPump"]["antiStuckInterval"] = roundf(src.externalPump.antiStuckInterval / 86400, 0);
    dst["externalPump"]["antiStuckTime"] = roundf(src.externalPump.antiStuckTime / 60, 0);

    dst["cascadeControl"]["input"]["enable"] = src.cascadeControl.input.enabled;
    dst["cascadeControl"]["input"]["gpio"] = src.cascadeControl.input.gpio;
    dst["cascadeControl"]["input"]["invertState"] = src.cascadeControl.input.invertState;
    dst["cascadeControl"]["input"]["thresholdTime"] = src.cascadeControl.input.thresholdTime;

    dst["cascadeControl"]["output"]["enable"] = src.cascadeControl.output.enabled;
    dst["cascadeControl"]["output"]["gpio"] = src.cascadeControl.output.gpio;
    dst["cascadeControl"]["output"]["invertState"] = src.cascadeControl.output.invertState;
    dst["cascadeControl"]["output"]["thresholdTime"] = src.cascadeControl.output.thresholdTime;
    dst["cascadeControl"]["output"]["onFault"] = src.cascadeControl.output.onFault;
    dst["cascadeControl"]["output"]["onLossConnection"] = src.cascadeControl.output.onLossConnection;
    dst["cascadeControl"]["output"]["onEnabledHeating"] = src.cascadeControl.output.onEnabledHeating;
  }
}

inline void safeSettingsToJson(const Settings& src, JsonVariant dst) {
  settingsToJson(src, dst, true);
}

bool jsonToSettings(const JsonVariantConst src, Settings& dst, bool safe = false) {
  bool changed = false;

  if (!safe) {
    // system
    if (!src["system"]["logLevel"].isNull()) {
      uint8_t value = src["system"]["logLevel"].as<uint8_t>();

      if (value != dst.system.logLevel && value >= TinyLogger::Level::SILENT && value <= TinyLogger::Level::VERBOSE) {
        dst.system.logLevel = value;
        changed = true;
      }
    }

    if (src["system"]["serial"]["enable"].is<bool>()) {
      bool value = src["system"]["serial"]["enable"].as<bool>();

      if (value != dst.system.serial.enabled) {
        dst.system.serial.enabled = value;
        changed = true;
      }
    }

    if (!src["system"]["serial"]["baudrate"].isNull()) {
      unsigned int value = src["system"]["serial"]["baudrate"].as<unsigned int>();

      if (value == 9600 || value == 19200 || value == 38400 || value == 57600 || value == 74880 || value == 115200) {
        if (value != dst.system.serial.baudrate) {
          dst.system.serial.baudrate = value;
          changed = true;
        }
      }
    }

    if (src["system"]["telnet"]["enable"].is<bool>()) {
      bool value = src["system"]["telnet"]["enable"].as<bool>();

      if (value != dst.system.telnet.enabled) {
        dst.system.telnet.enabled = value;
        changed = true;
      }
    }

    if (!src["system"]["telnet"]["port"].isNull()) {
      unsigned short value = src["system"]["telnet"]["port"].as<unsigned short>();

      if (value > 0 && value <= 65535 && value != dst.system.telnet.port) {
        dst.system.telnet.port = value;
        changed = true;
      }
    }

    if (!src["system"]["unitSystem"].isNull()) {
      uint8_t value = src["system"]["unitSystem"].as<unsigned char>();
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

    if (!src["system"]["statusLedGpio"].isNull()) {
      if (src["system"]["statusLedGpio"].is<JsonString>() && src["system"]["statusLedGpio"].as<JsonString>().size() == 0) {
        if (dst.system.statusLedGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.system.statusLedGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src["system"]["statusLedGpio"].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.system.statusLedGpio) {
          dst.system.statusLedGpio = value;
          changed = true;
        }
      }
    }


    // portal
    if (src["portal"]["auth"].is<bool>()) {
      bool value = src["portal"]["auth"].as<bool>();

      if (value != dst.portal.auth) {
        dst.portal.auth = value;
        changed = true;
      }
    }

    if (!src["portal"]["login"].isNull()) {
      String value = src["portal"]["login"].as<String>();

      if (value.length() < sizeof(dst.portal.login) && !value.equals(dst.portal.login)) {
        strcpy(dst.portal.login, value.c_str());
        changed = true;
      }
    }

    if (!src["portal"]["password"].isNull()) {
      String value = src["portal"]["password"].as<String>();

      if (value.length() < sizeof(dst.portal.password) && !value.equals(dst.portal.password)) {
        strcpy(dst.portal.password, value.c_str());
        changed = true;
      }
    }


    // opentherm
    if (!src["opentherm"]["unitSystem"].isNull()) {
      uint8_t value = src["opentherm"]["unitSystem"].as<unsigned char>();

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

    if (!src["opentherm"]["inGpio"].isNull()) {
      if (src["opentherm"]["inGpio"].is<JsonString>() && src["opentherm"]["inGpio"].as<JsonString>().size() == 0) {
        if (dst.opentherm.inGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.opentherm.inGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src["opentherm"]["inGpio"].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.opentherm.inGpio) {
          dst.opentherm.inGpio = value;
          changed = true;
        }
      }
    }

    if (!src["opentherm"]["outGpio"].isNull()) {
      if (src["opentherm"]["outGpio"].is<JsonString>() && src["opentherm"]["outGpio"].as<JsonString>().size() == 0) {
        if (dst.opentherm.outGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.opentherm.outGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src["opentherm"]["outGpio"].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.opentherm.outGpio) {
          dst.opentherm.outGpio = value;
          changed = true;
        }
      }
    }

    if (!src["opentherm"]["rxLedGpio"].isNull()) {
      if (src["opentherm"]["rxLedGpio"].is<JsonString>() && src["opentherm"]["rxLedGpio"].as<JsonString>().size() == 0) {
        if (dst.opentherm.rxLedGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.opentherm.rxLedGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src["opentherm"]["rxLedGpio"].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.opentherm.rxLedGpio) {
          dst.opentherm.rxLedGpio = value;
          changed = true;
        }
      }
    }

    if (!src["opentherm"]["memberId"].isNull()) {
      auto value = src["opentherm"]["memberId"].as<uint8_t>();

      if (value != dst.opentherm.memberId) {
        dst.opentherm.memberId = value;
        changed = true;
      }
    }

    if (!src["opentherm"]["flags"].isNull()) {
      auto value = src["opentherm"]["flags"].as<uint8_t>();

      if (value != dst.opentherm.flags) {
        dst.opentherm.flags = value;
        changed = true;
      }
    }

    if (!src["opentherm"]["maxModulation"].isNull()) {
      unsigned char value = src["opentherm"]["maxModulation"].as<unsigned char>();

      if (value > 0 && value <= 100 && value != dst.opentherm.maxModulation) {
        dst.opentherm.maxModulation = value;
        changed = true;
      }
    }

    if (!src["opentherm"]["minPower"].isNull()) {
      float value = src["opentherm"]["minPower"].as<float>();

      if (value >= 0 && value <= 1000 && fabsf(value - dst.opentherm.minPower) > 0.0001f) {
        dst.opentherm.minPower = roundf(value, 2);
        changed = true;
      }
    }

    if (!src["opentherm"]["maxPower"].isNull()) {
      float value = src["opentherm"]["maxPower"].as<float>();

      if (value >= 0 && value <= 1000 && fabsf(value - dst.opentherm.maxPower) > 0.0001f) {
        dst.opentherm.maxPower = roundf(value, 2);
        changed = true;
      }
    }

    if (src["opentherm"]["dhwPresent"].is<bool>()) {
      bool value = src["opentherm"]["dhwPresent"].as<bool>();

      if (value != dst.opentherm.dhwPresent) {
        dst.opentherm.dhwPresent = value;
        changed = true;
      }
    }

    if (src["opentherm"]["summerWinterMode"].is<bool>()) {
      bool value = src["opentherm"]["summerWinterMode"].as<bool>();

      if (value != dst.opentherm.summerWinterMode) {
        dst.opentherm.summerWinterMode = value;
        changed = true;
      }
    }

    if (src["opentherm"]["heatingCh2Enabled"].is<bool>()) {
      bool value = src["opentherm"]["heatingCh2Enabled"].as<bool>();

      if (value != dst.opentherm.heatingCh2Enabled) {
        dst.opentherm.heatingCh2Enabled = value;

        if (dst.opentherm.heatingCh2Enabled) {
          dst.opentherm.heatingCh1ToCh2 = false;
          dst.opentherm.dhwToCh2 = false;
        }

        changed = true;
      }
    }

    if (src["opentherm"]["heatingCh1ToCh2"].is<bool>()) {
      bool value = src["opentherm"]["heatingCh1ToCh2"].as<bool>();

      if (value != dst.opentherm.heatingCh1ToCh2) {
        dst.opentherm.heatingCh1ToCh2 = value;

        if (dst.opentherm.heatingCh1ToCh2) {
          dst.opentherm.heatingCh2Enabled = false;
          dst.opentherm.dhwToCh2 = false;
        }

        changed = true;
      }
    }

    if (src["opentherm"]["dhwToCh2"].is<bool>()) {
      bool value = src["opentherm"]["dhwToCh2"].as<bool>();

      if (value != dst.opentherm.dhwToCh2) {
        dst.opentherm.dhwToCh2 = value;

        if (dst.opentherm.dhwToCh2) {
          dst.opentherm.heatingCh2Enabled = false;
          dst.opentherm.heatingCh1ToCh2 = false;
        }

        changed = true;
      }
    }

    if (src["opentherm"]["dhwBlocking"].is<bool>()) {
      bool value = src["opentherm"]["dhwBlocking"].as<bool>();

      if (value != dst.opentherm.dhwBlocking) {
        dst.opentherm.dhwBlocking = value;
        changed = true;
      }
    }

    if (src["opentherm"]["modulationSyncWithHeating"].is<bool>()) {
      bool value = src["opentherm"]["modulationSyncWithHeating"].as<bool>();

      if (value != dst.opentherm.modulationSyncWithHeating) {
        dst.opentherm.modulationSyncWithHeating = value;
        changed = true;
      }
    }

    if (src["opentherm"]["getMinMaxTemp"].is<bool>()) {
      bool value = src["opentherm"]["getMinMaxTemp"].as<bool>();

      if (value != dst.opentherm.getMinMaxTemp) {
        dst.opentherm.getMinMaxTemp = value;
        changed = true;
      }
    }

    if (src["opentherm"]["nativeHeatingControl"].is<bool>()) {
      bool value = src["opentherm"]["nativeHeatingControl"].as<bool>();

      if (value != dst.opentherm.nativeHeatingControl) {
        dst.opentherm.nativeHeatingControl = value;

        if (value) {
          dst.equitherm.enabled = false;
          dst.pid.enabled = false;
        }

        changed = true;
      }
    }

    if (src["opentherm"]["immergasFix"].is<bool>()) {
      bool value = src["opentherm"]["immergasFix"].as<bool>();

      if (value != dst.opentherm.immergasFix) {
        dst.opentherm.immergasFix = value;
        changed = true;
      }
    }


    // mqtt
    if (src["mqtt"]["enable"].is<bool>()) {
      bool value = src["mqtt"]["enable"].as<bool>();

      if (value != dst.mqtt.enabled) {
        dst.mqtt.enabled = value;
        changed = true;
      }
    }

    if (!src["mqtt"]["server"].isNull()) {
      String value = src["mqtt"]["server"].as<String>();

      if (value.length() < sizeof(dst.mqtt.server) && !value.equals(dst.mqtt.server)) {
        strcpy(dst.mqtt.server, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["port"].isNull()) {
      unsigned short value = src["mqtt"]["port"].as<unsigned short>();

      if (value > 0 && value <= 65535 && value != dst.mqtt.port) {
        dst.mqtt.port = value;
        changed = true;
      }
    }

    if (!src["mqtt"]["user"].isNull()) {
      String value = src["mqtt"]["user"].as<String>();

      if (value.length() < sizeof(dst.mqtt.user) && !value.equals(dst.mqtt.user)) {
        strcpy(dst.mqtt.user, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["password"].isNull()) {
      String value = src["mqtt"]["password"].as<String>();

      if (value.length() < sizeof(dst.mqtt.password) && !value.equals(dst.mqtt.password)) {
        strcpy(dst.mqtt.password, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["prefix"].isNull()) {
      String value = src["mqtt"]["prefix"].as<String>();

      if (value.length() < sizeof(dst.mqtt.prefix) && !value.equals(dst.mqtt.prefix)) {
        strcpy(dst.mqtt.prefix, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["interval"].isNull()) {
      unsigned short value = src["mqtt"]["interval"].as<unsigned short>();

      if (value >= 3 && value <= 60 && value != dst.mqtt.interval) {
        dst.mqtt.interval = value;
        changed = true;
      }
    }

    if (src["mqtt"]["homeAssistantDiscovery"].is<bool>()) {
      bool value = src["mqtt"]["homeAssistantDiscovery"].as<bool>();

      if (value != dst.mqtt.homeAssistantDiscovery) {
        dst.mqtt.homeAssistantDiscovery = value;
        changed = true;
      }
    }


    // emergency
    if (!src["emergency"]["tresholdTime"].isNull()) {
      unsigned short value = src["emergency"]["tresholdTime"].as<unsigned short>();

      if (value >= 60 && value <= 1800 && value != dst.emergency.tresholdTime) {
        dst.emergency.tresholdTime = value;
        changed = true;
      }
    }
  }


  // equitherm
  if (src["equitherm"]["enable"].is<bool>()) {
    bool value = src["equitherm"]["enable"].as<bool>();

    if (!dst.opentherm.nativeHeatingControl) {
      if (value != dst.equitherm.enabled) {
        dst.equitherm.enabled = value;
        changed = true;
      }

    } else if (dst.equitherm.enabled) {
      dst.equitherm.enabled = false;
      changed = true;
    }
  }

  if (!src["equitherm"]["n_factor"].isNull()) {
    float value = src["equitherm"]["n_factor"].as<float>();

    if (value > 0 && value <= 10 && fabsf(value - dst.equitherm.n_factor) > 0.0001f) {
      dst.equitherm.n_factor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src["equitherm"]["k_factor"].isNull()) {
    float value = src["equitherm"]["k_factor"].as<float>();

    if (value >= 0 && value <= 10 && fabsf(value - dst.equitherm.k_factor) > 0.0001f) {
      dst.equitherm.k_factor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src["equitherm"]["t_factor"].isNull()) {
    float value = src["equitherm"]["t_factor"].as<float>();

    if (value >= 0 && value <= 10 && fabsf(value - dst.equitherm.t_factor) > 0.0001f) {
      dst.equitherm.t_factor = roundf(value, 3);
      changed = true;
    }
  }


  // pid
  if (src["pid"]["enable"].is<bool>()) {
    bool value = src["pid"]["enable"].as<bool>();

    if (!dst.opentherm.nativeHeatingControl) {
      if (value != dst.pid.enabled) {
        dst.pid.enabled = value;
        changed = true;
      }

    } else if (dst.pid.enabled) {
      dst.pid.enabled = false;
      changed = true;
    }
  }

  if (!src["pid"]["p_factor"].isNull()) {
    float value = src["pid"]["p_factor"].as<float>();

    if (value > 0 && value <= 1000 && fabsf(value - dst.pid.p_factor) > 0.0001f) {
      dst.pid.p_factor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src["pid"]["i_factor"].isNull()) {
    float value = src["pid"]["i_factor"].as<float>();

    if (value >= 0 && value <= 100 && fabsf(value - dst.pid.i_factor) > 0.0001f) {
      dst.pid.i_factor = roundf(value, 4);
      changed = true;
    }
  }

  if (!src["pid"]["d_factor"].isNull()) {
    float value = src["pid"]["d_factor"].as<float>();

    if (value >= 0 && value <= 100000 && fabsf(value - dst.pid.d_factor) > 0.0001f) {
      dst.pid.d_factor = roundf(value, 1);
      changed = true;
    }
  }

  if (!src["pid"]["dt"].isNull()) {
    unsigned short value = src["pid"]["dt"].as<unsigned short>();

    if (value >= 30 && value <= 1800 && value != dst.pid.dt) {
      dst.pid.dt = value;
      changed = true;
    }
  }

  if (!src["pid"]["minTemp"].isNull()) {
    short value = src["pid"]["minTemp"].as<short>();

    if (isValidTemp(value, dst.system.unitSystem, dst.equitherm.enabled ? -99.9f : 0.0f) && value != dst.pid.minTemp) {
      dst.pid.minTemp = value;
      changed = true;
    }
  }

  if (!src["pid"]["maxTemp"].isNull()) {
    short value = src["pid"]["maxTemp"].as<short>();

    if (isValidTemp(value, dst.system.unitSystem) && value != dst.pid.maxTemp) {
      dst.pid.maxTemp = value;
      changed = true;
    }
  }

  if (dst.pid.maxTemp < dst.pid.minTemp) {
    dst.pid.maxTemp = dst.pid.minTemp;
    changed = true;
  }


  // heating
  if (src["heating"]["enable"].is<bool>()) {
    bool value = src["heating"]["enable"].as<bool>();

    if (value != dst.heating.enabled) {
      dst.heating.enabled = value;
      changed = true;
    }
  }

  if (src["heating"]["turbo"].is<bool>()) {
    bool value = src["heating"]["turbo"].as<bool>();

    if (value != dst.heating.turbo) {
      dst.heating.turbo = value;
      changed = true;
    }
  }

  if (!src["heating"]["hysteresis"].isNull()) {
    float value = src["heating"]["hysteresis"].as<float>();

    if (value >= 0.0f && value <= 15.0f && fabsf(value - dst.heating.hysteresis) > 0.0001f) {
      dst.heating.hysteresis = roundf(value, 2);
      changed = true;
    }
  }

  if (!src["heating"]["turboFactor"].isNull()) {
    float value = src["heating"]["turboFactor"].as<float>();

    if (value >= 1.5f && value <= 10.0f && fabsf(value - dst.heating.turboFactor) > 0.0001f) {
      dst.heating.turboFactor = roundf(value, 3);
      changed = true;
    }
  }

  if (!src["heating"]["minTemp"].isNull()) {
    unsigned char value = src["heating"]["minTemp"].as<unsigned char>();

    if (value != dst.heating.minTemp && value >= vars.slave.heating.minTemp && value < vars.slave.heating.maxTemp && value != dst.heating.minTemp) {
      dst.heating.minTemp = value;
      changed = true;
    }
  }

  if (!src["heating"]["maxTemp"].isNull()) {
    unsigned char value = src["heating"]["maxTemp"].as<unsigned char>();

    if (value != dst.heating.maxTemp && value > vars.slave.heating.minTemp && value <= vars.slave.heating.maxTemp && value != dst.heating.maxTemp) {
      dst.heating.maxTemp = value;
      changed = true;
    }
  }

  if (dst.heating.maxTemp < dst.heating.minTemp) {
    dst.heating.maxTemp = dst.heating.minTemp;
    changed = true;
  }


  // dhw
  if (src["dhw"]["enable"].is<bool>()) {
    bool value = src["dhw"]["enable"].as<bool>();

    if (value != dst.dhw.enabled) {
      dst.dhw.enabled = value;
      changed = true;
    }
  }

  if (!src["dhw"]["minTemp"].isNull()) {
    unsigned char value = src["dhw"]["minTemp"].as<unsigned char>();

    if (value >= vars.slave.dhw.minTemp && value < vars.slave.dhw.maxTemp && value != dst.dhw.minTemp) {
      dst.dhw.minTemp = value;
      changed = true;
    }
  }

  if (!src["dhw"]["maxTemp"].isNull()) {
    unsigned char value = src["dhw"]["maxTemp"].as<unsigned char>();

    if (value > vars.slave.dhw.minTemp && value <= vars.slave.dhw.maxTemp && value != dst.dhw.maxTemp) {
      dst.dhw.maxTemp = value;
      changed = true;
    }
  }

  if (dst.dhw.maxTemp < dst.dhw.minTemp) {
    dst.dhw.maxTemp = dst.dhw.minTemp;
    changed = true;
  }


  if (!safe) {
    // external pump
    if (src["externalPump"]["use"].is<bool>()) {
      bool value = src["externalPump"]["use"].as<bool>();

      if (value != dst.externalPump.use) {
        dst.externalPump.use = value;
        changed = true;
      }
    }

    if (!src["externalPump"]["gpio"].isNull()) {
      if (src["externalPump"]["gpio"].is<JsonString>() && src["externalPump"]["gpio"].as<JsonString>().size() == 0) {
        if (dst.externalPump.gpio != GPIO_IS_NOT_CONFIGURED) {
          dst.externalPump.gpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src["externalPump"]["gpio"].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.externalPump.gpio) {
          dst.externalPump.gpio = value;
          changed = true;
        }
      }
    }

    if (!src["externalPump"]["postCirculationTime"].isNull()) {
      unsigned short value = src["externalPump"]["postCirculationTime"].as<unsigned short>();

      if (value >= 0 && value <= 120) {
        value = value * 60;

        if (value != dst.externalPump.postCirculationTime) {
          dst.externalPump.postCirculationTime = value;
          changed = true;
        }
      }
    }

    if (!src["externalPump"]["antiStuckInterval"].isNull()) {
      unsigned int value = src["externalPump"]["antiStuckInterval"].as<unsigned int>();

      if (value >= 0 && value <= 366) {
        value = value * 86400;

        if (value != dst.externalPump.antiStuckInterval) {
          dst.externalPump.antiStuckInterval = value;
          changed = true;
        }
      }
    }

    if (!src["externalPump"]["antiStuckTime"].isNull()) {
      unsigned short value = src["externalPump"]["antiStuckTime"].as<unsigned short>();

      if (value >= 0 && value <= 20) {
        value = value * 60;

        if (value != dst.externalPump.antiStuckTime) {
          dst.externalPump.antiStuckTime = value;
          changed = true;
        }
      }
    }


    // cascade control
    if (src["cascadeControl"]["input"]["enable"].is<bool>()) {
      bool value = src["cascadeControl"]["input"]["enable"].as<bool>();

      if (value != dst.cascadeControl.input.enabled) {
        dst.cascadeControl.input.enabled = value;
        changed = true;
      }
    }

    if (!src["cascadeControl"]["input"]["gpio"].isNull()) {
      if (src["cascadeControl"]["input"]["gpio"].is<JsonString>() && src["cascadeControl"]["input"]["gpio"].as<JsonString>().size() == 0) {
        if (dst.cascadeControl.input.gpio != GPIO_IS_NOT_CONFIGURED) {
          dst.cascadeControl.input.gpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src["cascadeControl"]["input"]["gpio"].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.cascadeControl.input.gpio) {
          dst.cascadeControl.input.gpio = value;
          changed = true;
        }
      }
    }

    if (src["cascadeControl"]["input"]["invertState"].is<bool>()) {
      bool value = src["cascadeControl"]["input"]["invertState"].as<bool>();

      if (value != dst.cascadeControl.input.invertState) {
        dst.cascadeControl.input.invertState = value;
        changed = true;
      }
    }

    if (!src["cascadeControl"]["input"]["thresholdTime"].isNull()) {
      unsigned short value = src["cascadeControl"]["input"]["thresholdTime"].as<unsigned short>();

      if (value >= 5 && value <= 600) {
        if (value != dst.cascadeControl.input.thresholdTime) {
          dst.cascadeControl.input.thresholdTime = value;
          changed = true;
        }
      }
    }

    if (src["cascadeControl"]["output"]["enable"].is<bool>()) {
      bool value = src["cascadeControl"]["output"]["enable"].as<bool>();

      if (value != dst.cascadeControl.output.enabled) {
        dst.cascadeControl.output.enabled = value;
        changed = true;
      }
    }

    if (!src["cascadeControl"]["output"]["gpio"].isNull()) {
      if (src["cascadeControl"]["output"]["gpio"].is<JsonString>() && src["cascadeControl"]["output"]["gpio"].as<JsonString>().size() == 0) {
        if (dst.cascadeControl.output.gpio != GPIO_IS_NOT_CONFIGURED) {
          dst.cascadeControl.output.gpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }

      } else {
        unsigned char value = src["cascadeControl"]["output"]["gpio"].as<unsigned char>();

        if (GPIO_IS_VALID(value) && value != dst.cascadeControl.output.gpio) {
          dst.cascadeControl.output.gpio = value;
          changed = true;
        }
      }
    }

    if (src["cascadeControl"]["output"]["invertState"].is<bool>()) {
      bool value = src["cascadeControl"]["output"]["invertState"].as<bool>();

      if (value != dst.cascadeControl.output.invertState) {
        dst.cascadeControl.output.invertState = value;
        changed = true;
      }
    }

    if (!src["cascadeControl"]["output"]["thresholdTime"].isNull()) {
      unsigned short value = src["cascadeControl"]["output"]["thresholdTime"].as<unsigned short>();

      if (value >= 5 && value <= 600) {
        if (value != dst.cascadeControl.output.thresholdTime) {
          dst.cascadeControl.output.thresholdTime = value;
          changed = true;
        }
      }
    }

    if (src["cascadeControl"]["output"]["onFault"].is<bool>()) {
      bool value = src["cascadeControl"]["output"]["onFault"].as<bool>();

      if (value != dst.cascadeControl.output.onFault) {
        dst.cascadeControl.output.onFault = value;
        changed = true;
      }
    }

    if (src["cascadeControl"]["output"]["onLossConnection"].is<bool>()) {
      bool value = src["cascadeControl"]["output"]["onLossConnection"].as<bool>();

      if (value != dst.cascadeControl.output.onLossConnection) {
        dst.cascadeControl.output.onLossConnection = value;
        changed = true;
      }
    }

    if (src["cascadeControl"]["output"]["onEnabledHeating"].is<bool>()) {
      bool value = src["cascadeControl"]["output"]["onEnabledHeating"].as<bool>();

      if (value != dst.cascadeControl.output.onEnabledHeating) {
        dst.cascadeControl.output.onEnabledHeating = value;
        changed = true;
      }
    }
  }

  // force check emergency target
  {
    float value = !src["emergency"]["target"].isNull() ? src["emergency"]["target"].as<float>() : dst.emergency.target;
    bool noRegulators = !dst.opentherm.nativeHeatingControl;
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
    float value = !src["heating"]["target"].isNull() ? src["heating"]["target"].as<float>() : dst.heating.target;
    bool valid = isValidTemp(
      value,
      dst.system.unitSystem,
      vars.master.heating.minTemp,
      vars.master.heating.maxTemp,
      dst.system.unitSystem
    );

    if (!valid) {
      value = convertTemp(
        vars.master.heating.indoorTempControl
        ? THERMOSTAT_INDOOR_DEFAULT_TEMP
        : DEFAULT_HEATING_TARGET_TEMP,
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
    float value = !src["dhw"]["target"].isNull() ? src["dhw"]["target"].as<float>() : dst.dhw.target;
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
  dst["id"] = sensorId;
  dst["enabled"] = src.enabled;
  dst["name"] = src.name;
  dst["purpose"] = static_cast<uint8_t>(src.purpose);
  dst["type"] = static_cast<uint8_t>(src.type);
  dst["gpio"] = src.gpio;

  if (src.type == Sensors::Type::DALLAS_TEMP) {
    char addr[24];
    sprintf(
      addr,
      "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
      src.address[0], src.address[1], src.address[2], src.address[3],
      src.address[4], src.address[5], src.address[6], src.address[7]
    );
    dst["address"] = String(addr);

  } else if (src.type == Sensors::Type::BLUETOOTH) {
    char addr[18];
    sprintf(
      addr,
      "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
      src.address[0], src.address[1], src.address[2],
      src.address[3], src.address[4], src.address[5]
    );
    dst["address"] = String(addr);

  } else {
    dst["address"] = "";
  }

  dst["offset"] = roundf(src.offset, 3);
  dst["factor"] = roundf(src.factor, 3);
  dst["filtering"] = src.filtering;
  dst["filteringFactor"] = roundf(src.filteringFactor, 3);
}

bool jsonToSensorSettings(const uint8_t sensorId, const JsonVariantConst src, Sensors::Settings& dst) {
  if (sensorId > Sensors::getMaxSensorId()) {
    return false;
  }

  bool changed = false;

  // enabled
  if (src["enabled"].is<bool>()) {
    auto value = src["enabled"].as<bool>();

    if (value != dst.enabled) {
      dst.enabled = value;
      changed = true;
    }
  }

  // name
  if (!src["name"].isNull()) {
    String value = Sensors::cleanName(src["name"].as<String>());

    if (value.length() < sizeof(dst.name) && !value.equals(dst.name)) {
      strcpy(dst.name, value.c_str());
      changed = true;
    }
  }

  // purpose
  if (!src["purpose"].isNull()) {
    uint8_t value = src["purpose"].as<uint8_t>();

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
      case static_cast<uint8_t>(Sensors::Purpose::CURRENT_POWER):
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
  if (!src["type"].isNull()) {
    uint8_t value = src["type"].as<uint8_t>();

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
  if (!src["gpio"].isNull()) {
    if (dst.type != Sensors::Type::DALLAS_TEMP && dst.type == Sensors::Type::BLUETOOTH && dst.type == Sensors::Type::NTC_10K_TEMP) {
      if (dst.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }

    } else if (src["gpio"].is<JsonString>() && src["gpio"].as<JsonString>().size() == 0) {
      if (dst.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }

    } else {
      unsigned char value = src["gpio"].as<unsigned char>();

      if (GPIO_IS_VALID(value) && value != dst.gpio) {
        dst.gpio = value;
        changed = true;
      }
    }
  }

  // address
  if (!src["address"].isNull()) {
    String value = src["address"].as<String>();

    if (dst.type == Sensors::Type::DALLAS_TEMP) {
      uint8_t tmp[8];
      int parsed = sscanf(
        value.c_str(),
        "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &tmp[0], &tmp[1], &tmp[2], &tmp[3],
        &tmp[4], &tmp[5], &tmp[6], &tmp[7]
      );

      if (parsed == 8) {
        for (uint8_t i = 0; i < 8; i++) {
          if (dst.address[i] != tmp[i]) {
            dst.address[i] = tmp[i];
            changed = true;
          }
        }
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
        for (uint8_t i = 0; i < 6; i++) {
          if (dst.address[i] != tmp[i]) {
            dst.address[i] = tmp[i];
            changed = true;
          }
        }
      }
    }
  }

  // offset
  if (!src["offset"].isNull()) {
    float value = src["offset"].as<float>();

    if (value >= -20.0f && value <= 20.0f && fabsf(value - dst.offset) > 0.0001f) {
      dst.offset = roundf(value, 2);
      changed = true;
    }
  }

  // factor
  if (!src["factor"].isNull()) {
    float value = src["factor"].as<float>();

    if (value > 0.09f && value <= 10.0f && fabsf(value - dst.factor) > 0.0001f) {
      dst.factor = roundf(value, 3);
      changed = true;
    }
  }

  // filtering
  if (src["filtering"].is<bool>()) {
    auto value = src["filtering"].as<bool>();

    if (value != dst.filtering) {
      dst.filtering = value;
      changed = true;
    }
  }

  // filtering factor
  if (!src["filteringFactor"].isNull()) {
    float value = src["filteringFactor"].as<float>();

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

  //dst["id"] = sensorId;
  dst["connected"] = rSensor.connected;
  dst["signalQuality"] = rSensor.signalQuality;

  if (sSensor.type == Sensors::Type::BLUETOOTH) {
    dst["temperature"] = rSensor.values[static_cast<uint8_t>(Sensors::ValueType::TEMPERATURE)];
    dst["humidity"] = rSensor.values[static_cast<uint8_t>(Sensors::ValueType::HUMIDITY)];
    dst["battery"] = rSensor.values[static_cast<uint8_t>(Sensors::ValueType::BATTERY)];
    dst["rssi"] = rSensor.values[static_cast<uint8_t>(Sensors::ValueType::RSSI)];

  } else {
    dst["value"] = rSensor.values[static_cast<uint8_t>(Sensors::ValueType::PRIMARY)];
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
  if (!src["value"].isNull()) {
    float value = src["value"].as<float>();

    uint8_t vType = static_cast<uint8_t>(Sensors::ValueType::PRIMARY);
    if (fabsf(value - dst.values[vType]) > 0.0001f) {
      dst.values[vType] = roundf(value, 2);
      changed = true;
    }
  }

  return changed;
}

void varsToJson(const Variables& src, JsonVariant dst) {
  dst["slave"]["memberId"] = src.slave.memberId;
  dst["slave"]["flags"] = src.slave.flags;
  dst["slave"]["type"] = src.slave.type;
  dst["slave"]["appVersion"] = src.slave.appVersion;
  dst["slave"]["protocolVersion"] = src.slave.appVersion;
  dst["slave"]["connected"] = src.slave.connected;
  dst["slave"]["flame"] = src.slave.flame;

  dst["slave"]["modulation"]["min"] = src.slave.modulation.min;
  dst["slave"]["modulation"]["max"] = src.slave.modulation.max;

  dst["slave"]["power"]["min"] = roundf(src.slave.power.min, 2);
  dst["slave"]["power"]["max"] = roundf(src.slave.power.max, 2);

  dst["slave"]["heating"]["active"] = src.slave.heating.active;
  dst["slave"]["heating"]["minTemp"] = src.slave.heating.minTemp;
  dst["slave"]["heating"]["maxTemp"] = src.slave.heating.maxTemp;

  dst["slave"]["dhw"]["active"] = src.slave.dhw.active;
  dst["slave"]["dhw"]["minTemp"] = src.slave.dhw.minTemp;
  dst["slave"]["dhw"]["maxTemp"] = src.slave.dhw.maxTemp;

  dst["slave"]["fault"]["active"] = src.slave.fault.active;
  dst["slave"]["fault"]["code"] = src.slave.fault.code;

  dst["slave"]["diag"]["active"] = src.slave.diag.active;
  dst["slave"]["diag"]["code"] = src.slave.diag.code;

  dst["master"]["heating"]["enabled"] = src.master.heating.enabled;
  dst["master"]["heating"]["blocking"] = src.master.heating.blocking;
  dst["master"]["heating"]["indoorTempControl"] = src.master.heating.indoorTempControl;
  dst["master"]["heating"]["targetTemp"] = roundf(src.master.heating.targetTemp, 2);
  dst["master"]["heating"]["currentTemp"] = roundf(src.master.heating.currentTemp, 2);
  dst["master"]["heating"]["returnTemp"] = roundf(src.master.heating.returnTemp, 2);
  dst["master"]["heating"]["indoorTemp"] = roundf(src.master.heating.indoorTemp, 2);
  dst["master"]["heating"]["outdoorTemp"] = roundf(src.master.heating.outdoorTemp, 2);
  dst["master"]["heating"]["minTemp"] = roundf(src.master.heating.minTemp, 2);
  dst["master"]["heating"]["maxTemp"] = roundf(src.master.heating.maxTemp, 2);

  dst["master"]["dhw"]["enabled"] = src.master.dhw.enabled;
  dst["master"]["dhw"]["targetTemp"] = roundf(src.master.dhw.targetTemp, 2);
  dst["master"]["dhw"]["currentTemp"] = roundf(src.master.dhw.currentTemp, 2);
  dst["master"]["dhw"]["returnTemp"] = roundf(src.master.dhw.returnTemp, 2);
  dst["master"]["dhw"]["minTemp"] = settings.dhw.minTemp;
  dst["master"]["dhw"]["maxTemp"] = settings.dhw.maxTemp;

  dst["master"]["network"]["connected"] = src.network.connected;
  dst["master"]["mqtt"]["connected"] = src.mqtt.connected;
  dst["master"]["emergency"]["state"] = src.emergency.state;
  dst["master"]["externalPump"]["state"] = src.externalPump.state;

  dst["master"]["cascadeControl"]["input"] = src.cascadeControl.input;
  dst["master"]["cascadeControl"]["output"] = src.cascadeControl.output;

  dst["master"]["uptime"] = millis() / 1000ul;
}

bool jsonToVars(const JsonVariantConst src, Variables& dst) {
  bool changed = false;

  // actions
  if (src["actions"]["restart"].is<bool>() && src["actions"]["restart"].as<bool>()) {
    dst.actions.restart = true;
  }

  if (src["actions"]["resetFault"].is<bool>() && src["actions"]["resetFault"].as<bool>()) {
    dst.actions.resetFault = true;
  }

  if (src["actions"]["resetDiagnostic"].is<bool>() && src["actions"]["resetDiagnostic"].as<bool>()) {
    dst.actions.resetDiagnostic = true;
  }

  return changed;
}