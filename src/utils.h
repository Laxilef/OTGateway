#include <Arduino.h>

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

size_t getTotalHeap() {
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

uint8_t getHeapFrag() {
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
    dst["system"]["useSerial"] = src.system.useSerial;
    dst["system"]["useTelnet"] = src.system.useTelnet;

    dst["portal"]["useAuth"] = src.portal.useAuth;
    dst["portal"]["login"] = src.portal.login;
    dst["portal"]["password"] = src.portal.password;

    dst["opentherm"]["inGpio"] = src.opentherm.inGpio;
    dst["opentherm"]["outGpio"] = src.opentherm.outGpio;
    dst["opentherm"]["memberIdCode"] = src.opentherm.memberIdCode;
    dst["opentherm"]["dhwPresent"] = src.opentherm.dhwPresent;
    dst["opentherm"]["summerWinterMode"] = src.opentherm.summerWinterMode;
    dst["opentherm"]["heatingCh2Enabled"] = src.opentherm.heatingCh2Enabled;
    dst["opentherm"]["heatingCh1ToCh2"] = src.opentherm.heatingCh1ToCh2;
    dst["opentherm"]["dhwToCh2"] = src.opentherm.dhwToCh2;
    dst["opentherm"]["dhwBlocking"] = src.opentherm.dhwBlocking;
    dst["opentherm"]["modulationSyncWithHeating"] = src.opentherm.modulationSyncWithHeating;

    dst["mqtt"]["server"] = src.mqtt.server;
    dst["mqtt"]["port"] = src.mqtt.port;
    dst["mqtt"]["user"] = src.mqtt.user;
    dst["mqtt"]["password"] = src.mqtt.password;
    dst["mqtt"]["prefix"] = src.mqtt.prefix;
    dst["mqtt"]["interval"] = src.mqtt.interval;
  }

  dst["emergency"]["enable"] = src.emergency.enable;
  dst["emergency"]["target"] = roundd(src.emergency.target, 2);
  dst["emergency"]["useEquitherm"] = src.emergency.useEquitherm;
  dst["emergency"]["usePid"] = src.emergency.usePid;

  dst["heating"]["enable"] = src.heating.enable;
  dst["heating"]["turbo"] = src.heating.turbo;
  dst["heating"]["target"] = roundd(src.heating.target, 2);
  dst["heating"]["hysteresis"] = roundd(src.heating.hysteresis, 2);
  dst["heating"]["minTemp"] = src.heating.minTemp;
  dst["heating"]["maxTemp"] = src.heating.maxTemp;
  dst["heating"]["maxModulation"] = src.heating.maxModulation;

  dst["dhw"]["enable"] = src.dhw.enable;
  dst["dhw"]["target"] = src.dhw.target;
  dst["dhw"]["minTemp"] = src.dhw.minTemp;
  dst["dhw"]["maxTemp"] = src.dhw.maxTemp;

  dst["pid"]["enable"] = src.pid.enable;
  dst["pid"]["p_factor"] = roundd(src.pid.p_factor, 3);
  dst["pid"]["i_factor"] = roundd(src.pid.i_factor, 3);
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

  if (!safe) {
    dst["externalPump"]["use"] = src.externalPump.use;
    dst["externalPump"]["gpio"] = src.externalPump.gpio;
    dst["externalPump"]["postCirculationTime"] = roundd(src.externalPump.postCirculationTime / 60, 0);
    dst["externalPump"]["antiStuckInterval"] = roundd(src.externalPump.antiStuckInterval / 86400, 0);
    dst["externalPump"]["antiStuckTime"] = roundd(src.externalPump.antiStuckTime / 60, 0);
  }
}

void safeSettingsToJson(const Settings& src, JsonVariant dst) {
  settingsToJson(src, dst, true);
}

