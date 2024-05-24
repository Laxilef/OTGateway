#include <Arduino.h>

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

double roundd(double value, uint8_t decimals = 2) {
  if (decimals == 0) {
    return (int)(value + 0.5);

  } else if (abs(value) < 0.00000001) {
    return 0.0;
  }

  double multiplier = pow10(decimals);
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
  switch(esp_reset_reason()) {
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

    if (value.length() < sizeof(dst.hostname)) {
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

    if (value.length() < sizeof(dst.staticConfig.ip)) {
      strcpy(dst.staticConfig.ip, value.c_str());
      changed = true;
    }
  }

  if (!src["staticConfig"]["gateway"].isNull()) {
    String value = src["staticConfig"]["gateway"].as<String>();

    if (value.length() < sizeof(dst.staticConfig.gateway)) {
      strcpy(dst.staticConfig.gateway, value.c_str());
      changed = true;
    }
  }

  if (!src["staticConfig"]["subnet"].isNull()) {
    String value = src["staticConfig"]["subnet"].as<String>();

    if (value.length() < sizeof(dst.staticConfig.subnet)) {
      strcpy(dst.staticConfig.subnet, value.c_str());
      changed = true;
    }
  }

  if (!src["staticConfig"]["dns"].isNull()) {
    String value = src["staticConfig"]["dns"].as<String>();

    if (value.length() < sizeof(dst.staticConfig.dns)) {
      strcpy(dst.staticConfig.dns, value.c_str());
      changed = true;
    }
  }


  // ap
  if (!src["ap"]["ssid"].isNull()) {
    String value = src["ap"]["ssid"].as<String>();

    if (value.length() < sizeof(dst.ap.ssid)) {
      strcpy(dst.ap.ssid, value.c_str());
      changed = true;
    }
  }

  if (!src["ap"]["password"].isNull()) {
    String value = src["ap"]["password"].as<String>();

    if (value.length() < sizeof(dst.ap.password)) {
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

    if (value.length() < sizeof(dst.sta.ssid)) {
      strcpy(dst.sta.ssid, value.c_str());
      changed = true;
    }
  }

  if (!src["sta"]["password"].isNull()) {
    String value = src["sta"]["password"].as<String>();

    if (value.length() < sizeof(dst.sta.password)) {
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
    dst["system"]["debug"] = src.system.debug;
    dst["system"]["serial"]["enable"] = src.system.serial.enable;
    dst["system"]["serial"]["baudrate"] = src.system.serial.baudrate;
    dst["system"]["telnet"]["enable"] = src.system.telnet.enable;
    dst["system"]["telnet"]["port"] = src.system.telnet.port;
    dst["system"]["unitSystem"] = static_cast<byte>(src.system.unitSystem);
    dst["system"]["statusLedGpio"] = src.system.statusLedGpio;

    dst["portal"]["auth"] = src.portal.auth;
    dst["portal"]["login"] = src.portal.login;
    dst["portal"]["password"] = src.portal.password;

    dst["opentherm"]["unitSystem"] = static_cast<byte>(src.opentherm.unitSystem);
    dst["opentherm"]["inGpio"] = src.opentherm.inGpio;
    dst["opentherm"]["outGpio"] = src.opentherm.outGpio;
    dst["opentherm"]["rxLedGpio"] = src.opentherm.rxLedGpio;
    dst["opentherm"]["memberIdCode"] = src.opentherm.memberIdCode;
    dst["opentherm"]["dhwPresent"] = src.opentherm.dhwPresent;
    dst["opentherm"]["summerWinterMode"] = src.opentherm.summerWinterMode;
    dst["opentherm"]["heatingCh2Enabled"] = src.opentherm.heatingCh2Enabled;
    dst["opentherm"]["heatingCh1ToCh2"] = src.opentherm.heatingCh1ToCh2;
    dst["opentherm"]["dhwToCh2"] = src.opentherm.dhwToCh2;
    dst["opentherm"]["dhwBlocking"] = src.opentherm.dhwBlocking;
    dst["opentherm"]["modulationSyncWithHeating"] = src.opentherm.modulationSyncWithHeating;
    dst["opentherm"]["getMinMaxTemp"] = src.opentherm.getMinMaxTemp;
    dst["opentherm"]["nativeHeatingControl"] = src.opentherm.nativeHeatingControl;

    dst["mqtt"]["enable"] = src.mqtt.enable;
    dst["mqtt"]["server"] = src.mqtt.server;
    dst["mqtt"]["port"] = src.mqtt.port;
    dst["mqtt"]["user"] = src.mqtt.user;
    dst["mqtt"]["password"] = src.mqtt.password;
    dst["mqtt"]["prefix"] = src.mqtt.prefix;
    dst["mqtt"]["interval"] = src.mqtt.interval;
    dst["mqtt"]["homeAssistantDiscovery"] = src.mqtt.homeAssistantDiscovery;

    dst["emergency"]["enable"] = src.emergency.enable;
    dst["emergency"]["target"] = roundd(src.emergency.target, 2);
    dst["emergency"]["tresholdTime"] = src.emergency.tresholdTime;
    dst["emergency"]["useEquitherm"] = src.emergency.useEquitherm;
    dst["emergency"]["usePid"] = src.emergency.usePid;
    dst["emergency"]["onNetworkFault"] = src.emergency.onNetworkFault;
    dst["emergency"]["onMqttFault"] = src.emergency.onMqttFault;
  }

  dst["heating"]["enable"] = src.heating.enable;
  dst["heating"]["turbo"] = src.heating.turbo;
  dst["heating"]["target"] = roundd(src.heating.target, 2);
  dst["heating"]["hysteresis"] = roundd(src.heating.hysteresis, 2);
  dst["heating"]["minTemp"] = src.heating.minTemp;
  dst["heating"]["maxTemp"] = src.heating.maxTemp;
  dst["heating"]["maxModulation"] = src.heating.maxModulation;

  dst["dhw"]["enable"] = src.dhw.enable;
  dst["dhw"]["target"] = roundd(src.dhw.target, 1);
  dst["dhw"]["minTemp"] = src.dhw.minTemp;
  dst["dhw"]["maxTemp"] = src.dhw.maxTemp;

  dst["pid"]["enable"] = src.pid.enable;
  dst["pid"]["p_factor"] = roundd(src.pid.p_factor, 3);
  dst["pid"]["i_factor"] = roundd(src.pid.i_factor, 4);
  dst["pid"]["d_factor"] = roundd(src.pid.d_factor, 1);
  dst["pid"]["dt"] = src.pid.dt;
  dst["pid"]["minTemp"] = src.pid.minTemp;
  dst["pid"]["maxTemp"] = src.pid.maxTemp;

  dst["equitherm"]["enable"] = src.equitherm.enable;
  dst["equitherm"]["n_factor"] = roundd(src.equitherm.n_factor, 3);
  dst["equitherm"]["k_factor"] = roundd(src.equitherm.k_factor, 3);
  dst["equitherm"]["t_factor"] = roundd(src.equitherm.t_factor, 3);

  dst["sensors"]["outdoor"]["type"] = static_cast<byte>(src.sensors.outdoor.type);
  dst["sensors"]["outdoor"]["gpio"] = src.sensors.outdoor.gpio;
  dst["sensors"]["outdoor"]["offset"] = roundd(src.sensors.outdoor.offset, 2);

  dst["sensors"]["indoor"]["type"] = static_cast<byte>(src.sensors.indoor.type);
  dst["sensors"]["indoor"]["gpio"] = src.sensors.indoor.gpio;

  char bleAddress[18];
  sprintf(
    bleAddress,
    "%02x:%02x:%02x:%02x:%02x:%02x",
    src.sensors.indoor.bleAddresss[0],
    src.sensors.indoor.bleAddresss[1],
    src.sensors.indoor.bleAddresss[2],
    src.sensors.indoor.bleAddresss[3],
    src.sensors.indoor.bleAddresss[4],
    src.sensors.indoor.bleAddresss[5]
  );
  dst["sensors"]["indoor"]["bleAddresss"] = String(bleAddress);
  dst["sensors"]["indoor"]["offset"] = roundd(src.sensors.indoor.offset, 2);

  dst["relay1"]["gpio"] = src.relay1.gpio;
  dst["relay1"]["normal"] = src.relay1.normal;
  dst["relay1"]["selfLocking"] = src.relay1.selfLocking;
  dst["relay2"]["gpio"] = src.relay2.gpio;
  dst["relay2"]["normal"] = src.relay2.normal;
  dst["relay2"]["selfLocking"] = src.relay2.selfLocking;
  dst["relay3"]["gpio"] = src.relay3.gpio;
  dst["relay3"]["normal"] = src.relay3.normal;
  dst["relay3"]["selfLocking"] = src.relay3.selfLocking;
  dst["relay4"]["gpio"] = src.relay4.gpio;
  dst["relay4"]["normal"] = src.relay4.normal;
  dst["relay4"]["selfLocking"] = src.relay4.selfLocking;

  if (!safe) {
    dst["externalPump"]["use"] = src.externalPump.use;
    dst["externalPump"]["gpio"] = src.externalPump.gpio;
    dst["externalPump"]["postCirculationTime"] = roundd(src.externalPump.postCirculationTime / 60, 0);
    dst["externalPump"]["antiStuckInterval"] = roundd(src.externalPump.antiStuckInterval / 86400, 0);
    dst["externalPump"]["antiStuckTime"] = roundd(src.externalPump.antiStuckTime / 60, 0);
  }
}

inline void safeSettingsToJson(const Settings& src, JsonVariant dst) {
  settingsToJson(src, dst, true);
}

bool jsonToSettings(const JsonVariantConst src, Settings& dst, bool safe = false) {
  bool changed = false;

  if (!safe) {
    // system
    if (src["system"]["debug"].is<bool>()) {
      bool value = src["system"]["debug"].as<bool>();

      if (value != dst.system.debug) {
        dst.system.debug = value;
        changed = true;
      }
    }

    if (src["system"]["serial"]["enable"].is<bool>()) {
      bool value = src["system"]["serial"]["enable"].as<bool>();

      if (value != dst.system.serial.enable) {
        dst.system.serial.enable = value;
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
      
      if (value != dst.system.telnet.enable) {
        dst.system.telnet.enable = value;
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
      byte value = src["system"]["unitSystem"].as<unsigned char>();
      UnitSystem prevUnitSystem = dst.system.unitSystem;

      switch (value) {
        case static_cast<byte>(UnitSystem::METRIC):
          if (dst.system.unitSystem != UnitSystem::METRIC) {
            dst.system.unitSystem = UnitSystem::METRIC;
            changed = true;
          }
          break;

        case static_cast<byte>(UnitSystem::IMPERIAL):
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

      if (value.length() < sizeof(dst.portal.login) && !String(dst.portal.login).equals(value)) {
        strcpy(dst.portal.login, value.c_str());
        changed = true;
      }
    }

    if (!src["portal"]["password"].isNull()) {
      String value = src["portal"]["password"].as<String>();

      if (value.length() < sizeof(dst.portal.password) && !String(dst.portal.password).equals(value)) {
        strcpy(dst.portal.password, value.c_str());
        changed = true;
      }
    }


    // opentherm
    if (!src["opentherm"]["unitSystem"].isNull()) {
      byte value = src["opentherm"]["unitSystem"].as<unsigned char>();

      switch (value) {
        case static_cast<byte>(UnitSystem::METRIC):
          if (dst.opentherm.unitSystem != UnitSystem::METRIC) {
            dst.opentherm.unitSystem = UnitSystem::METRIC;
            changed = true;
          }
          break;

        case static_cast<byte>(UnitSystem::IMPERIAL):
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

    if (!src["opentherm"]["memberIdCode"].isNull()) {
      unsigned int value = src["opentherm"]["memberIdCode"].as<unsigned int>();

      if (value >= 0 && value < 65536 && value != dst.opentherm.memberIdCode) {
        dst.opentherm.memberIdCode = value;
        changed = true;
      }
    }

    if (src["opentherm"]["dhwPresent"].is<bool>()) {
      dst.opentherm.dhwPresent = src["opentherm"]["dhwPresent"].as<bool>();
      changed = true;
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
          dst.emergency.useEquitherm = false;
          dst.emergency.usePid = false;
          dst.equitherm.enable = false;
          dst.pid.enable = false;
        }

        changed = true;
      }
    }


    // mqtt
    if (src["mqtt"]["enable"].is<bool>()) {
      bool value = src["mqtt"]["enable"].as<bool>();

      if (value != dst.mqtt.enable) {
        dst.mqtt.enable = value;
        changed = true;
      }
    }
    
    if (!src["mqtt"]["server"].isNull()) {
      String value = src["mqtt"]["server"].as<String>();

      if (value.length() < sizeof(dst.mqtt.server) && !String(dst.mqtt.server).equals(value)) {
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

      if (value.length() < sizeof(dst.mqtt.user) && !String(dst.mqtt.user).equals(value)) {
        strcpy(dst.mqtt.user, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["password"].isNull()) {
      String value = src["mqtt"]["password"].as<String>();

      if (value.length() < sizeof(dst.mqtt.password) && !String(dst.mqtt.password).equals(value)) {
        strcpy(dst.mqtt.password, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["prefix"].isNull()) {
      String value = src["mqtt"]["prefix"].as<String>();

      if (value.length() < sizeof(dst.mqtt.prefix) && !String(dst.mqtt.prefix).equals(value)) {
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
    if (src["emergency"]["enable"].is<bool>()) {
      bool value = src["emergency"]["enable"].as<bool>();

      if (value != dst.emergency.enable) {
        dst.emergency.enable = value;
        changed = true;
      }
    }

    if (!src["emergency"]["tresholdTime"].isNull()) {
      unsigned short value = src["emergency"]["tresholdTime"].as<unsigned short>();

      if (value >= 60 && value <= 1800 && value != dst.emergency.tresholdTime) {
        dst.emergency.tresholdTime = value;
        changed = true;
      }
    }

    if (src["emergency"]["useEquitherm"].is<bool>()) {
      bool value = src["emergency"]["useEquitherm"].as<bool>();

      if (!dst.opentherm.nativeHeatingControl && dst.sensors.outdoor.type != SensorType::MANUAL) {
        if (value != dst.emergency.useEquitherm) {
          dst.emergency.useEquitherm = value;
          changed = true;
        }

      } else if (dst.emergency.useEquitherm) {
        dst.emergency.useEquitherm = false;
        changed = true;
      }

      if (dst.emergency.useEquitherm && dst.emergency.usePid) {
        dst.emergency.usePid = false;
        changed = true;
      }
    }

    if (src["emergency"]["usePid"].is<bool>()) {
      bool value = src["emergency"]["usePid"].as<bool>();

      if (!dst.opentherm.nativeHeatingControl && dst.sensors.indoor.type != SensorType::MANUAL) {
        if (value != dst.emergency.usePid) {
          dst.emergency.usePid = value;
          changed = true;
        }

      } else if (dst.emergency.usePid) {
        dst.emergency.usePid = false;
        changed = true;
      }

      if (dst.emergency.usePid && dst.emergency.useEquitherm) {
        dst.emergency.useEquitherm = false;
        changed = true;
      }
    }

    if (src["emergency"]["onNetworkFault"].is<bool>()) {
      bool value = src["emergency"]["onNetworkFault"].as<bool>();

      if (value != dst.emergency.onNetworkFault) {
        dst.emergency.onNetworkFault = value;
        changed = true;
      }
    }

    if (src["emergency"]["onMqttFault"].is<bool>()) {
      bool value = src["emergency"]["onMqttFault"].as<bool>();

      if (value != dst.emergency.onMqttFault) {
        dst.emergency.onMqttFault = value;
        changed = true;
      }
    }
  }


  // pid
  if (src["pid"]["enable"].is<bool>()) {
    bool value = src["pid"]["enable"].as<bool>();
    
    if (!dst.opentherm.nativeHeatingControl) {
      if (value != dst.pid.enable) {
        dst.pid.enable = value;
        changed = true;
      }

    } else if (dst.pid.enable) {
      dst.pid.enable = false;
      changed = true;
    }
  }

  if (!src["pid"]["p_factor"].isNull()) {
    float value = src["pid"]["p_factor"].as<float>();

    if (value > 0 && value <= 1000 && fabs(value - dst.pid.p_factor) > 0.0001f) {
      dst.pid.p_factor = roundd(value, 3);
      changed = true;
    }
  }

  if (!src["pid"]["i_factor"].isNull()) {
    float value = src["pid"]["i_factor"].as<float>();

    if (value >= 0 && value <= 100 && fabs(value - dst.pid.i_factor) > 0.0001f) {
      dst.pid.i_factor = roundd(value, 4);
      changed = true;
    }
  }

  if (!src["pid"]["d_factor"].isNull()) {
    float value = src["pid"]["d_factor"].as<float>();

    if (value >= 0 && value <= 100000 && fabs(value - dst.pid.d_factor) > 0.0001f) {
      dst.pid.d_factor = roundd(value, 1);
      changed = true;
    }
  }

  if (!src["pid"]["dt"].isNull()) {
    unsigned short value = src["pid"]["dt"].as<unsigned short>();

    if (value >= 30 && value <= 600 && value != dst.pid.dt) {
      dst.pid.dt = value;
      changed = true;
    }
  }

  if (!src["pid"]["maxTemp"].isNull()) {
    unsigned char value = src["pid"]["maxTemp"].as<unsigned char>();

    if (isValidTemp(value, dst.system.unitSystem) && value > dst.pid.minTemp && value != dst.pid.maxTemp) {
      dst.pid.maxTemp = value;
      changed = true;
    }
  }

  if (!src["pid"]["minTemp"].isNull()) {
    unsigned char value = src["pid"]["minTemp"].as<unsigned char>();

    if (isValidTemp(value, dst.system.unitSystem) && value < dst.pid.maxTemp && value != dst.pid.minTemp) {
      dst.pid.minTemp = value;
      changed = true;
    }
  }


  // equitherm
  if (src["equitherm"]["enable"].is<bool>()) {
    bool value = src["equitherm"]["enable"].as<bool>();

    if (!dst.opentherm.nativeHeatingControl) {
      if (value != dst.equitherm.enable) {
        dst.equitherm.enable = value;
        changed = true;
      }
      
    } else if (dst.equitherm.enable) {
      dst.equitherm.enable = false;
      changed = true;
    }
  }

  if (!src["equitherm"]["n_factor"].isNull()) {
    float value = src["equitherm"]["n_factor"].as<float>();

    if (value > 0 && value <= 10 && fabs(value - dst.equitherm.n_factor) > 0.0001f) {
      dst.equitherm.n_factor = roundd(value, 3);
      changed = true;
    }
  }

  if (!src["equitherm"]["k_factor"].isNull()) {
    float value = src["equitherm"]["k_factor"].as<float>();

    if (value >= 0 && value <= 10 && fabs(value - dst.equitherm.k_factor) > 0.0001f) {
      dst.equitherm.k_factor = roundd(value, 3);
      changed = true;
    }
  }

  if (!src["equitherm"]["t_factor"].isNull()) {
    float value = src["equitherm"]["t_factor"].as<float>();

    if (value >= 0 && value <= 10 && fabs(value - dst.equitherm.t_factor) > 0.0001f) {
      dst.equitherm.t_factor = roundd(value, 3);
      changed = true;
    }
  }


  // heating
  if (src["heating"]["enable"].is<bool>()) {
    bool value = src["heating"]["enable"].as<bool>();

    if (value != dst.heating.enable) {
      dst.heating.enable = value;
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

    if (value >= 0 && value <= 5 && fabs(value - dst.heating.hysteresis) > 0.0001f) {
      dst.heating.hysteresis = roundd(value, 2);
      changed = true;
    }
  }

  if (!src["heating"]["minTemp"].isNull()) {
    unsigned char value = src["heating"]["minTemp"].as<unsigned char>();

    if (value != dst.heating.minTemp && value >= vars.parameters.heatingMinTemp && value < vars.parameters.heatingMaxTemp && value != dst.heating.minTemp) {
      dst.heating.minTemp = value;
      changed = true;
    }
  }

  if (!src["heating"]["maxTemp"].isNull()) {
    unsigned char value = src["heating"]["maxTemp"].as<unsigned char>();

    if (value != dst.heating.maxTemp && value > vars.parameters.heatingMinTemp && value <= vars.parameters.heatingMaxTemp && value != dst.heating.maxTemp) {
      dst.heating.maxTemp = value;
      changed = true;
    }
  }

  if (!src["heating"]["maxModulation"].isNull()) {
    unsigned char value = src["heating"]["maxModulation"].as<unsigned char>();

    if (value > 0 && value <= 100 && value != dst.heating.maxModulation) {
      dst.heating.maxModulation = value;
      changed = true;
    }
  }


  // dhw
  if (src["dhw"]["enable"].is<bool>()) {
    bool value = src["dhw"]["enable"].as<bool>();

    if (value != dst.dhw.enable) {
      dst.dhw.enable = value;
      changed = true;
    }
  }

  if (!src["dhw"]["minTemp"].isNull()) {
    unsigned char value = src["dhw"]["minTemp"].as<unsigned char>();

    if (value >= vars.parameters.dhwMinTemp && value < vars.parameters.dhwMaxTemp && value != dst.dhw.minTemp) {
      dst.dhw.minTemp = value;
      changed = true;
    }
  }

  if (!src["dhw"]["maxTemp"].isNull()) {
    unsigned char value = src["dhw"]["maxTemp"].as<unsigned char>();

    if (value > vars.parameters.dhwMinTemp && value <= vars.parameters.dhwMaxTemp && value != dst.dhw.maxTemp) {
      dst.dhw.maxTemp = value;
      changed = true;
    }
  }


  // sensors
  if (!src["sensors"]["outdoor"]["type"].isNull()) {
    byte value = src["sensors"]["outdoor"]["type"].as<unsigned char>();

    switch (value) {
      case static_cast<byte>(SensorType::BOILER):
        if (dst.sensors.outdoor.type != SensorType::BOILER) {
          dst.sensors.outdoor.type = SensorType::BOILER;
          changed = true;
        }
        break;

      case static_cast<byte>(SensorType::MANUAL):
        if (dst.sensors.outdoor.type != SensorType::MANUAL) {
          dst.sensors.outdoor.type = SensorType::MANUAL;
          dst.emergency.useEquitherm = false;
          changed = true;
        }
        break;

      case static_cast<byte>(SensorType::DS18B20):
        if (dst.sensors.outdoor.type != SensorType::DS18B20) {
          dst.sensors.outdoor.type = SensorType::DS18B20;
          changed = true;
        }
        break;

      default:
        break;
    }
  }

  if (!src["sensors"]["outdoor"]["gpio"].isNull()) {
    if (src["sensors"]["outdoor"]["gpio"].is<JsonString>() && src["sensors"]["outdoor"]["gpio"].as<JsonString>().size() == 0) {
      if (dst.sensors.outdoor.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.sensors.outdoor.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }
      
    } else {
      unsigned char value = src["sensors"]["outdoor"]["gpio"].as<unsigned char>();

      if (GPIO_IS_VALID(value) && value != dst.sensors.outdoor.gpio) {
        dst.sensors.outdoor.gpio = value;
        changed = true;
      }
    }
  }

  if (!src["sensors"]["outdoor"]["offset"].isNull()) {
    float value = src["sensors"]["outdoor"]["offset"].as<float>();

    if (value >= -10 && value <= 10 && fabs(value - dst.sensors.outdoor.offset) > 0.0001f) {
      dst.sensors.outdoor.offset = roundd(value, 2);
      changed = true;
    }
  }

  if (!src["sensors"]["indoor"]["type"].isNull()) {
    byte value = src["sensors"]["indoor"]["type"].as<unsigned char>();

    switch (value) {
      case static_cast<byte>(SensorType::MANUAL):
        if (dst.sensors.indoor.type != SensorType::MANUAL) {
          dst.sensors.indoor.type = SensorType::MANUAL;
          dst.emergency.usePid = false;
          dst.opentherm.nativeHeatingControl = false;
          changed = true;
        }
        break;

      case static_cast<byte>(SensorType::DS18B20):
        if (dst.sensors.indoor.type != SensorType::DS18B20) {
          dst.sensors.indoor.type = SensorType::DS18B20;
          changed = true;
        }
        break;

      #if USE_BLE
      case static_cast<byte>(SensorType::BLUETOOTH):
        if (dst.sensors.indoor.type != SensorType::BLUETOOTH) {
          dst.sensors.indoor.type = SensorType::BLUETOOTH;
          changed = true;
        }
        break;
      #endif

      default:
        break;
    }
  }

  if (!src["sensors"]["indoor"]["gpio"].isNull()) {
    if (src["sensors"]["indoor"]["gpio"].is<JsonString>() && src["sensors"]["indoor"]["gpio"].as<JsonString>().size() == 0) {
      if (dst.sensors.indoor.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.sensors.indoor.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }
      
    } else {
      unsigned char value = src["sensors"]["indoor"]["gpio"].as<unsigned char>();
      
      if (GPIO_IS_VALID(value) && value != dst.sensors.indoor.gpio) {
        dst.sensors.indoor.gpio = value;
        changed = true;
      }
    }
  }

  #if USE_BLE
  if (!src["sensors"]["indoor"]["bleAddresss"].isNull()) {
    String value = src["sensors"]["indoor"]["bleAddresss"].as<String>();
    int tmp[6];
    if(sscanf(value.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]) == 6) {
      for(uint8_t i = 0; i < 6; i++) {
        if (dst.sensors.indoor.bleAddresss[i] != (uint8_t) tmp[i]) {
          dst.sensors.indoor.bleAddresss[i] = (uint8_t) tmp[i];
          changed = true;
        }
      }
    }
  }
  #endif

  if (!src["sensors"]["indoor"]["offset"].isNull()) {
    float value = src["sensors"]["indoor"]["offset"].as<float>();

    if (value >= -10 && value <= 10 && fabs(value - dst.sensors.indoor.offset) > 0.0001f) {
      dst.sensors.indoor.offset = roundd(value, 2);
      changed = true;
    }
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
  }

  // relays
  if (!src["relay1"]["gpio"].isNull()) {
    if (src["relay1"]["gpio"].is<JsonString>() && src["relay1"]["gpio"].as<JsonString>().size() == 0) {
      if (dst.relay1.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.relay1.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }
      
    } else {
      unsigned char value = src["relay1"]["gpio"].as<unsigned char>();

      if (GPIO_IS_VALID(value) && value != dst.relay1.gpio) {
        dst.relay1.gpio = value;
        changed = true;
      }
    }
  }
  if (src["relay1"]["normal"].is<bool>()) {
    bool value = src["relay1"]["normal"].as<bool>();

    if (value != dst.relay1.normal) {
      dst.relay1.normal = value;
      changed = true;
    }
  }
  if (src["relay1"]["selfLocking"].is<bool>()) {
    bool value = src["relay1"]["selfLocking"].as<bool>();

    if (value != dst.relay1.selfLocking) {
      dst.relay1.selfLocking = value;
      changed = true;
    }
  }

  if (!src["relay2"]["gpio"].isNull()) {
    if (src["relay2"]["gpio"].is<JsonString>() && src["relay2"]["gpio"].as<JsonString>().size() == 0) {
      if (dst.relay2.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.relay2.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }
      
    } else {
      unsigned char value = src["relay2"]["gpio"].as<unsigned char>();

      if (GPIO_IS_VALID(value) && value != dst.relay2.gpio) {
        dst.relay2.gpio = value;
        changed = true;
      }
    }
  }
  if (src["relay2"]["normal"].is<bool>()) {
    bool value = src["relay2"]["normal"].as<bool>();

    if (value != dst.relay2.normal) {
      dst.relay2.normal = value;
      changed = true;
    }
  }
  if (src["relay2"]["selfLocking"].is<bool>()) {
    bool value = src["relay2"]["selfLocking"].as<bool>();

    if (value != dst.relay2.selfLocking) {
      dst.relay2.selfLocking = value;
      changed = true;
    }
  }

  if (!src["relay3"]["gpio"].isNull()) {
    if (src["relay3"]["gpio"].is<JsonString>() && src["relay3"]["gpio"].as<JsonString>().size() == 0) {
      if (dst.relay3.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.relay3.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }
      
    } else {
      unsigned char value = src["relay3"]["gpio"].as<unsigned char>();

      if (GPIO_IS_VALID(value) && value != dst.relay3.gpio) {
        dst.relay3.gpio = value;
        changed = true;
      }
    }
  }
  if (src["relay3"]["normal"].is<bool>()) {
    bool value = src["relay3"]["normal"].as<bool>();

    if (value != dst.relay3.normal) {
      dst.relay3.normal = value;
      changed = true;
    }
  }
  if (src["relay3"]["selfLocking"].is<bool>()) {
    bool value = src["relay3"]["selfLocking"].as<bool>();

    if (value != dst.relay3.selfLocking) {
      dst.relay3.selfLocking = value;
      changed = true;
    }
  }

  if (!src["relay4"]["gpio"].isNull()) {
    if (src["relay4"]["gpio"].is<JsonString>() && src["relay4"]["gpio"].as<JsonString>().size() == 0) {
      if (dst.relay4.gpio != GPIO_IS_NOT_CONFIGURED) {
        dst.relay4.gpio = GPIO_IS_NOT_CONFIGURED;
        changed = true;
      }
      
    } else {
      unsigned char value = src["relay4"]["gpio"].as<unsigned char>();

      if (GPIO_IS_VALID(value) && value != dst.relay4.gpio) {
        dst.relay4.gpio = value;
        changed = true;
      }
    }
  }
  if (src["relay4"]["normal"].is<bool>()) {
    bool value = src["relay4"]["normal"].as<bool>();

    if (value != dst.relay4.normal) {
      dst.relay4.normal = value;
      changed = true;
    }
  }
  if (src["relay4"]["selfLocking"].is<bool>()) {
    bool value = src["relay4"]["selfLocking"].as<bool>();

    if (value != dst.relay4.selfLocking) {
      dst.relay4.selfLocking = value;
      changed = true;
    }
  }

  // force check emergency target
  {
    float value = !src["emergency"]["target"].isNull() ? src["emergency"]["target"].as<float>() : dst.emergency.target;
    bool noRegulators = !dst.opentherm.nativeHeatingControl && !dst.emergency.useEquitherm && !dst.emergency.usePid;
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

    if (fabs(dst.emergency.target - value) > 0.0001f) {
      dst.emergency.target = roundd(value, 2);
      changed = true;
    }
  }

  // force check heating target
  {
    float value = !src["heating"]["target"].isNull() ? src["heating"]["target"].as<float>() : dst.heating.target;
    bool noRegulators = !dst.opentherm.nativeHeatingControl && !dst.equitherm.enable && !dst.pid.enable;
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

    if (fabs(dst.heating.target - value) > 0.0001f) {
      dst.heating.target = roundd(value, 2);
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

    if (fabs(dst.dhw.target - value) > 0.0001f) {
      dst.dhw.target = value;
      changed = true;
    }
  }

  return changed;
}

inline bool safeJsonToSettings(const JsonVariantConst src, Settings& dst) {
  return jsonToSettings(src, dst, true);
}

void varsToJson(const Variables& src, JsonVariant dst) {
  dst["states"]["otStatus"] = src.states.otStatus;
  dst["states"]["emergency"] = src.states.emergency;
  dst["states"]["heating"] = src.states.heating;
  dst["states"]["dhw"] = src.states.dhw;
  dst["states"]["flame"] = src.states.flame;
  dst["states"]["fault"] = src.states.fault;
  dst["states"]["diagnostic"] = src.states.diagnostic;
  dst["states"]["externalPump"] = src.states.externalPump;
  dst["states"]["mqtt"] = src.states.mqtt;

  dst["sensors"]["modulation"] = roundd(src.sensors.modulation, 2);
  dst["sensors"]["pressure"] = roundd(src.sensors.pressure, 2);
  dst["sensors"]["dhwFlowRate"] = roundd(src.sensors.dhwFlowRate, 2);
  dst["sensors"]["faultCode"] = src.sensors.faultCode;
  dst["sensors"]["rssi"] = src.sensors.rssi;
  dst["sensors"]["uptime"] = millis() / 1000ul;

  dst["temperatures"]["indoor"] = roundd(src.temperatures.indoor, 2);
  dst["temperatures"]["outdoor"] = roundd(src.temperatures.outdoor, 2);
  dst["temperatures"]["heating"] = roundd(src.temperatures.heating, 2);
  dst["temperatures"]["heatingReturn"] = roundd(src.temperatures.heatingReturn, 2);
  dst["temperatures"]["dhw"] = roundd(src.temperatures.dhw, 2);
  dst["temperatures"]["exhaust"] = roundd(src.temperatures.exhaust, 2);

  dst["parameters"]["heatingEnabled"] = src.parameters.heatingEnabled;
  dst["parameters"]["heatingMinTemp"] = src.parameters.heatingMinTemp;
  dst["parameters"]["heatingMaxTemp"] = src.parameters.heatingMaxTemp;
  dst["parameters"]["heatingSetpoint"] = roundd(src.parameters.heatingSetpoint, 2);
  dst["parameters"]["dhwMinTemp"] = src.parameters.dhwMinTemp;
  dst["parameters"]["dhwMaxTemp"] = src.parameters.dhwMaxTemp;

  dst["parameters"]["slaveMemberId"] = src.parameters.slaveMemberId;
  dst["parameters"]["slaveFlags"] = src.parameters.slaveFlags;
  dst["parameters"]["slaveType"] = src.parameters.slaveType;
  dst["parameters"]["slaveVersion"] = src.parameters.slaveVersion;
  dst["parameters"]["slaveOtVersion"] = src.parameters.slaveOtVersion;

  dst["relays"]["relay1"] = src.relays.relay1;
  dst["relays"]["relay2"] = src.relays.relay2;
  dst["relays"]["relay3"] = src.relays.relay3;
  dst["relays"]["relay4"] = src.relays.relay4;
}

bool jsonToVars(const JsonVariantConst src, Variables& dst) {
  bool changed = false;

  // temperatures
  if (!src["temperatures"]["indoor"].isNull()) {
    float value = src["temperatures"]["indoor"].as<float>();

    if (settings.sensors.indoor.type == SensorType::MANUAL && isValidTemp(value, settings.system.unitSystem, -99.9f, 99.9f)) {
      if (fabs(value - dst.temperatures.indoor) > 0.0001f) {
        dst.temperatures.indoor = roundd(value, 2);
        changed = true;
      }
    }
  }

  if (!src["temperatures"]["outdoor"].isNull()) {
    float value = src["temperatures"]["outdoor"].as<float>();

    if (settings.sensors.outdoor.type == SensorType::MANUAL && isValidTemp(value, settings.system.unitSystem, -99.9f, 99.9f)) {
      if (fabs(value - dst.temperatures.outdoor) > 0.0001f) {
        dst.temperatures.outdoor = roundd(value, 2);
        changed = true;
      }
    }
  }

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

  // relays
  if (src["relays"]["relay1"].is<bool>()) {
    bool value = src["relays"]["relay1"].as<bool>();
    if (value != dst.relays.relay1) {
      dst.relays.relay1 = value;
      changed = true;
    }
  }
  if (src["relays"]["relay2"].is<bool>()) {
    bool value = src["relays"]["relay2"].as<bool>();
    if (value != dst.relays.relay2) {
      dst.relays.relay2 = value;
      changed = true;
    }
  }
  if (src["relays"]["relay3"].is<bool>()) {
    bool value = src["relays"]["relay3"].as<bool>();
    if (value != dst.relays.relay3) {
      dst.relays.relay3 = value;
      changed = true;
    }
  }
  if (src["relays"]["relay4"].is<bool>()) {
    bool value = src["relays"]["relay4"].as<bool>();
    if (value != dst.relays.relay4) {
      dst.relays.relay4 = value;
      changed = true;
    }
  }

  return changed;
}