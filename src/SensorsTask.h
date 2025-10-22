#include <unordered_map>
#include <OneWire.h>
#include <DallasTemperature.h>

#if USE_BLE
  #include <NimBLEDevice.h>
#endif

extern FileData fsSensorsSettings;

#if USE_BLE
class BluetoothScanCallbacks : public NimBLEScanCallbacks {
public:
  void onDiscovered(const NimBLEAdvertisedDevice* device) override {
    auto& deviceAddress = device->getAddress();

    bool found = false;
    uint8_t sensorId;
    for (sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      auto& sSensor = Sensors::settings[sensorId];
      if (!sSensor.enabled || sSensor.type != Sensors::Type::BLUETOOTH || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;
      }

      const auto sensorAddress = NimBLEAddress(sSensor.address, deviceAddress.getType());
      if (sensorAddress.isNull() || sensorAddress != deviceAddress) {
        continue;
      }

      found = true;
      break;
    }

    if (!found) {
      return;
    }

    auto& sSensor = Sensors::settings[sensorId];
    auto& rSensor = Sensors::results[sensorId];
    auto deviceName = device->getName();
    auto deviceRssi = device->getRSSI();

    Log.straceln(
      FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': discovered device %s, name: %s, RSSI: %hhd"),
      sensorId, sSensor.name,
      deviceAddress.toString().c_str(), deviceName.c_str(), deviceRssi
    );

    if (!device->haveServiceData()) {
      Log.straceln(
        FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': not found service data"),
        sensorId, sSensor.name
      );
      return;
    }

    auto serviceDataCount = device->getServiceDataCount();
    Log.straceln(
      FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found %hhu service data"),
      sensorId, sSensor.name, serviceDataCount
    );

    NimBLEUUID serviceUuid((uint16_t) 0x181A);
    auto serviceData = device->getServiceData(serviceUuid);
    if (!serviceData.size()) {
      Log.straceln(
        FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': NOT found %s env service data"),
        sensorId, sSensor.name, serviceUuid.toString().c_str()
      );
      return;
    }
  
    Log.straceln(
      FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found %s env service data"),
      sensorId, sSensor.name, serviceUuid.toString().c_str()
    );

    float temperature, humidity;
    uint16_t batteryMv;
    uint8_t batteryLevel;

    if (serviceData.size() == 13) {
      // atc1441 format

      // Temperature (2 bytes, big-endian)
      temperature = (
        (static_cast<uint8_t>(serviceData[6]) << 8) | static_cast<uint8_t>(serviceData[7])
      ) * 0.1f;

      // Humidity (1 byte)
      humidity = static_cast<uint8_t>(serviceData[8]);

      // Battery mV (2 bytes, big-endian)
      batteryMv = (static_cast<uint8_t>(serviceData[10]) << 8) | static_cast<uint8_t>(serviceData[11]);

      // Battery level (1 byte)
      batteryLevel = static_cast<uint8_t>(serviceData[9]);

    } else if (serviceData.size() == 15) {
      // custom pvvx format

      // Temperature (2 bytes, little-endian)
      temperature = (
        (static_cast<uint8_t>(serviceData[7]) << 8) | static_cast<uint8_t>(serviceData[6])
      ) * 0.01f;

      // Humidity (2 bytes, little-endian)
      humidity = (
        (static_cast<uint8_t>(serviceData[9]) << 8) | static_cast<uint8_t>(serviceData[8])
      ) * 0.01f;

      // Battery mV (2 bytes, little-endian)
      batteryMv = (static_cast<uint8_t>(serviceData[11]) << 8) | static_cast<uint8_t>(serviceData[10]);

      // Battery level (1 byte)
      batteryLevel = static_cast<uint8_t>(serviceData[12]);

    } else {
      // unknown format
      Log.straceln(
        FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': unknown data format (size: %i)"),
        sensorId, sSensor.name, serviceData.size()
      );
      return;
    }

    Log.straceln(
      FPSTR(L_SENSORS_BLE),
      F("Sensor #%hhu '%s', received temp: %.2f; humidity: %.2f, battery voltage: %hu, battery level: %hhu"),
      sensorId, sSensor.name,
      temperature, humidity, batteryMv, batteryLevel
    );

    // update data
    Sensors::setValueById(sensorId, temperature, Sensors::ValueType::TEMPERATURE, true, true);
    Sensors::setValueById(sensorId, humidity, Sensors::ValueType::HUMIDITY, true, true);
    Sensors::setValueById(sensorId, batteryLevel, Sensors::ValueType::BATTERY, true, true);

    // update rssi
    Sensors::setValueById(sensorId, deviceRssi, Sensors::ValueType::RSSI, false, false);
  }
};
#endif

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {
    this->owInstances.reserve(2);
    this->dallasInstances.reserve(2);
    this->dallasSearchTime.reserve(2);
    this->dallasPolling.reserve(2);
    this->dallasLastPollingTime.reserve(2);

    #if USE_BLE
    this->bluetoothScanCallbacks = new BluetoothScanCallbacks();
    #endif
  }

  ~SensorsTask() {
    this->dallasInstances.clear();
    this->owInstances.clear();
    this->dallasSearchTime.clear();
    this->dallasPolling.clear();
    this->dallasLastPollingTime.clear();

    #if USE_BLE
    delete this->bluetoothScanCallbacks;
    #endif
  }

