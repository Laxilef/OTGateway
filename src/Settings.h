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
    byte channel = 6;
  } ap;

  struct {
    char ssid[33] = DEFAULT_STA_SSID;
    char password[65] = DEFAULT_STA_PASSWORD;
    byte channel = 0;
  } sta;
} networkSettings;

struct Settings {
  struct {
    uint8_t logLevel = DEFAULT_LOG_LEVEL;

    struct {
      bool enable = DEFAULT_SERIAL_ENABLE;
      unsigned int baudrate = DEFAULT_SERIAL_BAUD;
    } serial;

    struct {
      bool enable = DEFAULT_TELNET_ENABLE;
      unsigned short port = DEFAULT_TELNET_PORT;
    } telnet;

    UnitSystem unitSystem = UnitSystem::METRIC;
    byte statusLedGpio = DEFAULT_STATUS_LED_GPIO;
  } system;

  struct {
    bool auth = false;
    char login[13] = DEFAULT_PORTAL_LOGIN;
    char password[33] = DEFAULT_PORTAL_PASSWORD;
  } portal;

  struct {
    UnitSystem unitSystem = UnitSystem::METRIC;
    byte inGpio = DEFAULT_OT_IN_GPIO;
    byte outGpio = DEFAULT_OT_OUT_GPIO;
    byte rxLedGpio = DEFAULT_OT_RX_LED_GPIO;
    unsigned int memberIdCode = 0;
    uint8_t maxModulation = 100;
    float pressureFactor = 1.0f;
    float dhwFlowRateFactor = 1.0f;
    float minPower = 0.0f;
    float maxPower = 0.0f;
    bool dhwPresent = true;
    bool summerWinterMode = false;
    bool heatingCh2Enabled = true;
    bool heatingCh1ToCh2 = false;
    bool dhwToCh2 = false;
    bool dhwBlocking = false;
    bool modulationSyncWithHeating = false;
    bool getMinMaxTemp = true;
    bool nativeHeatingControl = false;
    bool immergasFix = false;

    struct {
      bool enable = false;
      float factor = 0.1f;
    } filterNumValues;
  } opentherm;

  struct {
    bool enable = false;
    char server[81] = DEFAULT_MQTT_SERVER;
    unsigned short port = DEFAULT_MQTT_PORT;
    char user[33] = DEFAULT_MQTT_USER;
    char password[33] = DEFAULT_MQTT_PASSWORD;
    char prefix[33] = DEFAULT_MQTT_PREFIX;
    unsigned short interval = 5;
    bool homeAssistantDiscovery = true;
  } mqtt;

  struct {
    bool enable = false;
    float target = DEFAULT_HEATING_TARGET_TEMP;
    unsigned short tresholdTime = 120;
    bool useEquitherm = false;
    bool usePid = false;
    bool onNetworkFault = true;
    bool onMqttFault = true;
    bool onIndoorSensorDisconnect = false;
    bool onOutdoorSensorDisconnect = false;
  } emergency;

  struct {
    bool enable = true;
    bool turbo = false;
    float target = DEFAULT_HEATING_TARGET_TEMP;
    float hysteresis = 0.5f;
    byte minTemp = DEFAULT_HEATING_MIN_TEMP;
    byte maxTemp = DEFAULT_HEATING_MAX_TEMP;
  } heating;

  struct {
    bool enable = true;
    float target = DEFAULT_DHW_TARGET_TEMP;
    byte minTemp = DEFAULT_DHW_MIN_TEMP;
    byte maxTemp = DEFAULT_DHW_MAX_TEMP;
  } dhw;

  struct {
    bool enable = false;
    float p_factor = 2;
    float i_factor = 0.0055f;
    float d_factor = 0;
    unsigned short dt = 180;
    byte minTemp = 0;
    byte maxTemp = DEFAULT_HEATING_MAX_TEMP;
  } pid;

  struct {
    bool enable = false;
    float n_factor = 0.7f;
    float k_factor = 3.0f;
    float t_factor = 2.0f;
  } equitherm;

  struct {
    struct {
      SensorType type = SensorType::BOILER;
      byte gpio = DEFAULT_SENSOR_OUTDOOR_GPIO;
      uint8_t bleAddress[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      float offset = 0.0f;
    } outdoor;

    struct {
      SensorType type = SensorType::MANUAL;
      byte gpio = DEFAULT_SENSOR_INDOOR_GPIO;
      uint8_t bleAddress[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      float offset = 0.0f;
    } indoor;
  } sensors;

  struct {
    bool use = false;
    byte gpio = DEFAULT_EXT_PUMP_GPIO;
    unsigned short postCirculationTime = 600;
    unsigned int antiStuckInterval = 2592000;
    unsigned short antiStuckTime = 300;
  } externalPump;

  struct {
    struct {
      bool enable = false;
      byte gpio = GPIO_IS_NOT_CONFIGURED;
      byte invertState = false;
      unsigned short thresholdTime = 60;
    } input;

    struct {
      bool enable = false;
      byte gpio = GPIO_IS_NOT_CONFIGURED;
      byte invertState = false;
      unsigned short thresholdTime = 60;
      bool onFault = true;
      bool onLossConnection = true;
      bool onEnabledHeating = false;
    } output;
  } cascadeControl;

  char validationValue[8] = SETTINGS_VALID_VALUE;
} settings;

struct Variables {
  struct {
    bool otStatus = false;
    bool emergency = false;
    bool heating = false;
    bool dhw = false;
    bool flame = false;
    bool fault = false;
    bool diagnostic = false;
    bool externalPump = false;
    bool mqtt = false;
  } states;

  struct {
    float modulation = 0.0f;
    float pressure = 0.0f;
    float dhwFlowRate = 0.0f;
    float power = 0.0f;
    byte faultCode = 0;
    unsigned short diagnosticCode = 0;
    int8_t rssi = 0;

    struct {
      bool connected = false;
      int8_t rssi = 0;
      float battery = 0.0f;
      float humidity = 0.0f;
    } outdoor;

    struct {
      bool connected = false;
      int8_t rssi = 0;
      float battery = 0.0f;
      float humidity = 0.0f;
    } indoor;
  } sensors;

  struct {
    float indoor = 0.0f;
    float outdoor = 0.0f;
    float heating = 0.0f;
    float heatingReturn = 0.0f;
    float dhw = 0.0f;
    float exhaust = 0.0f;
  } temperatures;

  struct {
    bool input = false;
    bool output = false;
  } cascadeControl;

  struct {
    bool heatingEnabled = false;
    byte heatingMinTemp = DEFAULT_HEATING_MIN_TEMP;
    byte heatingMaxTemp = DEFAULT_HEATING_MAX_TEMP;
    float heatingSetpoint = 0;
    unsigned long extPumpLastEnableTime = 0;
    byte dhwMinTemp = DEFAULT_DHW_MIN_TEMP;
    byte dhwMaxTemp = DEFAULT_DHW_MAX_TEMP;
    byte minModulation = 0;
    byte maxModulation = 0;
    uint8_t maxPower = 0;
    uint8_t slaveMemberId = 0;
    uint8_t slaveFlags = 0;
    uint8_t slaveType = 0;
    uint8_t slaveVersion = 0;
    float slaveOtVersion = 0.0f;
    uint8_t masterMemberId = 0;
    uint8_t masterFlags = 0;
    uint8_t masterType = 0;
    uint8_t masterVersion = 0;
    float masterOtVersion = 0;
  } parameters;

  struct {
    bool restart = false;
    bool resetFault = false;
    bool resetDiagnostic = false;
  } actions;
} vars;