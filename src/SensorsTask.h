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
  float prevFilteredOutdoorTemp = 0;
  
  bool initIndoorSensor = false;
  unsigned long initIndoorSensorTime = 0;
  unsigned long startIndoorConversionTime = 0;
  float filteredIndoorTemp = 0;
  float prevFilteredIndoorTemp = 0;

  #if defined(ARDUINO_ARCH_ESP32)
  #if USE_BLE
  unsigned long outdoorConnectedTime = 0;
  unsigned long indoorConnectedTime = 0;
  #endif

  const char* getTaskName() override {
    return "Sensors";
  }

  BaseType_t getTaskCore() override {
    // https://github.com/h2zero/NimBLE-Arduino/issues/676
    #if USE_BLE && defined(CONFIG_BT_NIMBLE_PINNED_TO_CORE)
    return CONFIG_BT_NIMBLE_PINNED_TO_CORE;
    #else
    return tskNO_AFFINITY;
    #endif
  }

  int getTaskPriority() override {
    return 4;
  }
  #endif

  void loop() {
    #if USE_BLE
    if (!NimBLEDevice::getInitialized() && millis() > 5000) {
      Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Init BLE"));
      BLEDevice::init("");
      NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    }
    #endif

    if (settings.sensors.outdoor.type == SensorType::DS18B20 && GPIO_IS_VALID(settings.sensors.outdoor.gpio)) {
      outdoorDallasSensor();
    }
    #if USE_BLE
    else if (settings.sensors.outdoor.type == SensorType::BLUETOOTH) {
      bool connected = this->bluetoothSensor(
        BLEAddress(settings.sensors.outdoor.bleAddress),
        &vars.sensors.outdoor.rssi,
        &this->filteredOutdoorTemp,
        &vars.sensors.outdoor.humidity,
        &vars.sensors.outdoor.battery
      );

      if (connected) {
        this->outdoorConnectedTime = millis();
        vars.sensors.outdoor.connected = true;

      } else if (millis() - this->outdoorConnectedTime > 60000) {
        vars.sensors.outdoor.connected = false;
      }
    }
    #endif

    if (settings.sensors.indoor.type == SensorType::DS18B20 && GPIO_IS_VALID(settings.sensors.indoor.gpio)) {
      indoorDallasSensor();
    }
    #if USE_BLE
    else if (settings.sensors.indoor.type == SensorType::BLUETOOTH) {
      bool connected = this->bluetoothSensor(
        BLEAddress(settings.sensors.indoor.bleAddress),
        &vars.sensors.indoor.rssi,
        &this->filteredIndoorTemp,
        &vars.sensors.indoor.humidity,
        &vars.sensors.indoor.battery
      );

      if (connected) {
        this->indoorConnectedTime = millis();
        vars.sensors.indoor.connected = true;

      } else if (millis() - this->indoorConnectedTime > 60000) {
        vars.sensors.indoor.connected = false;
      }
    }
    #endif

    // convert
    if (fabs(this->prevFilteredOutdoorTemp - this->filteredOutdoorTemp) >= 0.1f) {
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

      this->prevFilteredOutdoorTemp = this->filteredOutdoorTemp;
    }

    if (fabs(this->prevFilteredIndoorTemp - this->filteredIndoorTemp) > 0.1f) {
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

      this->prevFilteredIndoorTemp = this->filteredIndoorTemp;
    }
  }

