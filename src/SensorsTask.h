#include <OneWire.h>
#include <DallasTemperature.h>

#if USE_BLE
  #include <NimBLEDevice.h>

  // BLE services and characterstics that we are interested in
  const uint16_t bleUuidServiceBattery = 0x180F;
  const uint16_t bleUuidServiceEnvironment = 0x181AU;
  const uint16_t bleUuidCharacteristicBatteryLevel = 0x2A19;
  const uint16_t bleUuidCharacteristicTemperature = 0x2A6E;
  const uint16_t bleUuidCharacteristicHumidity = 0x2A6F;
#endif

const char S_SENSORS_OUTDOOR[]  PROGMEM = "SENSORS.OUTDOOR";
const char S_SENSORS_INDOOR[]   PROGMEM = "SENSORS.INDOOR";
const char S_SENSORS_BLE[]      PROGMEM = "SENSORS.BLE";

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {
    this->oneWireOutdoorSensor = new OneWire();
    this->outdoorSensor = new DallasTemperature(this->oneWireOutdoorSensor);

    this->oneWireIndoorSensor = new OneWire();
    this->indoorSensor = new DallasTemperature(this->oneWireIndoorSensor);
  }

  ~SensorsTask() {
    if (this->outdoorSensor != nullptr) {
      delete this->outdoorSensor;
    }

    if (this->oneWireOutdoorSensor != nullptr) {
      delete this->oneWireOutdoorSensor;
    }

    if (this->indoorSensor != nullptr) {
      delete this->indoorSensor;
    }

    if (this->oneWireIndoorSensor != nullptr) {
      delete this->oneWireIndoorSensor;
    }
  }

protected:
  OneWire* oneWireOutdoorSensor = nullptr;
  OneWire* oneWireIndoorSensor = nullptr;

  DallasTemperature* outdoorSensor = nullptr;
  DallasTemperature* indoorSensor = nullptr;

  bool initOutdoorSensor = false;
  unsigned long startOutdoorConversionTime = 0;
  float filteredOutdoorTemp = 0;
  bool emptyOutdoorTemp = true;
  
  bool initIndoorSensor = false;
  unsigned long startIndoorConversionTime = 0;
  float filteredIndoorTemp = 0;
  bool emptyIndoorTemp = true;

#if USE_BLE
  BLEClient* pBleClient = nullptr;
  BLERemoteService* pBleServiceBattery = nullptr;
  BLERemoteService* pBleServiceEnvironment = nullptr;
  bool initBleSensor = false;
#endif

  const char* getTaskName() {
    return "Sensors";
  }

  /*int getTaskCore() {
    return 1;
  }*/

  int getTaskPriority() {
    return 4;
  }

  void loop() {
    if (settings.sensors.outdoor.type == 2 && settings.sensors.outdoor.pin) {
      outdoorTemperatureSensor();
    }

    if (settings.sensors.indoor.type == 2 && settings.sensors.indoor.pin) {
      indoorTemperatureSensor();
    }

#if USE_BLE
    if (settings.sensors.indoor.type == 3 && strlen(settings.sensors.indoor.bleAddresss)) {
      bluetoothSensor();
    }
#endif
  }

#if USE_BLE
  void bluetoothSensor() {
    if (!initBleSensor && millis() > 5000) {
      Log.sinfoln(FPSTR(S_SENSORS_BLE), "Init BLE. Free heap %u bytes", ESP.getFreeHeap());
      BLEDevice::init("");

      pBleClient = BLEDevice::createClient();   

      // Connect to the remote BLE Server.
      BLEAddress bleServerAddress(std::string(settings.sensors.indoor.bleAddresss));
      if (pBleClient->connect(bleServerAddress)) {
        Log.sinfoln(FPSTR(S_SENSORS_BLE), "Connected to BLE device at %s", bleServerAddress.toString().c_str());
        // Obtain a reference to the services we are interested in
        pBleServiceBattery = pBleClient->getService(BLEUUID(bleUuidServiceBattery));
        if (pBleServiceBattery == nullptr) {
          Log.sinfoln(FPSTR(S_SENSORS_BLE), "Failed to find battery service");
        }
        pBleServiceEnvironment = pBleClient->getService(BLEUUID(bleUuidServiceEnvironment));
        if (pBleServiceEnvironment == nullptr) {
          Log.sinfoln(FPSTR(S_SENSORS_BLE), "Failed to find environmental service");
        }

      } else {
        Log.swarningln(FPSTR(S_SENSORS_BLE), "Error connecting to BLE device at %s", bleServerAddress.toString().c_str());
      }
      
      initBleSensor = true;
    }

    if (pBleClient && pBleClient->isConnected()) {
      Log.straceln(FPSTR(S_SENSORS_BLE), "Connected. Free heap %u bytes", ESP.getFreeHeap());
      if (pBleServiceBattery) {
        uint8_t batteryLevel = *reinterpret_cast<const uint8_t *>(pBleServiceBattery->getValue(bleUuidCharacteristicBatteryLevel).data());
        Log.straceln(FPSTR(S_SENSORS_BLE), "Battery: %d", batteryLevel);
      }

      if (pBleServiceEnvironment) {
        float temperature = *reinterpret_cast<const int16_t *>(pBleServiceEnvironment->getValue(bleUuidCharacteristicTemperature).data()) / 100.0f;
        Log.straceln(FPSTR(S_SENSORS_BLE), "Temperature: %.2f", temperature);
        float humidity = *reinterpret_cast<const int16_t *>(pBleServiceEnvironment->getValue(bleUuidCharacteristicHumidity).data()) / 100.0f;
        Log.straceln(FPSTR(S_SENSORS_BLE), "Humidity: %.2f", humidity);

        vars.temperatures.indoor = temperature + settings.sensors.indoor.offset;
      }
    } else {
      Log.straceln(FPSTR(S_SENSORS_BLE), "Not connected");
    }
  }
