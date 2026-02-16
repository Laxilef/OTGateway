#include <unordered_map>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp32DHT.h>

#if USE_BLE
  #include <NimBLEDevice.h>
#endif

extern FileData fsSensorsSettings;

#if USE_BLE
class BluetoothClientCallbacks : public NimBLEClientCallbacks {
public:
  BluetoothClientCallbacks(uint8_t sensorId) : sensorId(sensorId) {}

  void onConnect(NimBLEClient* pClient) {
    auto& sSensor = Sensors::settings[this->sensorId];

    Log.sinfoln(
      FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': connected to %s"),
      sensorId, sSensor.name, pClient->getPeerAddress().toString().c_str()
    );
  }

  void onDisconnect(NimBLEClient* pClient, int reason) {
    auto& sSensor = Sensors::settings[this->sensorId];

    Log.sinfoln(
      FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': disconnected, reason %i"),
      sensorId, sSensor.name, reason
    );
  }

  void onConnectFail(NimBLEClient* pClient, int reason) {
    auto& sSensor = Sensors::settings[this->sensorId];

    Log.sinfoln(
      FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': failed to connect, reason %i"),
      sensorId, sSensor.name, reason
    );

    pClient->cancelConnect();
  }

protected:
  uint8_t sensorId;
};
#endif

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {
    this->gpioLastPollingTime.reserve(2);

    // OneWire
    this->owInstances.reserve(2);
    this->dallasInstances.reserve(2);
    this->dallasSearchTime.reserve(2);
    this->dallasPolling.reserve(2);
  }

  ~SensorsTask() {
    this->gpioLastPollingTime.clear();

    // OneWire
    this->dallasInstances.clear();
    this->owInstances.clear();
    this->dallasSearchTime.clear();
    this->dallasPolling.clear();
  }