#if USE_BLE
  bool bluetoothSensor(const BLEAddress& address, int8_t* const pRssi, float* const pTemperature, float* const pHumidity = nullptr, float* const pBattery = nullptr) {
    if (!NimBLEDevice::getInitialized()) {
      return false;
    }

    NimBLEClient* pClient = nullptr;
    pClient = NimBLEDevice::getClientByPeerAddress(address);

    if (pClient == nullptr) {
      pClient = NimBLEDevice::getDisconnectedClient();
    }

    if (pClient == nullptr) {
      if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
        return false;
      }

      pClient = NimBLEDevice::createClient();
      pClient->setConnectTimeout(5);
    }

    if(pClient->isConnected()) {
      *pRssi = pClient->getRssi();
      return true;
    }

    if (!pClient->connect(address)) {
      Log.swarningln(FPSTR(L_SENSORS_BLE), "Device %s: failed connecting", address.toString().c_str());

      NimBLEDevice::deleteClient(pClient);
      return false;
    }

    Log.sinfoln(FPSTR(L_SENSORS_BLE), "Device %s: connected", address.toString().c_str());
    NimBLERemoteService* pService = nullptr;
    NimBLERemoteCharacteristic* pChar = nullptr;

    // ENV Service (0x181A)
    pService = pClient->getService(NimBLEUUID((uint16_t) 0x181AU));
    if (!pService) {
      Log.straceln(
        FPSTR(L_SENSORS_BLE),
        F("Device %s: failed to find env service (%s)"),
        address.toString().c_str(),
        pService->getUUID().toString().c_str()
      );

    } else {
      Log.straceln(
        FPSTR(L_SENSORS_BLE),
        F("Device %s: found env service (%s)"),
        address.toString().c_str(),
        pService->getUUID().toString().c_str()
      );


      // 0x2A6E - Notify temperature x0.01C (pvvx)
      bool tempNotifyCreated = false;
      if (!tempNotifyCreated) {
        pChar = pService->getCharacteristic(NimBLEUUID((uint16_t) 0x2A6E));

        if (pChar && pChar->canNotify()) {
          Log.straceln(
            FPSTR(L_SENSORS_BLE),
            F("Device %s: found temperature char (%s) in env service"),
            address.toString().c_str(),
            pChar->getUUID().toString().c_str()
          );

          tempNotifyCreated = pChar->subscribe(true, [pTemperature](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
            NimBLEClient* pClient = pChar->getRemoteService()->getClient();

            if (length != 2) {
              Log.swarningln(
                FPSTR(L_SENSORS_BLE),
                F("Device %s: invalid notification data at temperature char (%s)"),
                pClient->getPeerAddress().toString().c_str(),
                pChar->getUUID().toString().c_str()
              );
              return;
            }

            float rawTemp = ((pData[0] | (pData[1] << 8)) * 0.01f);
            Log.straceln(
              FPSTR(L_SENSORS_INDOOR),
              F("Device %s: raw temp %f"),
              pClient->getPeerAddress().toString().c_str(),
              rawTemp
            );

            if (fabs(*pTemperature) < 0.1f) {
              *pTemperature = rawTemp;

            } else {
              *pTemperature += (rawTemp - (*pTemperature)) * EXT_SENSORS_FILTER_K;
            }

            *pTemperature = floor((*pTemperature) * 100) / 100;
          });

          if (tempNotifyCreated) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE),
              F("Device %s: subscribed to temperature char (%s) in env service"),
              address.toString().c_str(),
              pChar->getUUID().toString().c_str()
            );

          } else {
            Log.swarningln(
              FPSTR(L_SENSORS_BLE),
              F("Device %s: failed to subscribe to temperature char (%s) in env service"),
              address.toString().c_str(),
              pChar->getUUID().toString().c_str()
            );
          }
        }
      }


      // 0x2A1F - Notify temperature x0.1C (atc1441/pvvx)
      if (!tempNotifyCreated) {
        pChar = pService->getCharacteristic(NimBLEUUID((uint16_t) 0x2A1F));

        if (pChar && pChar->canNotify()) {
          Log.straceln(
            FPSTR(L_SENSORS_BLE),
            F("Device %s: found temperature char (%s) in env service"),
            address.toString().c_str(),
            pChar->getUUID().toString().c_str()
          );

          tempNotifyCreated = pChar->subscribe(true, [pTemperature](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
            NimBLEClient* pClient = pChar->getRemoteService()->getClient();

            if (length != 2) {
              Log.swarningln(
                FPSTR(L_SENSORS_BLE),
                F("Device %s: invalid notification data at temperature char (%s)"),
                pClient->getPeerAddress().toString().c_str(),
                pChar->getUUID().toString().c_str()
              );
              return;
            }

            float rawTemp = ((pData[0] | (pData[1] << 8)) * 0.1f);
            Log.straceln(
              FPSTR(L_SENSORS_INDOOR),
              F("Device %s: raw temp %f"),
              pClient->getPeerAddress().toString().c_str(),
              rawTemp
            );

            if (fabs(*pTemperature) < 0.1f) {
              *pTemperature = rawTemp;

            } else {
              *pTemperature += (rawTemp - (*pTemperature)) * EXT_SENSORS_FILTER_K;
            }

            *pTemperature = floor((*pTemperature) * 100) / 100;
          });

          if (tempNotifyCreated) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE),
              F("Device %s: subscribed to temperature char (%s) in env service"),
              address.toString().c_str(),
              pChar->getUUID().toString().c_str()
            );

          } else {
            Log.swarningln(
              FPSTR(L_SENSORS_BLE),
              F("Device %s: failed to subscribe to temperature char (%s) in env service"),
              address.toString().c_str(),
              pChar->getUUID().toString().c_str()
            );
          }
        }
      }

      if (!tempNotifyCreated) {
        Log.swarningln(
          FPSTR(L_SENSORS_BLE),
          F("Device %s: not found supported temperature chars in env service"),
          address.toString().c_str()
        );

        pClient->disconnect();
        return false;
      }


      // 0x2A6F - Notify about humidity x0.01% (pvvx)
      if (pHumidity != nullptr) {
        bool humidityNotifyCreated = false;
        if (!humidityNotifyCreated) {
          pChar = pService->getCharacteristic(NimBLEUUID((uint16_t) 0x2A6F));

          if (pChar && pChar->canNotify()) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE),
              F("Device %s: found humidity char (%s) in env service"),
              address.toString().c_str(),
              pChar->getUUID().toString().c_str()
            );

            humidityNotifyCreated = pChar->subscribe(true, [pHumidity](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
              NimBLEClient* pClient = pChar->getRemoteService()->getClient();

              if (length != 2) {
                Log.swarningln(
                  FPSTR(L_SENSORS_BLE),
                  F("Device %s: invalid notification data at humidity char (%s)"),
                  pClient->getPeerAddress().toString().c_str(),
                  pChar->getUUID().toString().c_str()
                );
                return;
              }

              float rawHumidity = ((pData[0] | (pData[1] << 8)) * 0.01f);
              Log.straceln(
                FPSTR(L_SENSORS_INDOOR),
                F("Device %s: raw humidity %f"),
                pClient->getPeerAddress().toString().c_str(),
                rawHumidity
              );

              if (fabs(*pHumidity) < 0.1f) {
                *pHumidity = rawHumidity;

              } else {
                *pHumidity += (rawHumidity - (*pHumidity)) * EXT_SENSORS_FILTER_K;
              }

              *pHumidity = floor((*pHumidity) * 100) / 100;
            });

            if (humidityNotifyCreated) {
              Log.straceln(
                FPSTR(L_SENSORS_BLE),
                F("Device %s: subscribed to humidity char (%s) in env service"),
                address.toString().c_str(),
                pChar->getUUID().toString().c_str()
              );

            } else {
              Log.swarningln(
                FPSTR(L_SENSORS_BLE),
                F("Device %s: failed to subscribe to humidity char (%s) in env service"),
                address.toString().c_str(),
                pChar->getUUID().toString().c_str()
              );
            }
          }
        }

        if (!humidityNotifyCreated) {
          Log.swarningln(
            FPSTR(L_SENSORS_BLE),
            F("Device %s: not found supported humidity chars in env service"),
            address.toString().c_str()
          );
        }
      }
    }


    // Battery Service (0x180F)
    if (pBattery != nullptr) {
      pService = pClient->getService(NimBLEUUID((uint16_t) 0x180F));
      if (!pService) {
        Log.straceln(
          FPSTR(L_SENSORS_BLE),
          F("Device %s: failed to find battery service (%s)"),
          address.toString().c_str(),
          pService->getUUID().toString().c_str()
        );

      } else {
        Log.straceln(
          FPSTR(L_SENSORS_BLE),
          F("Device %s: found battery service (%s)"),
          address.toString().c_str(),
          pService->getUUID().toString().c_str()
        );

        // 0x2A19 - Notify the battery charge level 0..99% (pvvx)
        bool batteryNotifyCreated = false;
        if (!batteryNotifyCreated) {
          pChar = pService->getCharacteristic(NimBLEUUID((uint16_t) 0x2A19));

          if (pChar && pChar->canNotify()) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE),
              F("Device %s: found battery char (%s) in battery service"),
              address.toString().c_str(),
              pChar->getUUID().toString().c_str()
            );

            batteryNotifyCreated = pChar->subscribe(true, [pBattery](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
              NimBLEClient* pClient = pChar->getRemoteService()->getClient();

              if (length != 1) {
                Log.swarningln(
                  FPSTR(L_SENSORS_BLE),
                  F("Device %s: invalid notification data at battery char (%s)"),
                  pClient->getPeerAddress().toString().c_str(),
                  pChar->getUUID().toString().c_str()
                );
                return;
              }

              uint8_t rawBattery = pData[0];
              Log.straceln(
                FPSTR(L_SENSORS_INDOOR),
                F("Device %s: raw battery %hhu"),
                pClient->getPeerAddress().toString().c_str(),
                rawBattery
              );

              if (fabs(*pBattery) < 0.1f) {
                *pBattery = rawBattery;

              } else {
                *pBattery += (rawBattery - (*pBattery)) * EXT_SENSORS_FILTER_K;
              }

              *pBattery = floor((*pBattery) * 100) / 100;
            });

            if (batteryNotifyCreated) {
              Log.straceln(
                FPSTR(L_SENSORS_BLE),
                F("Device %s: subscribed to battery char (%s) in battery service"),
                address.toString().c_str(),
                pChar->getUUID().toString().c_str()
              );

            } else {
              Log.swarningln(
                FPSTR(L_SENSORS_BLE),
                F("Device %s: failed to subscribe to battery char (%s) in battery service"),
                address.toString().c_str(),
                pChar->getUUID().toString().c_str()
              );
            }
          }
        }

        if (!batteryNotifyCreated) {
          Log.swarningln(
            FPSTR(L_SENSORS_BLE),
            F("Device %s: not found supported battery chars in battery service"),
            address.toString().c_str()
          );
        }
      }
    }

    return true;
  }
