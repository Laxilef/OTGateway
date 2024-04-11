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
    bool debug = DEBUG_BY_DEFAULT;

    struct {
      bool enable = USE_SERIAL;
      unsigned int baudrate = 115200;
    } serial;

    struct {
      bool enable = USE_TELNET;
      unsigned short port = 23;
    } telnet;

    UnitSystem unitSystem = UnitSystem::METRIC;
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
    unsigned int memberIdCode = 0;
    bool dhwPresent = true;
    bool summerWinterMode = false;
    bool heatingCh2Enabled = true;
    bool heatingCh1ToCh2 = false;
    bool dhwToCh2 = false;
    bool dhwBlocking = false;
    bool modulationSyncWithHeating = false;
    bool getMinMaxTemp = true;
  } opentherm;

  struct {
    bool enable = false;
    char server[81] = DEFAULT_MQTT_SERVER;
    unsigned short port = DEFAULT_MQTT_PORT;
    char user[33] = DEFAULT_MQTT_USER;
    char password[33] = DEFAULT_MQTT_PASSWORD;
    char prefix[33] = DEFAULT_MQTT_PREFIX;
    unsigned short interval = 5;
  } mqtt;

  struct {
    bool enable = true;
    float target = 40.0f;
    bool useEquitherm = false;
    bool usePid = false;
    bool onNetworkFault = true;
    bool onMqttFault = true;
  } emergency;

  struct {
    bool enable = true;
    bool turbo = false;
    float target = 40.0f;
    float hysteresis = 0.5f;
    byte minTemp = DEFAULT_HEATING_MIN_TEMP;
    byte maxTemp = DEFAULT_HEATING_MAX_TEMP;
    byte maxModulation = 100;
  } heating;

  struct {
    bool enable = true;
    byte target = 40;
    byte minTemp = DEFAULT_DHW_MIN_TEMP;
    byte maxTemp = DEFAULT_DHW_MAX_TEMP;
  } dhw;

  struct {
    bool enable = false;
    float p_factor = 50;
    float i_factor = 0.006f;
    float d_factor = 10000;
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
      float offset = 0.0f;
    } outdoor;

    struct {
      SensorType type = SensorType::MANUAL;
      byte gpio = DEFAULT_SENSOR_INDOOR_GPIO;
      uint8_t bleAddresss[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
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

  char validationValue[8] = SETTINGS_VALID_VALUE;
} settings;

struct Variables {
  struct {
    bool enable = false;
    byte regulator = 0;
  } tuning;

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
    byte faultCode = 0;
    int8_t rssi = 0;
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
    bool heatingEnabled = false;
    byte heatingMinTemp = DEFAULT_HEATING_MIN_TEMP;
    byte heatingMaxTemp = DEFAULT_HEATING_MAX_TEMP;
    byte heatingSetpoint = 0;
    unsigned long extPumpLastEnableTime = 0;
    byte dhwMinTemp = DEFAULT_DHW_MIN_TEMP;
    byte dhwMaxTemp = DEFAULT_DHW_MAX_TEMP;
    byte maxModulation = 0;
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