protected:
  const unsigned int wiredDisconnectTimeout = 180000u;
  const unsigned int wirelessDisconnectTimeout = 600000u;
  const unsigned short dallasSearchInterval = 60000;
  const unsigned short dallasPollingInterval = 10000;
  const unsigned short dhtPollingInterval = 15000;
  const unsigned short globalPollingInterval = 15000;
  #if USE_BLE
  const unsigned int bleSetDtInterval = 7200000;
  #endif

  std::unordered_map<uint8_t, unsigned long> gpioLastPollingTime;

  // OneWire
  std::unordered_map<uint8_t, OneWire> owInstances;
  std::unordered_map<uint8_t, DallasTemperature> dallasInstances;
  std::unordered_map<uint8_t, unsigned long> dallasSearchTime;
  std::unordered_map<uint8_t, bool> dallasPolling;

  // DHT
  DHT dhtInstance;
  bool dhtIsPolling = false;

  #if USE_BLE
  // Bluetooth
  std::unordered_map<uint8_t, NimBLEClient*> bleClients;
  std::unordered_map<uint8_t, bool> bleSubscribed;
  std::unordered_map<uint8_t, unsigned long> bleLastSetDtTime;
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

    pollingDhtSensors();
    this->yield();

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
      cleanBleInstances();
      pollingBleSensors();
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
      this->gpioLastPollingTime[sSensor.gpio] = 0;

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
        this->gpioLastPollingTime.erase(gpio);

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
        unsigned long estimatePollingTime = ts - this->gpioLastPollingTime[gpio];

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
        auto estimateLastPollingTime = ts - this->gpioLastPollingTime[gpio];

        // check last polling time
        if (estimateLastPollingTime < this->dallasPollingInterval) {
          continue;
        }

        // start polling
        instance.setResolution(12);
        instance.requestTemperatures();
        this->dallasPolling[gpio] = true;
        this->gpioLastPollingTime[gpio] = ts;

        Log.straceln(FPSTR(L_SENSORS_DALLAS), F("GPIO %hhu, polling..."), gpio);
      }
    }
  }

  void pollingDhtSensors() {
    if (this->dhtIsPolling) {
      // busy
      return;
    }

    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      auto& sSensor = Sensors::settings[sensorId];
      
      if (!sSensor.enabled || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;
      }

      if (sSensor.type != Sensors::Type::DHT11 && sSensor.type != Sensors::Type::DHT22) {
        continue;
      }

      if (this->gpioLastPollingTime.count(sSensor.gpio) && millis() - this->gpioLastPollingTime[sSensor.gpio] < this->dhtPollingInterval) {
        continue;
      }

      const auto sensorGpio = static_cast<gpio_num_t>(sSensor.gpio);
      if (this->dhtInstance.getGpio() != sensorGpio) {
        this->dhtInstance.reset();
        this->dhtInstance.onData([this, sensorId](float humidity, float temperature) {
          auto& sSensor = Sensors::settings[sensorId];

          Log.straceln(
            FPSTR(L_SENSORS_DHT), F("GPIO %hhu, sensor #%hhu '%s', temp: %.2f, humidity: %.2f%%"),
            sSensor.gpio, sensorId, sSensor.name, temperature, humidity
          );

          // set temperature
          Sensors::setValueById(sensorId, temperature, Sensors::ValueType::TEMPERATURE, true, true);

          // set humidity
          Sensors::setValueById(sensorId, humidity, Sensors::ValueType::HUMIDITY, true, true);

          auto& rSensor = Sensors::results[sensorId];
          if (rSensor.signalQuality < 100) {
            rSensor.signalQuality++;
          }

          this->gpioLastPollingTime[sSensor.gpio] = millis();
          this->dhtIsPolling = false;
        });

        this->dhtInstance.onError([this, sensorId](DHT::Status status) {
          auto& sSensor = Sensors::settings[sensorId];

          Log.swarningln(
            FPSTR(L_SENSORS_DHT), F("GPIO %hhu, sensor #%hhu '%s': failed receiving data (err: %s)"),
            sSensor.gpio, sensorId, sSensor.name, DHT::statusToString(this->dhtInstance.getStatus())
          );

          auto& rSensor = Sensors::results[sensorId];
          if (rSensor.signalQuality > 0) {
            rSensor.signalQuality--;
          }
          
          this->gpioLastPollingTime[sSensor.gpio] = millis();
          this->dhtIsPolling = false;
        });

        DHT::Type sType = DHT::Type::DHT22;
        if (sSensor.type == Sensors::Type::DHT11) {
          sType = DHT::Type::DHT11;

        } else if (sSensor.type == Sensors::Type::DHT22) {
          sType = DHT::Type::DHT22;
        }

        if (this->dhtInstance.setup(sensorGpio, sType)) {
          Log.sinfoln(FPSTR(L_SENSORS_DHT), F("Started on GPIO %hhu"), sSensor.gpio);

        } else {
          Log.swarningln(
            FPSTR(L_SENSORS_DHT), F("Failed to start on GPIO %hhu (err: %s)"),
            sSensor.gpio, DHT::statusToString(this->dhtInstance.getStatus())
          );
        }
      }

      this->dhtIsPolling = this->dhtInstance.poll();

      break;
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
  void cleanBleInstances() {
    if (!NimBLEDevice::isInitialized()) {
      return;
    }

    for (auto& [sensorId, pClient]: this->bleClients) {
      if (pClient == nullptr) {
        continue;
      }

      auto& sSensor = Sensors::settings[sensorId];
      const auto sAddress = NimBLEAddress(sSensor.address, 0);

      if (sAddress.isNull() || !sSensor.enabled || sSensor.type != Sensors::Type::BLUETOOTH || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        Log.sinfoln(
          FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s', deleted unused client"),
          sensorId, sSensor.name
        );

        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
      }
    }
  }

  void pollingBleSensors() {
    if (!Sensors::getAmountByType(Sensors::Type::BLUETOOTH, true)) {
      return;
    }

    if (!NimBLEDevice::isInitialized() && millis() > 5000) {
      Log.sinfoln(FPSTR(L_SENSORS_BLE), F("Initialized"));
      BLEDevice::init("");
      NimBLEDevice::setPower(9);
    }

    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      auto& sSensor = Sensors::settings[sensorId];
      auto& rSensor = Sensors::results[sensorId];
      
      if (!sSensor.enabled || sSensor.type != Sensors::Type::BLUETOOTH || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;
      }

      const auto address = NimBLEAddress(sSensor.address, 0);
      if (address.isNull()) {
        continue;
      }

      auto pClient = this->getBleClient(sensorId);
      if (pClient == nullptr) {
        continue;
      }

      if (pClient->getPeerAddress() != address) {
        if (pClient->isConnected()) {
          if (!pClient->disconnect()) {
            continue;
          }
        }

        pClient->setPeerAddress(address);
      }

      if (!pClient->isConnected()) {
        this->bleSubscribed[sensorId] = false;
        this->bleLastSetDtTime[sensorId] = 0;

        if (pClient->connect(false, true, true)) {
          Log.sinfoln(
            FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': trying connecting to %s..."),
            sensorId, sSensor.name, pClient->getPeerAddress().toString().c_str()
          );
        }

        continue;
      }
      
      if (!this->bleSubscribed[sensorId]) {
        if (this->subscribeToBleDevice(sensorId, pClient)) {
          this->bleSubscribed[sensorId] = true;

        } else {
          this->bleSubscribed[sensorId] = false;
          pClient->disconnect();
          continue;
        }
      }

      // Mark connected
      Sensors::setConnectionStatusById(sensorId, true, true);

      if (!this->bleLastSetDtTime[sensorId] || millis() - this->bleLastSetDtTime[sensorId] > this->bleSetDtInterval) {
        struct tm ti;
        
        if (getLocalTime(&ti)) {
          if (this->setDateOnBleSensor(pClient, &ti)) {
            Log.sinfoln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s', successfully set date: %02d.%02d.%04d %02d:%02d:%02d"),
              sensorId, sSensor.name,
              ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900, ti.tm_hour, ti.tm_min, ti.tm_sec
            );

          } else {
            Log.swarningln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s', failed set date: %02d.%02d.%04d %02d:%02d:%02d"),
              sensorId, sSensor.name,
              ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900, ti.tm_hour, ti.tm_min, ti.tm_sec
            );
          }

          this->bleLastSetDtTime[sensorId] = millis();
        }
      }
    }
  }

  NimBLEClient* getBleClient(const uint8_t sensorId) {
    if (!NimBLEDevice::isInitialized()) {
      return nullptr;
    }

    auto& sSensor = Sensors::settings[sensorId];
    auto& rSensor = Sensors::results[sensorId];

    if (!sSensor.enabled || sSensor.type != Sensors::Type::BLUETOOTH || sSensor.purpose == Sensors::Purpose::NOT_CONFIGURED) {
      return nullptr;
    }

    if (this->bleClients[sensorId] && this->bleClients[sensorId] != nullptr) {
      return this->bleClients[sensorId];
    }

    auto pClient = NimBLEDevice::createClient();
    if (pClient == nullptr) {
      return nullptr;
    }

    //pClient->setConnectionParams(BLE_GAP_CONN_ITVL_MS(10), BLE_GAP_CONN_ITVL_MS(100), 10, 150);
    pClient->setConnectTimeout(30000);
    pClient->setSelfDelete(false, false);
    pClient->setClientCallbacks(new BluetoothClientCallbacks(sensorId), true);

    this->bleClients[sensorId] = pClient;

    return pClient;
  }

  bool subscribeToBleDevice(const uint8_t sensorId, NimBLEClient* pClient) {
    auto& sSensor = Sensors::settings[sensorId];
    auto pAddress = pClient->getPeerAddress().toString();
    
    NimBLERemoteService* pService = nullptr;
    NimBLERemoteCharacteristic* pChar = nullptr;

    // ENV Service (0x181A)
    NimBLEUUID serviceUuid((uint16_t) 0x181AU);
    pService = pClient->getService(serviceUuid);
    if (!pService) {
      Log.straceln(
        FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': failed to find env service (%s) on device %s"),
        sensorId, sSensor.name, serviceUuid.toString().c_str(), pAddress.c_str()
      );

    } else {
      Log.straceln(
        FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found env service (%s) on device %s"),
        sensorId, sSensor.name, serviceUuid.toString().c_str(), pAddress.c_str()
      );

      // 0x2A6E - Notify temperature x0.01C (pvvx)
      bool tempNotifyCreated = false;
      if (!tempNotifyCreated) {
        NimBLEUUID charUuid((uint16_t) 0x2A6E);
        pChar = pService->getCharacteristic(charUuid);

        if (pChar && (pChar->canNotify() || pChar->canIndicate())) {
          Log.straceln(
            FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found temp char (%s) in env service on device %s"),
            sensorId, sSensor.name, charUuid.toString().c_str(), pAddress.c_str()
          );

          pChar->unsubscribe();
          tempNotifyCreated = pChar->subscribe(
            pChar->canNotify(),
            [sensorId](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
              if (pChar == nullptr) {
                return;
              }

              const NimBLERemoteService* pService = pChar->getRemoteService();
              if (pService == nullptr) {
                return;
              }

              NimBLEClient* pClient = pService->getClient();
              if (pClient == nullptr) {
                return;
              }

              auto& sSensor = Sensors::settings[sensorId];

              if (length != 2) {
                Log.swarningln(
                  FPSTR(L_SENSORS_BLE),
                  F("Sensor #%hhu '%s': invalid notification data at temp char (%s) on device %s"),
                  sensorId,
                  sSensor.name,
                  pChar->getUUID().toString().c_str(),
                  pClient->getPeerAddress().toString().c_str()
                );

                return;
              }

              float rawTemp = (pChar->getValue<int16_t>() * 0.01f);
              Log.straceln(
                FPSTR(L_SENSORS_BLE),
                F("Sensor #%hhu '%s': received temp: %.2f"),
                sensorId, sSensor.name, rawTemp
              );

              // set temp
              Sensors::setValueById(sensorId, rawTemp, Sensors::ValueType::TEMPERATURE, true, true);

              // update rssi
              Sensors::setValueById(sensorId, pClient->getRssi(), Sensors::ValueType::RSSI, false, false);
            }
          );

          if (tempNotifyCreated) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': subscribed to temp char (%s) in env service on device %s"),
              sensorId, sSensor.name,
              charUuid.toString().c_str(), pAddress.c_str()
            );

          } else {
            Log.swarningln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': failed to subscribe to temp char (%s) in env service on device %s"),
              sensorId, sSensor.name,
              charUuid.toString().c_str(), pAddress.c_str()
            );
          }
        }
      }


      // 0x2A1F - Notify temperature x0.1C (atc1441/pvvx)
      if (!tempNotifyCreated) {
        NimBLEUUID charUuid((uint16_t) 0x2A1F);
        pChar = pService->getCharacteristic(charUuid);

        if (pChar && (pChar->canNotify() || pChar->canIndicate())) {
          Log.straceln(
            FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found temp char (%s) in env service on device %s"),
            sensorId, sSensor.name, charUuid.toString().c_str(), pAddress.c_str()
          );

          pChar->unsubscribe();
          tempNotifyCreated = pChar->subscribe(
            pChar->canNotify(),
            [sensorId](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
              if (pChar == nullptr) {
                return;
              }

              const NimBLERemoteService* pService = pChar->getRemoteService();
              if (pService == nullptr) {
                return;
              }

              NimBLEClient* pClient = pService->getClient();
              if (pClient == nullptr) {
                return;
              }

              auto& sSensor = Sensors::settings[sensorId];

              if (length != 2) {
                Log.swarningln(
                  FPSTR(L_SENSORS_BLE),
                  F("Sensor #%hhu '%s': invalid notification data at temp char (%s) on device %s"),
                  sensorId,
                  sSensor.name,
                  pChar->getUUID().toString().c_str(),
                  pClient->getPeerAddress().toString().c_str()
                );

                return;
              }

              float rawTemp = (pChar->getValue<int16_t>() * 0.1f);
              Log.straceln(
                FPSTR(L_SENSORS_BLE),
                F("Sensor #%hhu '%s': received temp: %.2f"),
                sensorId, sSensor.name, rawTemp
              );

              // set temp
              Sensors::setValueById(sensorId, rawTemp, Sensors::ValueType::TEMPERATURE, true, true);

              // update rssi
              Sensors::setValueById(sensorId, pClient->getRssi(), Sensors::ValueType::RSSI, false, false);
            }
          );

          if (tempNotifyCreated) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': subscribed to temp char (%s) in env service on device %s"),
              sensorId, sSensor.name,
              charUuid.toString().c_str(), pAddress.c_str()
            );

          } else {
            Log.swarningln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': failed to subscribe to temp char (%s) in env service on device %s"),
              sensorId, sSensor.name,
              charUuid.toString().c_str(), pAddress.c_str()
            );
          }
        }
      }

      if (!tempNotifyCreated) {
        Log.swarningln(
          FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': not found supported temp chars in env service on device %s"),
          sensorId, sSensor.name, pAddress.c_str()
        );

        pClient->disconnect();
        return false;
      }


      // 0x2A6F - Notify about humidity x0.01% (pvvx)
      {
        bool humidityNotifyCreated = false;
        if (!humidityNotifyCreated) {
          NimBLEUUID charUuid((uint16_t) 0x2A6F);
          pChar = pService->getCharacteristic(charUuid);

          if (pChar && (pChar->canNotify() || pChar->canIndicate())) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found humidity char (%s) in env service on device %s"),
              sensorId, sSensor.name, charUuid.toString().c_str(), pAddress.c_str()
            );

            pChar->unsubscribe();
            humidityNotifyCreated = pChar->subscribe(
              pChar->canNotify(),
              [sensorId](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                if (pChar == nullptr) {
                  return;
                }

                const NimBLERemoteService* pService = pChar->getRemoteService();
                if (pService == nullptr) {
                  return;
                }

                NimBLEClient* pClient = pService->getClient();
                if (pClient == nullptr) {
                  return;
                }

                auto& sSensor = Sensors::settings[sensorId];

                if (length != 2) {
                  Log.swarningln(
                    FPSTR(L_SENSORS_BLE),
                    F("Sensor #%hhu '%s': invalid notification data at humidity char (%s) on device %s"),
                    sensorId,
                    sSensor.name,
                    pChar->getUUID().toString().c_str(),
                    pClient->getPeerAddress().toString().c_str()
                  );

                  return;
                }

                float rawHumidity = (pChar->getValue<uint16_t>() * 0.01f);
                Log.straceln(
                  FPSTR(L_SENSORS_BLE),
                  F("Sensor #%hhu '%s': received humidity: %.2f"),
                  sensorId, sSensor.name, rawHumidity
                );

                // set humidity
                Sensors::setValueById(sensorId, rawHumidity, Sensors::ValueType::HUMIDITY, true, true);

                // update rssi
                Sensors::setValueById(sensorId, pClient->getRssi(), Sensors::ValueType::RSSI, false, false);
              }
            );

            if (humidityNotifyCreated) {
              Log.straceln(
                FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': subscribed to humidity char (%s) in env service on device %s"),
                sensorId, sSensor.name,
                charUuid.toString().c_str(), pAddress.c_str()
              );

            } else {
              Log.swarningln(
                FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': failed to subscribe to humidity char (%s) in env service on device %s"),
                sensorId, sSensor.name,
                charUuid.toString().c_str(), pAddress.c_str()
              );
            }
          }
        }

        if (!humidityNotifyCreated) {
          Log.swarningln(
            FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': not found supported humidity chars in env service on device %s"),
            sensorId, sSensor.name, pAddress.c_str()
          );
        }
      }
    }


    // Battery Service (0x180F)
    {
      NimBLEUUID serviceUuid((uint16_t) 0x180F);
      pService = pClient->getService(serviceUuid);
      if (!pService) {
        Log.straceln(
          FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': failed to find battery service (%s) on device %s"),
          sensorId, sSensor.name, serviceUuid.toString().c_str(), pAddress.c_str()
        );

      } else {
        Log.straceln(
          FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found battery service (%s) on device %s"),
          sensorId, sSensor.name, serviceUuid.toString().c_str(), pAddress.c_str()
        );

        // 0x2A19 - Notify the battery charge level 0..99% (pvvx)
        bool batteryNotifyCreated = false;
        if (!batteryNotifyCreated) {
          NimBLEUUID charUuid((uint16_t) 0x2A19);
          pChar = pService->getCharacteristic(charUuid);

          if (pChar && (pChar->canNotify() || pChar->canIndicate())) {
            Log.straceln(
              FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': found battery char (%s) in battery service on device %s"),
              sensorId, sSensor.name, charUuid.toString().c_str(), pAddress.c_str()
            );

            pChar->unsubscribe();
            batteryNotifyCreated = pChar->subscribe(
              pChar->canNotify(),
              [sensorId](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                if (pChar == nullptr) {
                  return;
                }

                const NimBLERemoteService* pService = pChar->getRemoteService();
                if (pService == nullptr) {
                  return;
                }

                NimBLEClient* pClient = pService->getClient();
                if (pClient == nullptr) {
                  return;
                }

                auto& sSensor = Sensors::settings[sensorId];

                if (length != 1) {
                  Log.swarningln(
                    FPSTR(L_SENSORS_BLE),
                    F("Sensor #%hhu '%s': invalid notification data at battery char (%s) on device %s"),
                    sensorId,
                    sSensor.name,
                    pChar->getUUID().toString().c_str(),
                    pClient->getPeerAddress().toString().c_str()
                  );

                  return;
                }

                auto rawBattery = pChar->getValue<uint8_t>();
                Log.straceln(
                  FPSTR(L_SENSORS_BLE),
                  F("Sensor #%hhu '%s': received battery: %hhu"),
                  sensorId, sSensor.name, rawBattery
                );

                // set battery
                Sensors::setValueById(sensorId, rawBattery, Sensors::ValueType::BATTERY, true, true);
                
                // update rssi
                Sensors::setValueById(sensorId, pClient->getRssi(), Sensors::ValueType::RSSI, false, false);
              }
            );

            if (batteryNotifyCreated) {
              Log.straceln(
                FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': subscribed to battery char (%s) in battery service on device %s"),
                sensorId, sSensor.name,
                charUuid.toString().c_str(), pAddress.c_str()
              );

            } else {
              Log.swarningln(
                FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': failed to subscribe to battery char (%s) in battery service on device %s"),
                sensorId, sSensor.name,
                charUuid.toString().c_str(), pAddress.c_str()
              );
            }
          }
        }

        if (!batteryNotifyCreated) {
          Log.swarningln(
            FPSTR(L_SENSORS_BLE), F("Sensor #%hhu '%s': not found supported battery chars in battery service on device %s"),
            sensorId, sSensor.name, pAddress.c_str()
          );
        }
      }
    }

    return true;
  }

  bool setDateOnBleSensor(NimBLEClient* pClient, const struct tm *ptm) {
    auto ts = mkgmtime(ptm);

    uint8_t data[5] = {};
    data[0] = 0x23;
    data[1] = ts & 0xff;
    data[2] = (ts >> 8) & 0xff;
    data[3] = (ts >> 16) & 0xff;
    data[4] = (ts >> 24) & 0xff;

    return pClient->setValue(
      NimBLEUUID((uint16_t) 0x1f10),
      NimBLEUUID((uint16_t) 0x1f1f),
      NimBLEAttValue(data, sizeof(data))
    );
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

      } else if (rSensor.connected) {
        if (sSensor.type == Sensors::Type::MANUAL || sSensor.type == Sensors::Type::BLUETOOTH) {
          if ((millis() - rSensor.activityTime) > this->wirelessDisconnectTimeout) {
            Sensors::setConnectionStatusById(sensorId, false, false);
          }

        } else if ((millis() - rSensor.activityTime) > this->wiredDisconnectTimeout) {
          Sensors::setConnectionStatusById(sensorId, false, false);
        }
      }
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