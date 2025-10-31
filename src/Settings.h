struct NetworkSettings {
  char hostname[25] = DEFAULT_HOSTNAME;
  bool useDhcp = true;

  struct {
    char ip[16] = "192.168.0.100";
    char gateway[16] = "192.168.0.1";
    char subnet[16] = "255.255.255.0";
    char dns[16] = "192.168.0.1";
  } staticConfig;

  struct {
    char ssid[33] = DEFAULT_AP_SSID;
    char password[65] = DEFAULT_AP_PASSWORD;
    uint8_t channel = 6;
  } ap;

  struct {
    char ssid[33] = DEFAULT_STA_SSID;
    char password[65] = DEFAULT_STA_PASSWORD;
    uint8_t channel = 0;
  } sta;
} networkSettings;

struct Settings {
  struct {
    uint8_t logLevel = DEFAULT_LOG_LEVEL;

    struct {
      bool enabled = DEFAULT_SERIAL_ENABLED;
      unsigned int baudrate = DEFAULT_SERIAL_BAUD;
    } serial;

    struct {
      bool enabled = DEFAULT_TELNET_ENABLED;
      unsigned short port = DEFAULT_TELNET_PORT;
    } telnet;

    struct {
      char server[49] = "pool.ntp.org";
      char timezone[49] = "UTC0";
    } ntp;

    UnitSystem unitSystem = UnitSystem::METRIC;
    uint8_t statusLedGpio = DEFAULT_STATUS_LED_GPIO;
  } system;

  struct {
    bool auth = false;
    char login[13] = DEFAULT_PORTAL_LOGIN;
    char password[33] = DEFAULT_PORTAL_PASSWORD;
    bool mdns = true;
  } portal;

  struct {
    UnitSystem unitSystem = UnitSystem::METRIC;
    uint8_t inGpio = DEFAULT_OT_IN_GPIO;
    uint8_t outGpio = DEFAULT_OT_OUT_GPIO;
    uint8_t rxLedGpio = DEFAULT_OT_RX_LED_GPIO;
    uint8_t memberId = 0;
    uint8_t flags = 0;
    float minPower = 0.0f;
    float maxPower = 0.0f;

    struct {
      bool dhwSupport = true;
      bool coolingSupport = false;
      bool summerWinterMode = false;
      bool heatingStateToSummerWinterMode = false;
      bool ch2AlwaysEnabled = true;
      bool heatingToCh2 = false;
      bool dhwToCh2 = false;
      bool dhwBlocking = false;
      bool dhwStateAsDhwBlocking = false;
      bool maxTempSyncWithTargetTemp = true;
      bool getMinMaxTemp = true;
      bool ignoreDiagState = false;
      bool autoFaultReset = false;
      bool autoDiagReset = false;
      bool setDateAndTime = false;
      bool nativeHeatingControl = false;
      bool immergasFix = false;
    } options;
  } opentherm;

  struct {
    bool enabled = DEFAULT_MQTT_ENABLED;
    char server[81] = DEFAULT_MQTT_SERVER;
    unsigned short port = DEFAULT_MQTT_PORT;
    char user[33] = DEFAULT_MQTT_USER;
    char password[33] = DEFAULT_MQTT_PASSWORD;
    char prefix[33] = DEFAULT_MQTT_PREFIX;
    unsigned short interval = 5;
    bool homeAssistantDiscovery = true;
  } mqtt;

  struct {
    float target = DEFAULT_HEATING_TARGET_TEMP;
    unsigned short tresholdTime = 120;
  } emergency;

  struct {
    bool enabled = true;
    bool turbo = false;
    float target = DEFAULT_HEATING_TARGET_TEMP;
    float hysteresis = 0.5f;
    float turboFactor = 7.5f;
    uint8_t minTemp = DEFAULT_HEATING_MIN_TEMP;
    uint8_t maxTemp = DEFAULT_HEATING_MAX_TEMP;
    uint8_t maxModulation = 100;

    struct {
      uint8_t highTemp = 95;
      uint8_t lowTemp = 90;
    } overheatProtection;

    struct {
      uint8_t lowTemp = 10;
      unsigned short thresholdTime = 600;
    } freezeProtection;
  } heating;

