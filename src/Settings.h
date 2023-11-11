struct Settings {
  bool debug = false;
  char hostname[80] = "opentherm";

  struct {
    byte inPin = OT_IN_PIN_DEFAULT;
    byte outPin = OT_OUT_PIN_DEFAULT;
    unsigned int memberIdCode = 0;
    bool dhwPresent = true;
  } opentherm;

  struct {
    char server[80];
    int port = 1883;
    char user[32];
    char password[32];
    char prefix[80] = "opentherm";
    unsigned int interval = 5000;
  } mqtt;

  struct {
    bool enable = true;
    float target = 40.0f;
    bool useEquitherm = false;
  } emergency;

  struct {
    bool enable = true;
    bool turbo = false;
    float target = 40.0f;
    float hysteresis = 0.5f;
    byte minTemp = 20.0f;
    byte maxTemp = 90.0f;
  } heating;

  struct {
    bool enable = true;
    byte target = 40;
    byte minTemp = 30.0f;
    byte maxTemp = 60.0f;
  } dhw;

  struct {
    bool enable = false;
    float p_factor = 3;
    float i_factor = 0.2f;
    float d_factor = 0;
    byte minTemp = 0.0f;
    byte maxTemp = 90.0f;
  } pid;

  struct {
    bool enable = false;
    float n_factor = 0.7f;
    float k_factor = 3.0f;
    float t_factor = 2.0f;
  } equitherm;

  struct {
    struct {
      // 0 - boiler, 1 - manual, 2 - ds18b20
      byte type = 0;
      byte pin = SENSOR_OUTDOOR_PIN_DEFAULT;
      float offset = 0.0f;
    } outdoor;

    struct {
      // 1 - manual, 2 - ds18b20
      byte type = 1;
      byte pin = SENSOR_INDOOR_PIN_DEFAULT;
      float offset = 0.0f;
    } indoor;
  } sensors;

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
    byte faultCode = 0;
    int8_t rssi = 0;
  } states;

  struct {
    float modulation = 0.0f;
    float pressure = 0.0f;
  } sensors;

  struct {
    float indoor = 0.0f;
    float outdoor = 0.0f;
    float heating = 0.0f;
    float dhw = 0.0f;
  } temperatures;

  struct {
    unsigned long restartSignalTime = 0;
    unsigned int restartAfterTime = 0;
    bool heatingEnabled = false;
    byte heatingMinTemp = 20;
    byte heatingMaxTemp = 90;
    byte heatingSetpoint = 0.0f;
    byte dhwMinTemp = 30;
    byte dhwMaxTemp = 60;
    uint8_t slaveMemberIdCode;
    uint8_t slaveType;
    uint8_t slaveVersion;
    uint8_t masterType;
    uint8_t masterVersion;
  } parameters;
} vars;