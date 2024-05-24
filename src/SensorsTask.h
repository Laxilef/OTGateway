#include <OneWire.h>
#include <DallasTemperature.h>

#if USE_BLE
  #include <NimBLEDevice.h>
#endif

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {
    this->oneWireOutdoorSensor = new OneWire();
    this->outdoorSensor = new DallasTemperature(this->oneWireOutdoorSensor);
    this->outdoorSensor->setWaitForConversion(false);

    this->oneWireIndoorSensor = new OneWire();
    this->indoorSensor = new DallasTemperature(this->oneWireIndoorSensor);
    this->indoorSensor->setWaitForConversion(false);
  }

  ~SensorsTask() {
    delete this->outdoorSensor;
    delete this->oneWireOutdoorSensor;
    delete this->indoorSensor;
    delete this->oneWireIndoorSensor;
  }

protected:
  OneWire* oneWireOutdoorSensor = nullptr;
  OneWire* oneWireIndoorSensor = nullptr;

  DallasTemperature* outdoorSensor = nullptr;
  DallasTemperature* indoorSensor = nullptr;

  bool initOutdoorSensor = false;
  unsigned long initOutdoorSensorTime = 0;
  unsigned long startOutdoorConversionTime = 0;
  float filteredOutdoorTemp = 0;
  bool emptyOutdoorTemp = true;
  
  bool initIndoorSensor = false;
  unsigned long initIndoorSensorTime = 0;
  unsigned long startIndoorConversionTime = 0;
  float filteredIndoorTemp = 0;
  bool emptyIndoorTemp = true;

#if USE_BLE
  BLEClient* pBleClient = nullptr;
  bool initBleSensor = false;
  bool initBleNotify = false;
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
    bool indoorTempUpdated = false;
    bool outdoorTempUpdated = false;

    if (settings.sensors.outdoor.type == SensorType::DS18B20 && GPIO_IS_VALID(settings.sensors.outdoor.gpio)) {
      outdoorTemperatureSensor();
      outdoorTempUpdated = true;
    }

    if (settings.sensors.indoor.type == SensorType::DS18B20 && GPIO_IS_VALID(settings.sensors.indoor.gpio)) {
      indoorTemperatureSensor();
      indoorTempUpdated = true;
    }
    #if USE_BLE
    else if (settings.sensors.indoor.type == SensorType::BLUETOOTH) {
      indoorTemperatureBluetoothSensor();
      indoorTempUpdated = true;
    }
    #endif

    if (outdoorTempUpdated) {
      float newTemp = settings.sensors.outdoor.offset;
      if (settings.system.unitSystem == UnitSystem::METRIC) {
        newTemp += this->filteredOutdoorTemp;

      } else if (settings.system.unitSystem == UnitSystem::IMPERIAL) {
        newTemp += c2f(this->filteredOutdoorTemp);
      }

      if (fabs(vars.temperatures.outdoor - newTemp) > 0.099f) {
        vars.temperatures.outdoor = newTemp;
        Log.sinfoln(FPSTR(L_SENSORS_OUTDOOR), F("New temp: %f"), vars.temperatures.outdoor);
      }
    }

    if (indoorTempUpdated) {
      float newTemp = settings.sensors.indoor.offset;
      if (settings.system.unitSystem == UnitSystem::METRIC) {
        newTemp += this->filteredIndoorTemp;

      } else if (settings.system.unitSystem == UnitSystem::IMPERIAL) {
        newTemp += c2f(this->filteredIndoorTemp);
      }

      if (fabs(vars.temperatures.indoor - newTemp) > 0.099f) {
        vars.temperatures.indoor = newTemp;
        Log.sinfoln(FPSTR(L_SENSORS_INDOOR), F("New temp: %f"), vars.temperatures.indoor);
      }
    }
  }