protected:
  const unsigned int disconnectedTimeout = 180000u;
  const unsigned short dallasSearchInterval = 60000u;
  const unsigned short dallasPollingInterval = 10000u;
  const unsigned short globalPollingInterval = 15000u;

  std::unordered_map<uint8_t, OneWire> owInstances;
  std::unordered_map<uint8_t, DallasTemperature> dallasInstances;
  std::unordered_map<uint8_t, unsigned long> dallasSearchTime;
  std::unordered_map<uint8_t, bool> dallasPolling;
  std::unordered_map<uint8_t, unsigned long> dallasLastPollingTime;
  #if USE_BLE
  NimBLEScan* pBLEScan = nullptr;
  BluetoothScanCallbacks* bluetoothScanCallbacks = nullptr;
  #endif
  unsigned long globalLastPollingTime = 0;

  #if defined(ARDUINO_ARCH_ESP32)
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
    if (vars.states.restarting || vars.states.upgrading) {
      return;
    }

    if (isPollingDallasSensors()) {
      pollingDallasSensors(false);
      this->yield();
    }

    if (millis() - this->globalLastPollingTime > this->globalPollingInterval) {
      cleanDallasInstances();
      makeDallasInstances();
      this->yield();

      searchDallasSensors();
      fillingAddressesDallasSensors();
      this->yield();

      pollingDallasSensors();
      this->yield();

      pollingNtcSensors();
      this->yield();

      #if USE_BLE
      scanBleSensors();
      this->yield();
      #endif

      this->globalLastPollingTime = millis();
    }

    updateConnectionStatus();
    updateMasterValues();
  }

  void updateMasterValues() {
    vars.master.heating.outdoorTemp = Sensors::getMeanValueByPurpose(Sensors::Purpose::OUTDOOR_TEMP, Sensors::ValueType::PRIMARY);
    vars.master.heating.indoorTemp = Sensors::getMeanValueByPurpose(Sensors::Purpose::INDOOR_TEMP, Sensors::ValueType::PRIMARY);

    vars.master.heating.currentTemp = Sensors::getMeanValueByPurpose(Sensors::Purpose::HEATING_TEMP, Sensors::ValueType::PRIMARY);
    vars.master.heating.returnTemp = Sensors::getMeanValueByPurpose(Sensors::Purpose::HEATING_RETURN_TEMP, Sensors::ValueType::PRIMARY);

    vars.master.dhw.currentTemp = Sensors::getMeanValueByPurpose(Sensors::Purpose::DHW_TEMP, Sensors::ValueType::PRIMARY);
    vars.master.dhw.returnTemp = Sensors::getMeanValueByPurpose(Sensors::Purpose::DHW_RETURN_TEMP, Sensors::ValueType::PRIMARY);
  }

  void makeDallasInstances() {
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      auto& sSensor = Sensors::settings[sensorId];

      if (!sSensor.enabled || sSensor.type != Sensors::Type::DALLAS_TEMP || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;

      } else if (this->dallasInstances.count(sSensor.gpio)) {
        // no need to make instances
        continue;
      }

      auto& owInstance = this->owInstances[sSensor.gpio];
      owInstance.begin(sSensor.gpio);
      owInstance.reset();

      this->dallasSearchTime[sSensor.gpio] = 0;
      this->dallasPolling[sSensor.gpio] = false;
      this->dallasLastPollingTime[sSensor.gpio] = 0;

      auto& instance = this->dallasInstances[sSensor.gpio];
      instance.setOneWire(&owInstance);
      instance.setWaitForConversion(false);

      Log.sinfoln(FPSTR(L_SENSORS_DALLAS), F("Started on GPIO %hhu"), sSensor.gpio);
    }
  }

  void cleanDallasInstances() {
    // for (auto& [gpio, instance] : this->dallasInstances) {
    auto it = this->dallasInstances.begin();
    while (it != this->dallasInstances.end()) {
      auto gpio = it->first;
      bool instanceUsed = false;

      for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
        auto& sSensor = Sensors::settings[sensorId];
        
        if (!sSensor.enabled || sSensor.type != Sensors::Type::DALLAS_TEMP || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
          continue;
        }

        if (Sensors::settings[sensorId].gpio == gpio) {
          instanceUsed = true;
          break;
        }
      }

      if (!instanceUsed) {
        it = this->dallasInstances.erase(it);
        this->owInstances.erase(gpio);
        this->dallasSearchTime.erase(gpio);
        this->dallasPolling.erase(gpio);
        this->dallasLastPollingTime.erase(gpio);

        Log.sinfoln(FPSTR(L_SENSORS_DALLAS), F("Stopped on GPIO %hhu"), gpio);
        continue;
      }

      it++;
    }
  }

  void searchDallasSensors() {
    // search sensors on bus
    for (auto& [gpio, instance] : this->dallasInstances) {
      // do not search if polling!
      if (this->dallasPolling[gpio]) {
        continue;
      }

      if (millis() - this->dallasSearchTime[gpio] < this->dallasSearchInterval) {
        continue;
      }

      this->dallasSearchTime[gpio] = millis();
      this->owInstances[gpio].reset();
      instance.begin();

      Log.straceln(
        FPSTR(L_SENSORS_DALLAS),
        F("GPIO %hhu, devices on bus: %hhu, DS18* devices: %hhu"),
        gpio, instance.getDeviceCount(), instance.getDS18Count()
      );
    }
  }

  void fillingAddressesDallasSensors() {
    // check & filling sensors address
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      auto& sSensor = Sensors::settings[sensorId];
      
      if (!sSensor.enabled || sSensor.type != Sensors::Type::DALLAS_TEMP || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;

      } else if (!this->dallasInstances.count(sSensor.gpio)) {
        continue;
      }

      // do nothing if address not empty
      if (!isEmptyAddress(sSensor.address)) {
        continue;
      }

      // do nothing if polling
      if (this->dallasPolling[sSensor.gpio]) {
        continue;
      }

      auto& instance = this->dallasInstances[sSensor.gpio];
      DeviceAddress devAddr;
      for (uint8_t devId = 0; devId < instance.getDeviceCount(); devId++) {
        if (!instance.getAddress(devAddr, devId)) {
          continue;
        }

        bool freeAddress = true;

        // checking address usage
        for (uint8_t checkingSensorId = 0; checkingSensorId <= Sensors::getMaxSensorId(); checkingSensorId++) {
          auto& sCheckingSensor = Sensors::settings[checkingSensorId];
          if (sCheckingSensor.type != Sensors::Type::DALLAS_TEMP || checkingSensorId == sensorId) {
            continue;
          }
          
          if (sCheckingSensor.gpio != sSensor.gpio || isEmptyAddress(sCheckingSensor.address)) {
            continue;
          }

          if (isEqualAddress(sCheckingSensor.address, devAddr)) {
            freeAddress = false;
            break;
          }
        }

        // address already in use
        if (!freeAddress) {
          continue;
        }

        // set address
        for (uint8_t i = 0; i < 8; i++) {
          sSensor.address[i] = devAddr[i];
        }

        fsSensorsSettings.update();
        Log.straceln(
          FPSTR(L_SENSORS_DALLAS), F("GPIO %hhu, sensor #%hhu '%s', set address: %hhX:%hhX:%hhX:%hhX:%hhX:%hhX:%hhX:%hhX"),
          sSensor.gpio, sensorId, sSensor.name,
          sSensor.address[0], sSensor.address[1], sSensor.address[2], sSensor.address[3],
          sSensor.address[4], sSensor.address[5], sSensor.address[6], sSensor.address[7]
        );

        break;
      }
    }
  }

  bool isPollingDallasSensors() {
    for (auto& [gpio, instance] : this->dallasInstances) {
      if (this->dallasPolling.count(gpio) && this->dallasPolling[gpio]) {
        return true;
      }
    }

    return false;
  }

  void pollingDallasSensors(bool newPolling = true) {
    for (auto& [gpio, instance] : this->dallasInstances) {
      unsigned long ts = millis();

      if (this->dallasPolling[gpio]) {
        unsigned long minPollingTime = instance.millisToWaitForConversion(12) * 2;
        unsigned long estimatePollingTime = ts - this->dallasLastPollingTime[gpio];

        // check conversion time
        // isConversionComplete does not work with chinese clones!
        if (estimatePollingTime < minPollingTime) {
          continue;
        }

        // read sensors data for current instance
        for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
          auto& sSensor = Sensors::settings[sensorId];
          
          // only target & valid sensors
          if (!sSensor.enabled || sSensor.type != Sensors::Type::DALLAS_TEMP || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
            continue;

          } else if (sSensor.gpio != gpio || isEmptyAddress(sSensor.address)) {
            continue;
          }
          
          auto& rSensor = Sensors::results[sensorId];
          float value = instance.getTempC(sSensor.address);
          if (value == DEVICE_DISCONNECTED_C) {
            Log.swarningln(
              FPSTR(L_SENSORS_DALLAS), F("GPIO %hhu, sensor #%hhu '%s': failed receiving data"),
              sSensor.gpio, sensorId, sSensor.name
            );

            if (rSensor.signalQuality > 0) {
              rSensor.signalQuality--;
            }

            continue;
          }

          Log.straceln(
            FPSTR(L_SENSORS_DALLAS), F("GPIO %hhu, sensor #%hhu '%s', received data: %.2f"),
            sSensor.gpio, sensorId, sSensor.name, value
          );

          if (rSensor.signalQuality < 100) {
            rSensor.signalQuality++;
          }

          // set sensor value
          Sensors::setValueById(sensorId, value, Sensors::ValueType::TEMPERATURE, true, true);
        }

        // reset polling flag
        this->dallasPolling[gpio] = false;

      } else if (newPolling) {
        auto estimateLastPollingTime = ts - this->dallasLastPollingTime[gpio];

        // check last polling time
        if (estimateLastPollingTime < this->dallasPollingInterval) {
          continue;
        }

        // start polling
        instance.setResolution(12);
        instance.requestTemperatures();
        this->dallasPolling[gpio] = true;
        this->dallasLastPollingTime[gpio] = ts;

        Log.straceln(FPSTR(L_SENSORS_DALLAS), F("GPIO %hhu, polling..."), gpio);
      }
    }
  }

  void pollingNtcSensors() {
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      auto& sSensor = Sensors::settings[sensorId];
      
      if (!sSensor.enabled || sSensor.type != Sensors::Type::NTC_10K_TEMP || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;
      }

      #ifdef ARDUINO_ARCH_ESP32
      const auto value = analogReadMilliVolts(sSensor.gpio);
      #else
      const auto value = analogRead(sSensor.gpio) / 1023 * DEFAULT_NTC_VREF;
      #endif

      if (value < DEFAULT_NTC_VLOW_TRESHOLD || value > DEFAULT_NTC_VHIGH_TRESHOLD) {
        if (Sensors::getConnectionStatusById(sensorId)) {
          Sensors::setConnectionStatusById(sensorId, false, false);
        }

        Log.swarningln(
          FPSTR(L_SENSORS_NTC), F("GPIO %hhu, sensor #%hhu '%s', voltage is out of threshold: %.3f"),
          sSensor.gpio, sensorId, sSensor.name, (value / 1000.0f)
        );

        continue;
      }

      const float sensorResistance = value > 1
        ? DEFAULT_NTC_REF_RESISTANCE / (DEFAULT_NTC_VREF / (float) value - 1.0f)
        : 0.0f;
      const float rawTemp = 1.0f / (
        1.0f / (DEFAULT_NTC_NOMINAL_TEMP + 273.15f) +
        log(sensorResistance / DEFAULT_NTC_NOMINAL_RESISTANCE) / DEFAULT_NTC_BETA_FACTOR
      ) - 273.15f;

      Log.straceln(
        FPSTR(L_SENSORS_NTC), F("GPIO %hhu, sensor #%hhu '%s', raw temp: %.2f, raw voltage: %.3f, raw resistance: %.2f"),
        sSensor.gpio, sensorId, sSensor.name, rawTemp, (value / 1000.0f), sensorResistance
      );

      // set temp
      Sensors::setValueById(sensorId, rawTemp, Sensors::ValueType::TEMPERATURE, true, true);
    }
  }

  #if USE_BLE
  void scanBleSensors() {
    if (!Sensors::getAmountByType(Sensors::Type::BLUETOOTH, true)) {
      if (NimBLEDevice::isInitialized()) {
        if (this->pBLEScan != nullptr) {
          if (this->pBLEScan->isScanning()) {
            this->pBLEScan->stop();

          } else {
            this->pBLEScan = nullptr;
          }
        }

        if (this->pBLEScan == nullptr) {
          if (NimBLEDevice::deinit(true)) {
            Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Deinitialized"));

          } else {
            Log.swarningln(FPSTR(L_SENSORS_BLE), F("Unable to deinitialize!"));
          }
        }
      }

      return;
    }

    if (!NimBLEDevice::isInitialized() && millis() > 5000) {
      Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Initialized"));
      BLEDevice::init("");
      
      #if defined(ESP_PWR_LVL_P20)
      NimBLEDevice::setPower(ESP_PWR_LVL_P20);
      #elif defined(ESP_PWR_LVL_P9)
      NimBLEDevice::setPower(ESP_PWR_LVL_P9);
      #endif
    }

    if (this->pBLEScan == nullptr) {
      this->pBLEScan = NimBLEDevice::getScan();
      this->pBLEScan->setScanCallbacks(this->bluetoothScanCallbacks);
      this->pBLEScan->setActiveScan(false);
      this->pBLEScan->setDuplicateFilter(false);
      this->pBLEScan->setMaxResults(0);
      this->pBLEScan->setInterval(10);
      this->pBLEScan->setWindow(10);

      Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Scanning initialized"));
    }

    if (!this->pBLEScan->isScanning()) {
      if (this->pBLEScan->start(0, false, true)) {
        Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Scanning started"));

      } else {
        Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Unable to start scanning"));
      }
    }
  }
  #endif

  void updateConnectionStatus() {
    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      auto& sSensor = Sensors::settings[sensorId];
      auto& rSensor = Sensors::results[sensorId];

      if (rSensor.connected && !sSensor.enabled) {
        Sensors::setConnectionStatusById(sensorId, false, false);

      } else if (rSensor.connected && sSensor.type == Sensors::Type::NOT_CONFIGURED) {
        Sensors::setConnectionStatusById(sensorId, false, false);

      } else if (rSensor.connected && sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        Sensors::setConnectionStatusById(sensorId, false, false);

      } else if (sSensor.type != Sensors::Type::MANUAL && rSensor.connected && (millis() - rSensor.activityTime) > this->disconnectedTimeout) {
        Sensors::setConnectionStatusById(sensorId, false, false);

      }/* else if (!rSensor.connected) {
        rSensor.connected = true;
      }*/
    }
  }

  static bool isEqualAddress(const uint8_t *addr1, const uint8_t *addr2, const uint8_t length = 8) {
    return memcmp(addr1, addr2, length) == 0;
    /*bool result = true;

    for (uint8_t i = 0; i < length; i++) {
      if (addr1[i] != addr2[i]) {
        result = false;
        break;
      }
    }

    return result;*/
  }

  static bool isEmptyAddress(const uint8_t *addr, const uint8_t length = 8) {
    bool result = true;

    for (uint8_t i = 0; i < length; i++) {
      if (addr[i] != 0) {
        result = false;
        break;
      }
    }

    return result;
  }
};