#endif

  void outdoorTemperatureSensor() {
    if (!this->initOutdoorSensor) {
      this->oneWireOutdoorSensor->begin(settings.sensors.outdoor.pin);
      this->outdoorSensor->begin();
      this->outdoorSensor->setResolution(12);
      this->outdoorSensor->setWaitForConversion(false);
      this->outdoorSensor->requestTemperatures();
      this->startOutdoorConversionTime = millis();

      this->initOutdoorSensor = true;
    }

    unsigned long estimateConversionTime = millis() - this->startOutdoorConversionTime;
    if (estimateConversionTime < this->outdoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = this->outdoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      // fail, retry
      this->outdoorSensor->requestTemperatures();
      this->startOutdoorConversionTime = millis();

      Log.serrorln(FPSTR(S_SENSORS_OUTDOOR), F("Could not read temperature data (no response)"));
    }

    if (!completed) {
      return;
    }

    float rawTemp = this->outdoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      Log.serrorln(FPSTR(S_SENSORS_OUTDOOR), F("Could not read temperature data (not connected)"));

    } else {
      Log.straceln(FPSTR(S_SENSORS_OUTDOOR), F("Raw temp: %f"), rawTemp);

      if (this->emptyOutdoorTemp) {
        this->filteredOutdoorTemp = rawTemp;
        this->emptyOutdoorTemp = false;

      } else {
        this->filteredOutdoorTemp += (rawTemp - this->filteredOutdoorTemp) * EXT_SENSORS_FILTER_K;
      }

      this->filteredOutdoorTemp = floor(this->filteredOutdoorTemp * 100) / 100;

      if (fabs(vars.temperatures.outdoor - this->filteredOutdoorTemp) > 0.099) {
        vars.temperatures.outdoor = this->filteredOutdoorTemp + settings.sensors.outdoor.offset;
        Log.sinfoln(FPSTR(S_SENSORS_OUTDOOR), F("New temp: %f"), this->filteredOutdoorTemp);
      }
    }

    this->outdoorSensor->requestTemperatures();
    this->startOutdoorConversionTime = millis();
  }

  void indoorTemperatureSensor() {
    if (!this->initIndoorSensor) {
      this->oneWireIndoorSensor->begin(settings.sensors.indoor.pin);
      this->indoorSensor->begin();
      this->indoorSensor->setResolution(12);
      this->indoorSensor->setWaitForConversion(false);
      this->indoorSensor->requestTemperatures();
      this->startIndoorConversionTime = millis();

      this->initIndoorSensor = true;
    }

    unsigned long estimateConversionTime = millis() - this->startIndoorConversionTime;
    if (estimateConversionTime < this->indoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = this->indoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      // fail, retry
      this->indoorSensor->requestTemperatures();
      this->startIndoorConversionTime = millis();
      
      Log.serrorln(FPSTR(S_SENSORS_INDOOR), F("Could not read temperature data (no response)"));
    }

    if (!completed) {
      return;
    }

    float rawTemp = this->indoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      Log.serrorln(FPSTR(S_SENSORS_INDOOR), F("Could not read temperature data (not connected)"));

    } else {
      Log.straceln(FPSTR(S_SENSORS_INDOOR), F("Raw temp: %f"), rawTemp);

      if (this->emptyIndoorTemp) {
        this->filteredIndoorTemp = rawTemp;
        this->emptyIndoorTemp = false;

      } else {
        this->filteredIndoorTemp += (rawTemp - this->filteredIndoorTemp) * EXT_SENSORS_FILTER_K;
      }

      this->filteredIndoorTemp = floor(this->filteredIndoorTemp * 100) / 100;

      if (fabs(vars.temperatures.indoor - this->filteredIndoorTemp) > 0.099) {
        vars.temperatures.indoor = this->filteredIndoorTemp + settings.sensors.indoor.offset;
        Log.sinfoln(FPSTR(S_SENSORS_INDOOR), F("New temp: %f"), this->filteredIndoorTemp);
      }
    }

    this->indoorSensor->requestTemperatures();
    this->startIndoorConversionTime = millis();
  }
};