  struct {
    bool enabled = true;
    float target = DEFAULT_DHW_TARGET_TEMP;
    uint8_t minTemp = DEFAULT_DHW_MIN_TEMP;
    uint8_t maxTemp = DEFAULT_DHW_MAX_TEMP;
    uint8_t maxModulation = 100;
    
    struct {
      uint8_t highTemp = 95;
      uint8_t lowTemp = 90;
    } overheatProtection;
  } dhw;

  struct {
    bool enabled = false;
    float p_factor = 2.0f;
    float i_factor = 0.002f;
    float d_factor = 0.0f;
    unsigned short dt = 300;
    short minTemp = 0;
    short maxTemp = DEFAULT_HEATING_MAX_TEMP;

    struct {
      bool enabled = true;
      float p_multiplier = 1.0f;
      float i_multiplier = 0.05f;
      float d_multiplier = 1.0f;
      float thresholdHigh = 0.5f;
      float thresholdLow = 1.0f;
    } deadband;
  } pid;

  struct {
    bool enabled = false;
    float n_factor = 0.7f;
    float k_factor = 3.0f;
    float t_factor = 2.0f;
  } equitherm;

  struct {
    bool use = false;
    uint8_t gpio = DEFAULT_EXT_PUMP_GPIO;
    bool invertState = false;
    unsigned short postCirculationTime = 600;
    unsigned int antiStuckInterval = 2592000;
    unsigned short antiStuckTime = 300;
  } externalPump;

  struct {
    struct {
      bool enabled = false;
      uint8_t gpio = GPIO_IS_NOT_CONFIGURED;
      bool invertState = false;
      unsigned short thresholdTime = 60;
    } input;

    struct {
      bool enabled = false;
      uint8_t gpio = GPIO_IS_NOT_CONFIGURED;
      bool invertState = false;
      unsigned short thresholdTime = 60;
      bool onFault = true;
      bool onLossConnection = true;
      bool onEnabledHeating = false;
    } output;
  } cascadeControl;

  char validationValue[8] = SETTINGS_VALID_VALUE;
} settings;

Sensors::Settings sensorsSettings[SENSORS_AMOUNT] = {
  {
    false,
    "Outdoor temp",
    Sensors::Purpose::OUTDOOR_TEMP,
    Sensors::Type::OT_OUTDOOR_TEMP,
    DEFAULT_SENSOR_OUTDOOR_GPIO
  },
  {
    false,
    "Indoor temp",
    Sensors::Purpose::INDOOR_TEMP,
    Sensors::Type::DALLAS_TEMP,
    DEFAULT_SENSOR_INDOOR_GPIO
  },
  {
    true,
    "Heating temp",
    Sensors::Purpose::HEATING_TEMP,
    Sensors::Type::OT_HEATING_TEMP,
  },
  {
    true,
    "Heating return temp",
    Sensors::Purpose::HEATING_RETURN_TEMP,
    Sensors::Type::OT_HEATING_RETURN_TEMP,
  },
  {
    true,
    "Heating setpoint temp",
    Sensors::Purpose::TEMPERATURE,
    Sensors::Type::HEATING_SETPOINT_TEMP,
  },
  {
    true,
    "DHW temp",
    Sensors::Purpose::DHW_TEMP,
    Sensors::Type::OT_DHW_TEMP,
  },
  {
    true,
    "DHW flow rate",
    Sensors::Purpose::DHW_FLOW_RATE,
    Sensors::Type::OT_DHW_FLOW_RATE,
  },
  {
    true,
    "Exhaust temp",
    Sensors::Purpose::EXHAUST_TEMP,
    Sensors::Type::OT_EXHAUST_TEMP,
  },
  {
    true,
    "Pressure",
    Sensors::Purpose::PRESSURE,
    Sensors::Type::OT_PRESSURE,
  },
  {
    true,
    "Modulation level",
    Sensors::Purpose::MODULATION_LEVEL,
    Sensors::Type::OT_MODULATION_LEVEL,
  },
  {
    true,
    "Power",
    Sensors::Purpose::POWER,
    Sensors::Type::OT_CURRENT_POWER,
  }
};