#if USE_BLE
  void indoorTemperatureBluetoothSensor() {
    static bool initBleNotify = false;
    if (!initBleSensor && millis() > 5000) {
      Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Init BLE"));
      BLEDevice::init("");

      pBleClient = BLEDevice::createClient();
      pBleClient->setConnectTimeout(5); 

      initBleSensor = true;
    }

    if (!initBleSensor || pBleClient->isConnected()) {
      return;
    }
    
    // Reset init notify flag
    this->initBleNotify = false;

    // Connect to the remote BLE Server.
    BLEAddress bleServerAddress(settings.sensors.indoor.bleAddresss);
    if (!pBleClient->connect(bleServerAddress)) {
      Log.swarningln(FPSTR(L_SENSORS_BLE), "Failed connecting to device at %s", bleServerAddress.toString().c_str());
      return;
    }

    Log.sinfoln(FPSTR(L_SENSORS_BLE), "Connected to device at %s", bleServerAddress.toString().c_str());

    NimBLEUUID serviceUUID((uint16_t) 0x181AU);
    BLERemoteService* pRemoteService = pBleClient->getService(serviceUUID);
    if (!pRemoteService) {
      Log.straceln(FPSTR(L_SENSORS_BLE), F("Failed to find service UUID: %s"), serviceUUID.toString().c_str());
      return;
    }

    Log.straceln(FPSTR(L_SENSORS_BLE), F("Found service UUID: %s"), serviceUUID.toString().c_str());

    // 0x2A6E - Notify temperature x0.01C (pvvx)
    if (!this->initBleNotify) {
      NimBLEUUID charUUID((uint16_t) 0x2A6E);
      BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
      if (pRemoteCharacteristic && pRemoteCharacteristic->canNotify()) {
        Log.straceln(FPSTR(L_SENSORS_BLE), F("Found characteristic UUID: %s"), charUUID.toString().c_str());

        this->initBleNotify = pRemoteCharacteristic->subscribe(true, [this](NimBLERemoteCharacteristic*, uint8_t* pData, size_t length, bool isNotify) {
          if (length != 2) {
            Log.swarningln(FPSTR(L_SENSORS_BLE), F("Invalid notification data"));
            return;
          }

          float rawTemp = ((pData[0] | (pData[1] << 8)) * 0.01f);
          Log.straceln(FPSTR(L_SENSORS_INDOOR), F("Raw temp: %f"), rawTemp);

          if (this->emptyIndoorTemp) {
            this->filteredIndoorTemp = rawTemp;
            this->emptyIndoorTemp = false;

          } else {
            this->filteredIndoorTemp += (rawTemp - this->filteredIndoorTemp) * EXT_SENSORS_FILTER_K;
          }

          this->filteredIndoorTemp = floor(this->filteredIndoorTemp * 100) / 100;
        });

        if (this->initBleNotify) {
          Log.straceln(FPSTR(L_SENSORS_BLE), F("Subscribed to characteristic UUID: %s"), charUUID.toString().c_str());

        } else {
          Log.swarningln(FPSTR(L_SENSORS_BLE), F("Failed to subscribe to characteristic UUID: %s"), charUUID.toString().c_str());
        }
      }
    }

    // 0x2A1F - Notify temperature x0.1C (atc1441/pvvx)
    if (!this->initBleNotify) {
      NimBLEUUID charUUID((uint16_t) 0x2A1F);
      BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
      if (pRemoteCharacteristic && pRemoteCharacteristic->canNotify()) {
        Log.straceln(FPSTR(L_SENSORS_BLE), F("Found characteristic UUID: %s"), charUUID.toString().c_str());

        this->initBleNotify = pRemoteCharacteristic->subscribe(true, [this](NimBLERemoteCharacteristic*, uint8_t* pData, size_t length, bool isNotify) {
          if (length != 2) {
            Log.swarningln(FPSTR(L_SENSORS_BLE), F("Invalid notification data"));
            return;
          }

          float rawTemp = ((pData[0] | (pData[1] << 8)) * 0.1);
          Log.straceln(FPSTR(L_SENSORS_INDOOR), F("Raw temp: %f"), rawTemp);

          if (this->emptyIndoorTemp) {
            this->filteredIndoorTemp = rawTemp;
            this->emptyIndoorTemp = false;

          } else {
            this->filteredIndoorTemp += (rawTemp - this->filteredIndoorTemp) * EXT_SENSORS_FILTER_K;
          }

          this->filteredIndoorTemp = floor(this->filteredIndoorTemp * 100) / 100;
        });

        if (this->initBleNotify) {
          Log.straceln(FPSTR(L_SENSORS_BLE), F("Subscribed to characteristic UUID: %s"), charUUID.toString().c_str());
          
        } else {
          Log.swarningln(FPSTR(L_SENSORS_BLE), F("Failed to subscribe to characteristic UUID: %s"), charUUID.toString().c_str());
        }
      }
    }

    if (!this->initBleNotify) {
      Log.swarningln(FPSTR(L_SENSORS_BLE), F("Not found supported characteristics"));
      pBleClient->disconnect();
    }
  }