bool jsonToSettings(const JsonVariantConst src, Settings& dst, bool safe = false) {
  bool changed = false;

  if (!safe) {
    // system
    if (src["system"]["debug"].is<bool>()) {
      dst.system.debug = src["system"]["debug"].as<bool>();
      changed = true;
    }

    if (src["system"]["useSerial"].is<bool>()) {
      dst.system.useSerial = src["system"]["useSerial"].as<bool>();
      changed = true;
    }

    if (src["system"]["useTelnet"].is<bool>()) {
      dst.system.useTelnet = src["system"]["useTelnet"].as<bool>();
      changed = true;
    }


    // portal
    if (src["portal"]["useAuth"].is<bool>()) {
      dst.portal.useAuth = src["portal"]["useAuth"].as<bool>();
      changed = true;
    }

    if (!src["portal"]["login"].isNull()) {
      String value = src["portal"]["login"].as<String>();

      if (value.length() < sizeof(dst.portal.login)) {
        strcpy(dst.portal.login, value.c_str());
        changed = true;
      }
    }

    if (!src["portal"]["password"].isNull()) {
      String value = src["portal"]["password"].as<String>();

      if (value.length() < sizeof(dst.portal.password)) {
        strcpy(dst.portal.password, value.c_str());
        changed = true;
      }
    }


    // opentherm
    if (!src["opentherm"]["inGpio"].isNull()) {
      if (src["opentherm"]["inGpio"].is<JsonString>() && src["opentherm"]["inGpio"].as<JsonString>().size() == 0) {
        if (dst.opentherm.inGpio != GPIO_IS_NOT_CONFIGURED) {
          dst.opentherm.inGpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }
        
      } else {
        unsigned char value = src["opentherm"]["inGpio"].as<unsigned char>();

        if (value >= 0 && value <= 254) {
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

        if (value >= 0 && value <= 254) {
          dst.opentherm.outGpio = value;
          changed = true;
        }
      }
    }

    if (!src["opentherm"]["memberIdCode"].isNull()) {
      unsigned int value = src["opentherm"]["memberIdCode"].as<unsigned int>();

      if (value >= 0 && value < 65536) {
        dst.opentherm.memberIdCode = value;
        changed = true;
      }
    }

    if (src["opentherm"]["dhwPresent"].is<bool>()) {
      dst.opentherm.dhwPresent = src["opentherm"]["dhwPresent"].as<bool>();
      changed = true;
    }

    if (src["opentherm"]["summerWinterMode"].is<bool>()) {
      dst.opentherm.summerWinterMode = src["opentherm"]["summerWinterMode"].as<bool>();
      changed = true;
    }

    if (src["opentherm"]["heatingCh2Enabled"].is<bool>()) {
      dst.opentherm.heatingCh2Enabled = src["opentherm"]["heatingCh2Enabled"].as<bool>();

      if (dst.opentherm.heatingCh2Enabled) {
        dst.opentherm.heatingCh1ToCh2 = false;
        dst.opentherm.dhwToCh2 = false;
      }

      changed = true;
    }

    if (src["opentherm"]["heatingCh1ToCh2"].is<bool>()) {
      dst.opentherm.heatingCh1ToCh2 = src["opentherm"]["heatingCh1ToCh2"].as<bool>();

      if (dst.opentherm.heatingCh1ToCh2) {
        dst.opentherm.heatingCh2Enabled = false;
        dst.opentherm.dhwToCh2 = false;
      }

      changed = true;
    }

    if (src["opentherm"]["dhwToCh2"].is<bool>()) {
      dst.opentherm.dhwToCh2 = src["opentherm"]["dhwToCh2"].as<bool>();

      if (dst.opentherm.dhwToCh2) {
        dst.opentherm.heatingCh2Enabled = false;
        dst.opentherm.heatingCh1ToCh2 = false;
      }

      changed = true;
    }

    if (src["opentherm"]["dhwBlocking"].is<bool>()) {
      dst.opentherm.dhwBlocking = src["opentherm"]["dhwBlocking"].as<bool>();
      changed = true;
    }

    if (src["opentherm"]["modulationSyncWithHeating"].is<bool>()) {
      dst.opentherm.modulationSyncWithHeating = src["opentherm"]["modulationSyncWithHeating"].as<bool>();
      changed = true;
    }


    // mqtt
    if (!src["mqtt"]["server"].isNull()) {
      String value = src["mqtt"]["server"].as<String>();

      if (value.length() < sizeof(dst.mqtt.server)) {
        strcpy(dst.mqtt.server, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["port"].isNull()) {
      unsigned short value = src["mqtt"]["port"].as<unsigned short>();

      if (value > 0 && value <= 65535) {
        dst.mqtt.port = value;
        changed = true;
      }
    }

    if (!src["mqtt"]["user"].isNull()) {
      String value = src["mqtt"]["user"].as<String>();

      if (value.length() < sizeof(dst.mqtt.user)) {
        strcpy(dst.mqtt.user, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["password"].isNull()) {
      String value = src["mqtt"]["password"].as<String>();

      if (value.length() < sizeof(dst.mqtt.password)) {
        strcpy(dst.mqtt.password, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["prefix"].isNull()) {
      String value = src["mqtt"]["prefix"].as<String>();

      if (value.length() < sizeof(dst.mqtt.prefix)) {
        strcpy(dst.mqtt.prefix, value.c_str());
        changed = true;
      }
    }

    if (!src["mqtt"]["interval"].isNull()) {
      unsigned short value = src["mqtt"]["interval"].as<unsigned short>();

      if (value >= 3 && value <= 60) {
        dst.mqtt.interval = value;
        changed = true;
      }
    }
  }


  // emergency
  if (src["emergency"]["enable"].is<bool>()) {
    dst.emergency.enable = src["emergency"]["enable"].as<bool>();
    changed = true;
  }

  if (!src["emergency"]["target"].isNull()) {
    double value = src["emergency"]["target"].as<double>();

    if (value > 0 && value < 100) {
      dst.emergency.target = roundd(value, 2);
      changed = true;
    }
  }

  if (src["emergency"]["useEquitherm"].is<bool>()) {
    if (dst.sensors.outdoor.type != SensorType::MANUAL) {
      dst.emergency.useEquitherm = src["emergency"]["useEquitherm"].as<bool>();

    } else {
      dst.emergency.useEquitherm = false;
    }

    if (dst.emergency.useEquitherm && dst.emergency.usePid) {
      dst.emergency.usePid = false;
    }

    changed = true;
  }

  if (src["emergency"]["usePid"].is<bool>()) {
    if (dst.sensors.indoor.type != SensorType::MANUAL) {
      dst.emergency.usePid = src["emergency"]["usePid"].as<bool>();

    } else {
      dst.emergency.usePid = false;
    }

    if (dst.emergency.usePid && dst.emergency.useEquitherm) {
      dst.emergency.useEquitherm = false;
    }

    changed = true;
  }


  // heating
  if (src["heating"]["enable"].is<bool>()) {
    dst.heating.enable = src["heating"]["enable"].as<bool>();
    changed = true;
  }

  if (src["heating"]["turbo"].is<bool>()) {
    dst.heating.turbo = src["heating"]["turbo"].as<bool>();
    changed = true;
  }

  if (!src["heating"]["target"].isNull()) {
    double value = src["heating"]["target"].as<double>();

    if (value > 0 && value < 100) {
      dst.heating.target = roundd(value, 2);
      changed = true;
    }
  }

  if (!src["heating"]["hysteresis"].isNull()) {
    double value = src["heating"]["hysteresis"].as<double>();

    if (value >= 0 && value <= 5) {
      dst.heating.hysteresis = roundd(value, 2);
      changed = true;
    }
  }

  if (!src["heating"]["minTemp"].isNull()) {
    unsigned char value = src["heating"]["minTemp"].as<unsigned char>();

    if (value >= vars.parameters.heatingMinTemp && value <= vars.parameters.heatingMaxTemp) {
      dst.heating.minTemp = value;
      changed = true;
    }
  }

  if (!src["heating"]["maxTemp"].isNull()) {
    unsigned char value = src["heating"]["maxTemp"].as<unsigned char>();

    if (value >= vars.parameters.heatingMinTemp && value <= vars.parameters.heatingMaxTemp) {
      dst.heating.maxTemp = value;
      changed = true;
    }
  }

  if (!src["heating"]["maxModulation"].isNull()) {
    unsigned char value = src["heating"]["maxModulation"].as<unsigned char>();

    if (value > 0 && value <= 100) {
      dst.heating.maxModulation = value;
      changed = true;
    }
  }


  // dhw
  if (src["dhw"]["enable"].is<bool>()) {
    dst.dhw.enable = src["dhw"]["enable"].as<bool>();
    changed = true;
  }

  if (!src["dhw"]["target"].isNull()) {
    unsigned char value = src["dhw"]["target"].as<unsigned char>();

    if (value >= 0 && value < 100) {
      dst.dhw.target = value;
      changed = true;
    }
  }

  if (!src["dhw"]["minTemp"].isNull()) {
    unsigned char value = src["dhw"]["minTemp"].as<unsigned char>();

    if (value >= vars.parameters.dhwMinTemp && value <= vars.parameters.dhwMaxTemp) {
      dst.dhw.minTemp = value;
      changed = true;
    }
  }

  if (!src["dhw"]["maxTemp"].isNull()) {
    unsigned char value = src["dhw"]["maxTemp"].as<unsigned char>();

    if (value >= vars.parameters.dhwMinTemp && value <= vars.parameters.dhwMaxTemp) {
      dst.dhw.maxTemp = value;
      changed = true;
    }
  }


  // pid
  if (src["pid"]["enable"].is<bool>()) {
    dst.pid.enable = src["pid"]["enable"].as<bool>();
    changed = true;
  }

  if (!src["pid"]["p_factor"].isNull()) {
    double value = src["pid"]["p_factor"].as<double>();

    if (value > 0 && value <= 1000) {
      dst.pid.p_factor = roundd(value, 3);
      changed = true;
    }
  }

  if (!src["pid"]["i_factor"].isNull()) {
    double value = src["pid"]["i_factor"].as<double>();

    if (value >= 0 && value <= 100) {
      dst.pid.i_factor = roundd(value, 3);
      changed = true;
    }
  }

  if (!src["pid"]["d_factor"].isNull()) {
    double value = src["pid"]["d_factor"].as<double>();

    if (value >= 0 && value <= 100000) {
      dst.pid.d_factor = roundd(value, 1);
      changed = true;
    }
  }

  if (!src["pid"]["dt"].isNull()) {
    unsigned short value = src["pid"]["dt"].as<unsigned short>();

    if (value >= 30 && value <= 600) {
      dst.pid.dt = value;
      changed = true;
    }
  }

  if (!src["pid"]["maxTemp"].isNull()) {
    unsigned char value = src["pid"]["maxTemp"].as<unsigned char>();

    if (value > 0 && value <= 100 && value > dst.pid.minTemp) {
      dst.pid.maxTemp = value;
      changed = true;
    }
  }

  if (!src["pid"]["minTemp"].isNull()) {
    unsigned char value = src["pid"]["minTemp"].as<unsigned char>();

    if (value >= 0 && value < 100 && value < dst.pid.maxTemp) {
      dst.pid.minTemp = value;
      changed = true;
    }
  }


  // equitherm
  if (src["equitherm"]["enable"].is<bool>()) {
    dst.equitherm.enable = src["equitherm"]["enable"].as<bool>();
    changed = true;
  }

  if (!src["equitherm"]["n_factor"].isNull()) {
    double value = src["equitherm"]["n_factor"].as<double>();

    if (value > 0 && value <= 10) {
      dst.equitherm.n_factor = roundd(value, 3);
      changed = true;
    }
  }

  if (!src["equitherm"]["k_factor"].isNull()) {
    double value = src["equitherm"]["k_factor"].as<double>();

    if (value >= 0 && value <= 10) {
      dst.equitherm.k_factor = roundd(value, 3);
      changed = true;
    }
  }

  if (!src["equitherm"]["t_factor"].isNull()) {
    double value = src["equitherm"]["t_factor"].as<double>();

    if (value >= 0 && value <= 10) {
      dst.equitherm.t_factor = roundd(value, 3);
      changed = true;
    }
  }


  // sensors
  if (!src["sensors"]["outdoor"]["type"].isNull()) {
    byte value = src["sensors"]["outdoor"]["type"].as<unsigned char>();

    switch (value) {
      case static_cast<byte>(SensorType::BOILER):
        dst.sensors.outdoor.type = SensorType::BOILER;
        changed = true;
        break;

      case static_cast<byte>(SensorType::MANUAL):
        dst.sensors.outdoor.type = SensorType::MANUAL;
        dst.emergency.useEquitherm = false;
        changed = true;
        break;

      case static_cast<byte>(SensorType::DS18B20):
        dst.sensors.outdoor.type = SensorType::DS18B20;
        changed = true;
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

      if (value >= 0 && value <= 254) {
        dst.sensors.outdoor.gpio = value;
        changed = true;
      }
    }
  }

  if (!src["sensors"]["outdoor"]["offset"].isNull()) {
    double value = src["sensors"]["outdoor"]["offset"].as<double>();

    if (value >= -10 && value <= 10) {
      dst.sensors.outdoor.offset = roundd(value, 2);
      changed = true;
    }
  }

  if (!src["sensors"]["indoor"]["type"].isNull()) {
    byte value = src["sensors"]["indoor"]["type"].as<unsigned char>();


    switch (value) {
      case static_cast<byte>(SensorType::BOILER):
        dst.sensors.indoor.type = SensorType::BOILER;
        changed = true;
        break;

      case static_cast<byte>(SensorType::MANUAL):
        dst.sensors.indoor.type = SensorType::MANUAL;
        dst.emergency.useEquitherm = false;
        changed = true;
        break;

      case static_cast<byte>(SensorType::DS18B20):
        dst.sensors.indoor.type = SensorType::DS18B20;
        changed = true;
        break;

      #if USE_BLE
      case static_cast<byte>(SensorType::BLUETOOTH):
        dst.sensors.indoor.type = SensorType::BLUETOOTH;
        changed = true;
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
      
      if (value >= 0 && value <= 254) {
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
        dst.sensors.indoor.bleAddresss[i] = (uint8_t) tmp[i];
      }

      changed = true;
    }
  }
  #endif

  if (!src["sensors"]["indoor"]["offset"].isNull()) {
    double value = src["sensors"]["indoor"]["offset"].as<double>();

    if (value >= -10 && value <= 10) {
      dst.sensors.indoor.offset = roundd(value, 2);
      changed = true;
    }
  }


  if (!safe) {
    // external pump
    if (src["externalPump"]["use"].is<bool>()) {
      dst.externalPump.use = src["externalPump"]["use"].as<bool>();
      changed = true;
    }

    if (!src["externalPump"]["gpio"].isNull()) {
      if (src["externalPump"]["gpio"].is<JsonString>() && src["externalPump"]["gpio"].as<JsonString>().size() == 0) {
        if (dst.externalPump.gpio != GPIO_IS_NOT_CONFIGURED) {
          dst.externalPump.gpio = GPIO_IS_NOT_CONFIGURED;
          changed = true;
        }
        
      } else {
        unsigned char value = src["externalPump"]["gpio"].as<unsigned char>();

        if (value >= 0 && value <= 254) {
          dst.externalPump.gpio = value;
          changed = true;
        }
      }
    }

    if (!src["externalPump"]["postCirculationTime"].isNull()) {
      unsigned short value = src["externalPump"]["postCirculationTime"].as<unsigned short>();

      if (value >= 0 && value <= 120) {
        dst.externalPump.postCirculationTime = value * 60;
        changed = true;
      }
    }

    if (!src["externalPump"]["antiStuckInterval"].isNull()) {
      unsigned int value = src["externalPump"]["antiStuckInterval"].as<unsigned int>();

      if (value >= 0 && value <= 366) {
        dst.externalPump.antiStuckInterval = value * 86400;
        changed = true;
      }
    }

    if (!src["externalPump"]["antiStuckTime"].isNull()) {
      unsigned short value = src["externalPump"]["antiStuckTime"].as<unsigned short>();

      if (value >= 0 && value <= 20) {
        dst.externalPump.antiStuckTime = value * 60;
        changed = true;
      }
    }
  }

  return changed;
}

bool safeJsonToSettings(const JsonVariantConst src, Settings& dst) {
  return jsonToSettings(src, dst, true);
}

void varsToJson(const Variables& src, JsonVariant dst) {
  dst["tuning"]["enable"] = src.tuning.enable;
  dst["tuning"]["regulator"] = src.tuning.regulator;

  dst["states"]["otStatus"] = src.states.otStatus;
  dst["states"]["emergency"] = src.states.emergency;
  dst["states"]["heating"] = src.states.heating;
  dst["states"]["dhw"] = src.states.dhw;
  dst["states"]["flame"] = src.states.flame;
  dst["states"]["fault"] = src.states.fault;
  dst["states"]["diagnostic"] = src.states.diagnostic;
  dst["states"]["externalPump"] = src.states.externalPump;

  dst["sensors"]["modulation"] = roundd(src.sensors.modulation, 2);
  dst["sensors"]["pressure"] = roundd(src.sensors.pressure, 2);
  dst["sensors"]["dhwFlowRate"] = src.sensors.dhwFlowRate;
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
  dst["parameters"]["heatingSetpoint"] = src.parameters.heatingSetpoint;
  dst["parameters"]["dhwMinTemp"] = src.parameters.dhwMinTemp;
  dst["parameters"]["dhwMaxTemp"] = src.parameters.dhwMaxTemp;
}

bool jsonToVars(const JsonVariantConst src, Variables& dst) {
  bool changed = false;

  // tuning
  if (src["tuning"]["enable"].is<bool>()) {
    dst.tuning.enable = src["tuning"]["enable"].as<bool>();
    changed = true;
  }

  if (!src["tuning"]["regulator"].isNull()) {
    unsigned char value = src["tuning"]["regulator"].as<unsigned char>();

    if (value >= 0 && value <= 1) {
      dst.tuning.regulator = value;
      changed = true;
    }
  }


  // temperatures
  if (!src["temperatures"]["indoor"].isNull()) {
    double value = src["temperatures"]["indoor"].as<double>();

    if (settings.sensors.indoor.type == SensorType::MANUAL && value > -100 && value < 100) {
      dst.temperatures.indoor = roundd(value, 2);
      changed = true;
    }
  }

  if (!src["temperatures"]["outdoor"].isNull()) {
    double value = src["temperatures"]["outdoor"].as<double>();

    if (settings.sensors.outdoor.type == SensorType::MANUAL && value > -100 && value < 100) {
      dst.temperatures.outdoor = roundd(value, 2);
      changed = true;
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

  return changed;
}