struct Variables {
  struct {
    bool connected = false;
    int8_t rssi = 0;
  } network;

  struct {
    bool connected = false;
  } mqtt;

  struct {
    bool state = false;
  } emergency;

  struct {
    bool state = false;
    unsigned long lastEnabledTime = 0;
  } externalPump;

  struct {
    bool input = false;
    bool output = false;
  } cascadeControl;

  struct {
    uint8_t memberId = 0;
    uint8_t flags = 0;
    uint8_t type = 0;
    uint8_t appVersion = 0;
    float protocolVersion = 0.0f;

    struct {
      bool blocking = false;
      bool enabled = false;
      bool indoorTempControl = false;
      bool overheat = false;
      float setpointTemp = 0.0f;
      float targetTemp = 0.0f;
      float currentTemp = 0.0f;
      float returnTemp = 0.0f;
      float indoorTemp = 0.0f;
      float outdoorTemp = 0.0f;
      float minTemp = 0.0f;
      float maxTemp = 0.0f;
    } heating;

    struct {
      bool enabled = false;
      bool overheat = false;
      float targetTemp = 0.0f;
      float currentTemp = 0.0f;
      float returnTemp = 0.0f;
    } dhw;

    struct {
      bool enabled = false;
      float targetTemp = 0.0f;
    } ch2;
  } master;

  struct {
    uint8_t memberId = 0;
    uint8_t flags = 0;
    uint8_t type = 0;
    uint8_t appVersion = 0;
    float protocolVersion = 0.0f;

    bool connected = false;
    bool flame = false;
    float pressure = 0.0f;
    float heatExchangerTemp = 0.0f;

    struct {
      bool active = false;
      uint8_t setpoint = 0;
    } cooling;

    struct {
      bool active = false;
      uint8_t code = 0;
    } fault;

    struct {
      bool active = false;
      uint16_t code = 0;
    } diag;

    struct {
      uint8_t current = 0;
      uint8_t min = 0;
      uint8_t max = 100;
    } modulation;

    struct {
      float current = 0.0f;
      float min = 0.0f;
      float max = 0.0f;
    } power;

    struct {
      float temp = 0.0f;
      uint16_t co2 = 0;
      uint16_t fanSpeed = 0;
    } exhaust;

    struct {
      float storage = 0.0f;
      float collector = 0.0f;
    } solar;

    struct {
      uint8_t setpoint = 0;
      uint8_t current = 0;
      uint16_t supply = 0;
    } fanSpeed;

    struct {
      uint16_t burnerStarts = 0;
      uint16_t dhwBurnerStarts = 0;
      uint16_t heatingPumpStarts = 0;
      uint16_t dhwPumpStarts = 0;
      uint16_t burnerHours = 0;
      uint16_t dhwBurnerHours = 0;
      uint16_t heatingPumpHours = 0;
      uint16_t dhwPumpHours = 0;
    } stats;

    struct {
      bool active = false;
      bool enabled = false;
      float targetTemp = 0.0f;
      float currentTemp = 0.0f;
      float returnTemp = 0.0f;
      float indoorTemp = 0.0f;
      float outdoorTemp = 0.0f;
      uint8_t minTemp = DEFAULT_HEATING_MIN_TEMP;
      uint8_t maxTemp = DEFAULT_HEATING_MAX_TEMP;
    } heating;

    struct {
      bool active = false;
      bool enabled = false;
      float targetTemp = 0.0f;
      float currentTemp = 0.0f;
      float currentTemp2 = 0.0f;
      float returnTemp = 0.0f;
      float flowRate = 0.0f;
      uint8_t minTemp = DEFAULT_DHW_MIN_TEMP;
      uint8_t maxTemp = DEFAULT_DHW_MAX_TEMP;
    } dhw;

    struct {
      bool active = false;
      bool enabled = false;
      float targetTemp = 0.0f;
      float currentTemp = 0.0f;
      float indoorTemp = 0.0f;
    } ch2;
  } slave;

  struct {
    bool restart = false;
    bool resetFault = false;
    bool resetDiagnostic = false;
  } actions;

  struct {
    bool restarting = false;
    bool upgrading = false;
  } states;
} vars;