#endif

  void outdoorTemperatureSensor() {
    if (!this->initOutdoorSensor) {
      if (this->initOutdoorSensorTime && millis() - this->initOutdoorSensorTime < EXT_SENSORS_INTERVAL * 10) {
        return;
      }

      Log.sinfoln(FPSTR(L_SENSORS_OUTDOOR), F("Starting on GPIO %hhu..."), settings.sensors.outdoor.gpio);

      this->oneWireOutdoorSensor->begin(settings.sensors.outdoor.gpio);
      this->oneWireOutdoorSensor->reset();
      this->outdoorSensor->begin();
      this->initOutdoorSensorTime = millis();

      Log.straceln(
        FPSTR(L_SENSORS_OUTDOOR),
        F("Devices on bus: %hhu, DS18* devices: %hhu"),
        this->outdoorSensor->getDeviceCount(),
        this->outdoorSensor->getDS18Count()
      );

      if (this->outdoorSensor->getDeviceCount() > 0) {
        this->initOutdoorSensor = true;
        this->outdoorSensor->setResolution(12);
        this->outdoorSensor->requestTemperatures();
        this->startOutdoorConversionTime = millis();

        Log.sinfoln(FPSTR(L_SENSORS_OUTDOOR), F("Started"));

      } else {
        return;
      }
    }

    unsigned long estimateConversionTime = millis() - this->startOutdoorConversionTime;
    if (estimateConversionTime < this->outdoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = this->outdoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      this->initOutdoorSensor = false;

      Log.serrorln(FPSTR(L_SENSORS_OUTDOOR), F("Could not read temperature data (no response)"));
    }

    if (!completed) {
      return;
    }

    float rawTemp = this->outdoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      this->initOutdoorSensor = false;

      Log.serrorln(FPSTR(L_SENSORS_OUTDOOR), F("Could not read temperature data (not connected)"));

    } else {
      Log.straceln(FPSTR(L_SENSORS_OUTDOOR), F("Raw temp: %f"), rawTemp);

      if (this->emptyOutdoorTemp) {
        this->filteredOutdoorTemp = rawTemp;
        this->emptyOutdoorTemp = false;

      } else {
        this->filteredOutdoorTemp += (rawTemp - this->filteredOutdoorTemp) * EXT_SENSORS_FILTER_K;
      }

      this->filteredOutdoorTemp = floor(this->filteredOutdoorTemp * 100) / 100;
      this->outdoorSensor->requestTemperatures();
      this->startOutdoorConversionTime = millis();
    }
  }

  void indoorTemperatureSensor() {
    if (!this->initIndoorSensor) {
      if (this->initIndoorSensorTime && millis() - this->initIndoorSensorTime < EXT_SENSORS_INTERVAL * 10) {
        return;
      }
      
      Log.sinfoln(FPSTR(L_SENSORS_INDOOR), F("Starting on GPIO %hhu..."), settings.sensors.indoor.gpio);

      this->oneWireIndoorSensor->begin(settings.sensors.indoor.gpio);
      this->oneWireIndoorSensor->reset();
      this->indoorSensor->begin();
      this->initIndoorSensorTime = millis();

      Log.straceln(
        FPSTR(L_SENSORS_INDOOR),
        F("Devices on bus: %hhu, DS18* devices: %hhu"),
        this->indoorSensor->getDeviceCount(),
        this->indoorSensor->getDS18Count()
      );

      if (this->indoorSensor->getDeviceCount() > 0) {
        this->initIndoorSensor = true;
        this->indoorSensor->setResolution(12);
        this->indoorSensor->requestTemperatures();
        this->startIndoorConversionTime = millis();

        Log.sinfoln(FPSTR(L_SENSORS_INDOOR), F("Started"));

      } else {
        return;
      }
    }

    unsigned long estimateConversionTime = millis() - this->startIndoorConversionTime;
    if (estimateConversionTime < this->indoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = this->indoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      this->initIndoorSensor = false;

      Log.serrorln(FPSTR(L_SENSORS_INDOOR), F("Could not read temperature data (no response)"));
    }

    if (!completed) {
      return;
    }

    float rawTemp = this->indoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      this->initIndoorSensor = false;

      Log.serrorln(FPSTR(L_SENSORS_INDOOR), F("Could not read temperature data (not connected)"));

    } else {
      Log.straceln(FPSTR(L_SENSORS_INDOOR), F("Raw temp: %f"), rawTemp);

      if (this->emptyIndoorTemp) {
        this->filteredIndoorTemp = rawTemp;
        this->emptyIndoorTemp = false;

      } else {
        this->filteredIndoorTemp += (rawTemp - this->filteredIndoorTemp) * EXT_SENSORS_FILTER_K;
      }

      this->filteredIndoorTemp = floor(this->filteredIndoorTemp * 100) / 100;
      this->indoorSensor->requestTemperatures();
      this->startIndoorConversionTime = millis();
    }
  }
};