#endif

  void outdoorDallasSensor() {
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
        if (vars.sensors.outdoor.connected) {
          vars.sensors.outdoor.connected = false;
        }

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

      if (!vars.sensors.outdoor.connected) {
        vars.sensors.outdoor.connected = true;
      }

      if (fabs(this->filteredOutdoorTemp) < 0.1f) {
        this->filteredOutdoorTemp = rawTemp;

      } else {
        this->filteredOutdoorTemp += (rawTemp - this->filteredOutdoorTemp) * EXT_SENSORS_FILTER_K;
      }

      this->filteredOutdoorTemp = floor(this->filteredOutdoorTemp * 100) / 100;
      this->outdoorSensor->requestTemperatures();
      this->startOutdoorConversionTime = millis();
    }
  }

  void indoorDallasSensor() {
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
        if (vars.sensors.indoor.connected) {
          vars.sensors.indoor.connected = false;
        }

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

      if (!vars.sensors.indoor.connected) {
        vars.sensors.indoor.connected = true;
      }

      if (fabs(this->filteredIndoorTemp) < 0.1f) {
        this->filteredIndoorTemp = rawTemp;

      } else {
        this->filteredIndoorTemp += (rawTemp - this->filteredIndoorTemp) * EXT_SENSORS_FILTER_K;
      }

      this->filteredIndoorTemp = floor(this->filteredIndoorTemp * 100) / 100;
      this->indoorSensor->requestTemperatures();
      this->startIndoorConversionTime = millis();
    }
  }
};