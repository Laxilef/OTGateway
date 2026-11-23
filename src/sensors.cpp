#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>
#include <atomic>
#include <optional>
#include <unordered_map>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp32DHT.h>
#include <math.h>
#include <string.h>

#if USE_BLE
  #include <NimBLEDevice.h>
#endif

#if USE_BLE
class BluetoothScanCallbacks : public NimBLEScanCallbacks {
public:
  void onDiscovered(const NimBLEAdvertisedDevice* device) override {
    auto& deviceAddress = device->getAddress();

    bool found = false;
    uint8_t sensorId;
    std::optional<Sensors::Settings> sSensor;
    for (sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      sSensor = Sensors::getSettings(sensorId);
      if (!sSensor) {
        continue;
      }
      if (!sSensor->enabled || sSensor->type != Sensors::Type::BLUETOOTH || sSensor->purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;
      }

      const auto sensorAddress = NimBLEAddress(sSensor->address, deviceAddress.getType());
      if (sensorAddress.isNull() || sensorAddress != deviceAddress) {
        continue;
      }

      found = true;
      break;
    }

    if (!found) {
      return;
    }

    auto deviceName = device->getName();
    auto deviceRssi = device->getRSSI();

    ESP_LOGI(
      L_SENSORS_BLE, "Sensor #%hhu '%s': discovered device %s, name: %s, RSSI: %hhd",
      sensorId, sSensor->name,
      deviceAddress.toString().c_str(), deviceName.c_str(), deviceRssi
    );

    if (!device->haveServiceData()) {
      ESP_LOGW(
        L_SENSORS_BLE, "Sensor #%hhu '%s': not found service data!",
        sensorId, sSensor->name
      );
      return;
    }

    auto serviceDataCount = device->getServiceDataCount();
    ESP_LOGD(
      L_SENSORS_BLE, "Sensor #%hhu '%s': found %hhu service data",
      sensorId, sSensor->name, serviceDataCount
    );

    if (parseAtcData(device, sensorId) || parsePvvxData(device, sensorId) || parseBTHomeData(device, sensorId)) {
      // update rssi
      Sensors::setValueById(sensorId, deviceRssi, Sensors::ValueType::RSSI, false, false);

    } else {
      ESP_LOGW(
        L_SENSORS_BLE, "Sensor #%hhu '%s': unsupported data format!",
        sensorId, sSensor->name
      );
    }
  }

  static bool parseAtcData(const NimBLEAdvertisedDevice* device, uint8_t sensorId) {
    NimBLEUUID serviceUuid(static_cast<uint16_t>(0x181A));

    auto serviceData = device->getServiceData(serviceUuid);
    if (!serviceData.size()) {
      ESP_LOGD(
        L_SENSORS_BLE, "Sensor #%hhu, service %s: not found ATC1441 data",
        sensorId, serviceUuid.toString().c_str()
      );
      return false;

    } else if (serviceData.size() != 13) {
      ESP_LOGD(
        L_SENSORS_BLE, "Sensor #%hhu, service %s: not in ATC1441 format",
        sensorId, serviceUuid.toString().c_str()
      );
      return false;
    }

    ESP_LOGI(
      L_SENSORS_BLE, "Sensor #%hhu, service %s: found ATC1441 format",
      sensorId, serviceUuid.toString().c_str()
    );

    // Temperature (2 bytes, signed, big-endian)
    float temperature = static_cast<int16_t>(
      (static_cast<uint8_t>(serviceData[6]) << 8) |
      static_cast<uint8_t>(serviceData[7])
    ) * 0.1f;
    Sensors::setValueById(
      sensorId, temperature,
      Sensors::ValueType::TEMPERATURE, true, true
    );

    // Humidity (1 byte)
    float humidity = static_cast<uint8_t>(serviceData[8]);
    Sensors::setValueById(sensorId, humidity, Sensors::ValueType::HUMIDITY, true, true);

    // Battery level (1 byte)
    uint8_t batteryLevel = static_cast<uint8_t>(serviceData[9]);
    Sensors::setValueById(sensorId, batteryLevel, Sensors::ValueType::BATTERY, true, true);

    // Battery mV (2 bytes, big-endian)
    uint16_t batteryMv = (static_cast<uint8_t>(serviceData[10]) << 8) | static_cast<uint8_t>(serviceData[11]);

    // Log
    ESP_LOGI(
      L_SENSORS_BLE,
      "Sensor #%hhu, received temp: %.2f; humidity: %.2f, battery voltage: %hu, battery level: %hhu",
      sensorId, temperature, humidity, batteryMv, batteryLevel
    );

    return true;
  }

  static bool parsePvvxData(const NimBLEAdvertisedDevice* device, uint8_t sensorId) {
    NimBLEUUID serviceUuid(static_cast<uint16_t>(0x181A));

    auto serviceData = device->getServiceData(serviceUuid);
    if (!serviceData.size()) {
      ESP_LOGD(
        L_SENSORS_BLE, "Sensor #%hhu, service %s: not found PVVX data",
        sensorId, serviceUuid.toString().c_str()
      );
      return false;

    } else if (serviceData.size() != 14 && serviceData.size() != 15) {
      ESP_LOGD(
        L_SENSORS_BLE, "Sensor #%hhu, service %s: not in PVVX format",
        sensorId, serviceUuid.toString().c_str()
      );
      return false;
    }

    ESP_LOGI(
      L_SENSORS_BLE, "Sensor #%hhu, service %s: found PVVX format",
      sensorId, serviceUuid.toString().c_str()
    );

    // Temperature (2 bytes, signed, little-endian)
    float temperature = static_cast<int16_t>(
      (static_cast<uint8_t>(serviceData[7]) << 8) |
      static_cast<uint8_t>(serviceData[6])
    ) * 0.01f;
    Sensors::setValueById(
      sensorId, temperature,
      Sensors::ValueType::TEMPERATURE, true, true
    );

    // Humidity (2 bytes, little-endian)
    float humidity = (
      (static_cast<uint8_t>(serviceData[9]) << 8) | static_cast<uint8_t>(serviceData[8])
    ) * 0.01f;
    Sensors::setValueById(sensorId, humidity, Sensors::ValueType::HUMIDITY, true, true);

    // Battery level (1 byte)
    uint8_t batteryLevel = static_cast<uint8_t>(serviceData[12]);
    Sensors::setValueById(sensorId, batteryLevel, Sensors::ValueType::BATTERY, true, true);

    // Battery mV (2 bytes, little-endian)
    uint16_t batteryMv = (static_cast<uint8_t>(serviceData[11]) << 8) | static_cast<uint8_t>(serviceData[10]);

    // Log
    ESP_LOGI(
      L_SENSORS_BLE,
      "Sensor #%hhu, received temp: %.2f; humidity: %.2f, battery voltage: %hu, battery level: %hhu",
      sensorId, temperature, humidity, batteryMv, batteryLevel
    );

    return true;
  }

  static bool parseBTHomeData(const NimBLEAdvertisedDevice* device, uint8_t sensorId) {
    NimBLEUUID serviceUuid(static_cast<uint16_t>(0xFCD2));

    auto serviceData = device->getServiceData(serviceUuid);
    if (!serviceData.size()) {
      ESP_LOGD(
        L_SENSORS_BLE, "Sensor #%hhu, service %s: not found BTHome data",
        sensorId, serviceUuid.toString().c_str()
      );
      return false;

    } else if ((serviceData[0] & 0xE0) != 0x40) {
      ESP_LOGD(
        L_SENSORS_BLE, "Sensor #%hhu, service %s: unsupported BTHome version",
        sensorId, serviceUuid.toString().c_str()
      );
      return false;

    } else if ((serviceData[0] & 0x01) != 0) {
      ESP_LOGD(
        L_SENSORS_BLE, "Sensor #%hhu, service %s: unsupported BTHome encrypted data",
        sensorId, serviceUuid.toString().c_str()
      );
      return false;
    }

    ESP_LOGI(
      L_SENSORS_BLE, "Sensor #%hhu, service %s: found BTHome format",
      sensorId, serviceUuid.toString().c_str()
    );

    bool foundData = false;
    size_t serviceDataPos = 1;
    while (serviceDataPos < serviceData.size()) {
      uint8_t objectId = serviceData[serviceDataPos++];

      switch (objectId) {
        // Packet ID (1 byte)
        case 0x00:
          if (serviceDataPos + 1 > serviceData.size()) {
            return foundData;
          }

          serviceDataPos += 1;
          break;

        // Battery (1 byte)
        case 0x01: {
          if (serviceDataPos + 1 > serviceData.size()) {
            return foundData;
          }

          uint8_t batteryLevel = static_cast<uint8_t>(serviceData[serviceDataPos]);
          Sensors::setValueById(sensorId, batteryLevel, Sensors::ValueType::BATTERY, true, true);
          serviceDataPos += 1;
          foundData = true;

          ESP_LOGI(
            L_SENSORS_BLE, "Sensor #%hhu, received battery level: %hhu",
            sensorId, batteryLevel
          );
          break;
        }

        // Temperature (2 bytes, signed, little-endian)
        case 0x02: {
          if (serviceDataPos + 2 > serviceData.size()) {
            return foundData;
          }

          int16_t rawTemperature = (static_cast<uint8_t>(serviceData[serviceDataPos + 1]) << 8)
                                   | static_cast<uint8_t>(serviceData[serviceDataPos]);
          float temperature = static_cast<float>(rawTemperature) * 0.01f;
          Sensors::setValueById(
            sensorId, temperature,
            Sensors::ValueType::TEMPERATURE, true, true
          );
          serviceDataPos += 2;
          foundData = true;

          ESP_LOGI(
            L_SENSORS_BLE, "Sensor #%hhu, received temp: %.2f",
            sensorId, temperature
          );
          break;
        }

        // Humidity (2 bytes, little-endian)
        case 0x03: {
          if (serviceDataPos + 2 > serviceData.size()) {
            return foundData;
          }

          uint16_t rawHumidity = (static_cast<uint16_t>(serviceData[serviceDataPos + 1]) << 8)
                                 | static_cast<uint8_t>(serviceData[serviceDataPos]);
          float humidity = static_cast<float>(rawHumidity) * 0.01f;
          Sensors::setValueById(sensorId, humidity, Sensors::ValueType::HUMIDITY, true, true);
          serviceDataPos += 2;
          foundData = true;

          ESP_LOGI(
            L_SENSORS_BLE, "Sensor #%hhu, received humidity: %.2f",
            sensorId, humidity
          );
          break;
        }

        // Voltage (2 bytes, little-endian)
        case 0x0C: {
          if (serviceDataPos + 2 > serviceData.size()) {
            return foundData;
          }

          uint16_t batteryMv = (static_cast<uint16_t>(serviceData[serviceDataPos + 1]) << 8)
                               | static_cast<uint8_t>(serviceData[serviceDataPos]);
          serviceDataPos += 2;
          foundData = true;

          ESP_LOGI(
            L_SENSORS_BLE, "Sensor #%hhu, received battery voltage: %hu",
            sensorId, batteryMv
          );
          break;
        }

        default:
          return foundData;
      }
    }

    return foundData;
  }
};

static BluetoothScanCallbacks bluetoothScanCallbacks;
#endif

static const unsigned int wiredDisconnectTimeout = 180000u;
static const unsigned int wirelessDisconnectTimeout = 600000u;
static const unsigned short dallasSearchInterval = 60000u;
static const unsigned short dallasPollingInterval = 10000u;
static const unsigned short dhtPollingInterval = 15000;

// OneWire
static std::unordered_map<uint8_t, OneWire> owInstances;
static std::unordered_map<uint8_t, DallasTemperature> dallasInstances;
static std::unordered_map<uint8_t, unsigned long> dallasSearchTime;
static std::unordered_map<uint8_t, unsigned long> dallasLastPollingTime;
static std::unordered_map<uint8_t, bool> dallasPolling;

#if USE_BLE
static NimBLEScan* pBLEScan = nullptr;
static bool activeScanBle = false;
#endif

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

static void updateMasterValues() {
  const float previousIndoorTemp = vars.master.heating.indoorTemp;
  vars.master.heating.indoorTemp = Sensors::getMeanValueByPurpose(
    Sensors::Purpose::INDOOR_TEMP,
    Sensors::ValueType::PRIMARY,
    settings.heating.indoorTempAvgType,
    true,
    0.0f
  );

  if (previousIndoorTemp != vars.master.heating.indoorTemp) {
    writeRoomTemperatureTask.requestEarlyRun();
    writeRoomTemperatureCh2Task.requestEarlyRun();
  }

  vars.master.heating.outdoorTemp = Sensors::getMeanValueByPurpose(
    Sensors::Purpose::OUTDOOR_TEMP,
    Sensors::ValueType::PRIMARY,
    settings.heating.outdoorTempAvgType,
    true,
    0.0f
  );

  vars.master.heating.currentTemp = Sensors::getMeanValueByPurpose(
    Sensors::Purpose::HEATING_TEMP,
    Sensors::ValueType::PRIMARY,
    Sensors::AverageType::MEAN,
    true,
    0.0f
  );
  vars.master.heating.returnTemp = Sensors::getMeanValueByPurpose(
    Sensors::Purpose::HEATING_RETURN_TEMP,
    Sensors::ValueType::PRIMARY,
    Sensors::AverageType::MEAN,
    true,
    0.0f
  );

  vars.master.dhw.currentTemp = Sensors::getMeanValueByPurpose(
    Sensors::Purpose::DHW_TEMP,
    Sensors::ValueType::PRIMARY,
    Sensors::AverageType::MEAN,
    true,
    0.0f
  );
  vars.master.dhw.returnTemp = Sensors::getMeanValueByPurpose(
    Sensors::Purpose::DHW_RETURN_TEMP,
    Sensors::ValueType::PRIMARY,
    Sensors::AverageType::MEAN,
    true,
    0.0f
  );
}

static void makeDallasInstances() {
  for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
    const auto sSensor = Sensors::getSettings(sensorId);
    if (!sSensor) {
      continue;
    }

    if (!sSensor->enabled || sSensor->type != Sensors::Type::DALLAS_TEMP || sSensor->purpose == Sensors::Purpose::NOT_CONFIGURED) {
      continue;

    } else if (dallasInstances.count(sSensor->gpio)) {
      // no need to make instances
      continue;
    }

    auto& owInstance = owInstances[sSensor->gpio];
    owInstance.begin(sSensor->gpio);
    owInstance.reset();

    dallasSearchTime[sSensor->gpio] = 0;
    dallasPolling[sSensor->gpio] = false;
    dallasLastPollingTime[sSensor->gpio] = 0;

    auto& instance = dallasInstances[sSensor->gpio];
    instance.setOneWire(&owInstance);
    instance.setWaitForConversion(false);

    ESP_LOGI(L_SENSORS_DALLAS, "Started on GPIO %hhu", sSensor->gpio);
  }
}

static void cleanDallasInstances() {
  // for (auto& [gpio, instance] : dallasInstances) {
  auto it = dallasInstances.begin();
  while (it != dallasInstances.end()) {
    auto gpio = it->first;
    bool instanceUsed = false;

    for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
      const auto sSensor = Sensors::getSettings(sensorId);
      if (!sSensor) {
        continue;
      }

      if (!sSensor->enabled || sSensor->type != Sensors::Type::DALLAS_TEMP || sSensor->purpose == Sensors::Purpose::NOT_CONFIGURED) {
        continue;
      }

      if (sSensor->gpio == gpio) {
        instanceUsed = true;
        break;
      }
    }

    if (!instanceUsed) {
      it = dallasInstances.erase(it);
      owInstances.erase(gpio);
      dallasSearchTime.erase(gpio);
      dallasPolling.erase(gpio);
      dallasLastPollingTime.erase(gpio);

      ESP_LOGI(L_SENSORS_DALLAS, "Stopped on GPIO %hhu", gpio);
      continue;
    }

    it++;
  }
}

static void searchDallasSensors() {
  // search sensors on bus
  for (auto& [gpio, instance] : dallasInstances) {
    // do not search if polling!
    if (dallasPolling[gpio]) {
      continue;
    }

    if (millis() - dallasSearchTime[gpio] < dallasSearchInterval) {
      continue;
    }

    dallasSearchTime[gpio] = millis();
    owInstances[gpio].reset();
    instance.begin();

    ESP_LOGD(
      L_SENSORS_DALLAS,
      "GPIO %hhu, devices on bus: %hhu, DS18* devices: %hhu",
      gpio, instance.getDeviceCount(), instance.getDS18Count()
    );
  }
}

static void fillingAddressesDallasSensors() {
  // check & filling sensors address
  for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
    const auto sSensor = Sensors::getSettings(sensorId);
    if (!sSensor) {
      continue;
    }

    if (!sSensor->enabled || sSensor->type != Sensors::Type::DALLAS_TEMP || sSensor->purpose == Sensors::Purpose::NOT_CONFIGURED) {
      continue;

    } else if (!dallasInstances.count(sSensor->gpio)) {
      continue;
    }

    // do nothing if address not empty
    if (!isEmptyAddress(sSensor->address)) {
      continue;
    }

    // do nothing if polling
    if (dallasPolling[sSensor->gpio]) {
      continue;
    }

    auto& instance = dallasInstances[sSensor->gpio];
    DeviceAddress devAddr;
    for (uint8_t devId = 0; devId < instance.getDeviceCount(); devId++) {
      if (!instance.getAddress(devAddr, devId)) {
        continue;
      }

      bool freeAddress = true;

      // checking address usage
      for (uint8_t checkingSensorId = 0; checkingSensorId <= Sensors::getMaxSensorId(); checkingSensorId++) {
        const auto sCheckingSensor = Sensors::getSettings(checkingSensorId);
        if (!sCheckingSensor) {
          continue;
        }
        if (sCheckingSensor->type != Sensors::Type::DALLAS_TEMP || checkingSensorId == sensorId) {
          continue;
        }

        if (sCheckingSensor->gpio != sSensor->gpio || isEmptyAddress(sCheckingSensor->address)) {
          continue;
        }

        if (isEqualAddress(sCheckingSensor->address, devAddr)) {
          freeAddress = false;
          break;
        }
      }

      // address already in use
      if (!freeAddress) {
        continue;
      }

      const bool addressSet = Sensors::setAddressById(sensorId, devAddr, [sensorId] {
        requestReconfigureMqttSensor(sensorId);
        requestSaveSensorsSettings();
      });
      if (!addressSet) {
        break;
      }
      ESP_LOGD(
        L_SENSORS_DALLAS, "GPIO %hhu, sensor #%hhu '%s', set address: %hhX:%hhX:%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
        sSensor->gpio, sensorId, sSensor->name,
        devAddr[0], devAddr[1], devAddr[2], devAddr[3],
        devAddr[4], devAddr[5], devAddr[6], devAddr[7]
      );

      break;
    }
  }
}

static bool isPollingDallasSensors() {
  for (auto& [gpio, instance] : dallasInstances) {
    if (dallasPolling.count(gpio) && dallasPolling[gpio]) {
      return true;
    }
  }

  return false;
}

static void pollingDallasSensors(bool newPolling = true) {
  for (auto& [gpio, instance] : dallasInstances) {
    unsigned long ts = millis();

    if (dallasPolling[gpio]) {
      unsigned long minPollingTime = instance.millisToWaitForConversion(12) * 2;
      unsigned long estimatePollingTime = ts - dallasLastPollingTime[gpio];

      // check conversion time
      // isConversionComplete does not work with chinese clones!
      if (estimatePollingTime < minPollingTime) {
        continue;
      }

      // read sensors data for current instance
      for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
        const auto sSensor = Sensors::getSettings(sensorId);
        if (!sSensor) {
          continue;
        }

        // only target & valid sensors
        if (!sSensor->enabled || sSensor->type != Sensors::Type::DALLAS_TEMP || sSensor->purpose == Sensors::Purpose::NOT_CONFIGURED) {
          continue;

        } else if (sSensor->gpio != gpio || isEmptyAddress(sSensor->address)) {
          continue;
        }

        float value = instance.getTempC(sSensor->address);
        if (value == DEVICE_DISCONNECTED_C) {
          ESP_LOGW(
            L_SENSORS_DALLAS, "GPIO %hhu, sensor #%hhu '%s': failed receiving data",
            sSensor->gpio, sensorId, sSensor->name
          );

          Sensors::adjustSignalQuality(sensorId, -1);

          continue;
        }

        ESP_LOGD(
          L_SENSORS_DALLAS, "GPIO %hhu, sensor #%hhu '%s', received data: %.2f",
          sSensor->gpio, sensorId, sSensor->name, value
        );

        Sensors::adjustSignalQuality(sensorId, 1);

        // set sensor value
        Sensors::setValueById(
          sensorId, value,
          Sensors::ValueType::TEMPERATURE, true, true
        );
      }

      // reset polling flag
      dallasPolling[gpio] = false;

    } else if (newPolling) {
      auto estimateLastPollingTime = ts - dallasLastPollingTime[gpio];

      // check last polling time
      if (estimateLastPollingTime < dallasPollingInterval) {
        continue;
      }

      // start polling
      instance.setResolution(12);
      instance.requestTemperatures();
      dallasPolling[gpio] = true;
      dallasLastPollingTime[gpio] = ts;

      ESP_LOGD(L_SENSORS_DALLAS, "GPIO %hhu, polling...", gpio);
    }
  }
}

#if USE_BLE
static void scanBleSensors() {
  if (!Sensors::getAmountByType(Sensors::Type::BLUETOOTH, true)) {
    if (NimBLEDevice::isInitialized()) {
      if (pBLEScan != nullptr) {
        if (pBLEScan->isScanning()) {
          pBLEScan->stop();

        } else {
          pBLEScan = nullptr;
        }
      }

      if (pBLEScan == nullptr) {
        if (NimBLEDevice::deinit(true)) {
          ESP_LOGI(L_SENSORS_BLE, "Deinitialized");

        } else {
          ESP_LOGW(L_SENSORS_BLE, "Unable to deinitialize!");
        }
      }
    }

    return;
  }

  if (!NimBLEDevice::isInitialized() && millis() > 5000) {
    if (NimBLEDevice::init(PROJECT_NAME)) {
      ESP_LOGI(L_SENSORS_BLE, "Initialized");

      #ifdef ESP_PWR_LVL_P20
      NimBLEDevice::setPower(ESP_PWR_LVL_P20);
      #elifdef ESP_PWR_LVL_P9
      NimBLEDevice::setPower(ESP_PWR_LVL_P9);
      #endif

    } else {
      ESP_LOGW(L_SENSORS_BLE, "Unable to initialize!");
      return;
    }
  }

  if (pBLEScan == nullptr) {
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setCallbacks(bluetoothScanCallbacks);
    #if MYNEWT_VAL(BLE_EXT_ADV)
    pBLEScan->setPhy(NimBLEScan::Phy::SCAN_ALL);
    #endif
    pBLEScan->setDuplicateFilter(false);
    pBLEScan->setMaxResults(0);
    pBLEScan->setInterval(10000);
    pBLEScan->setWindow(10000);

    ESP_LOGI(L_SENSORS_BLE, "Scanning initialized");
  }

  if (!pBLEScan->isScanning()) {
    activeScanBle = !activeScanBle;
    pBLEScan->setActiveScan(activeScanBle);

    if (pBLEScan->start(30000, false, false)) {
      ESP_LOGI(
        L_SENSORS_BLE,
        "%s scanning started",
        activeScanBle ? "Active" : "Passive"
      );

    } else {
      ESP_LOGI(L_SENSORS_BLE, "Unable to start scanning");
    }
  }
}
#endif

static void updateConnectionStatus() {
  for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
    Sensors::updateConnectionStatusById(sensorId, wiredDisconnectTimeout, wirelessDisconnectTimeout);
  }
}

static bool canRunSensors() {
  return !vars.states.upgrading && !restartTask.scheduled() && !restartTask.active();
}

static Task readDallasSensorsTask("Read Dallas sensors", [](void*) {
  pollingDallasSensors(false);
});

static Task pollDhtSensorsTask("Poll DHT sensors", [](void*) {
  static DHT instance;
  static uint8_t lastPollingSensorId{0};
  static std::unordered_map<uint8_t, unsigned long> lastPollingTime(2);

  if (instance.isPolling()) {
    return;
  }

  for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
    const auto sSensor = Sensors::getSettings(sensorId);
    if (!sSensor) {
      continue;
    }

    if (!sSensor->enabled || sSensor->purpose == Sensors::Purpose::NOT_CONFIGURED) {
      continue;
    }

    if (sSensor->type != Sensors::Type::DHT11 && sSensor->type != Sensors::Type::DHT22) {
      continue;
    }

    if (lastPollingTime.count(sSensor->gpio) && millis() - lastPollingTime[sSensor->gpio] < dhtPollingInterval) {
      continue;
    }

    const auto sensorGpio = static_cast<gpio_num_t>(sSensor->gpio);
    DHT::Type sensorType = DHT::Type::DHT22;
    if (sSensor->type == Sensors::Type::DHT11) {
      sensorType = DHT::Type::DHT11;

    } else if (sSensor->type == Sensors::Type::DHT22) {
      sensorType = DHT::Type::DHT22;
    }

    if (lastPollingSensorId != sensorId || instance.getGpio() != sensorGpio || instance.getType() != sensorType) {
      instance.reset();
      instance.onData([sensorId, gpio = sSensor->gpio](float humidity, float temperature) {
        const auto sSensor = Sensors::getSettings(sensorId);
        if (!sSensor) {
          lastPollingTime[gpio] = millis();
          return;
        }

        ESP_LOGD(
          L_SENSORS_DHT, "GPIO %hhu, sensor #%hhu '%s', temp: %.2f, humidity: %.2f%%",
          sSensor->gpio, sensorId, sSensor->name, temperature, humidity
        );

        // set temperature
        Sensors::setValueById(
          sensorId, temperature,
          Sensors::ValueType::TEMPERATURE, true, true
        );

        // set humidity
        Sensors::setValueById(sensorId, humidity, Sensors::ValueType::HUMIDITY, true, true);

        Sensors::adjustSignalQuality(sensorId, 1);
        lastPollingTime[gpio] = millis();
      });

      instance.onError([sensorId, gpio = sSensor->gpio](DHT::Status status) {
        const auto sSensor = Sensors::getSettings(sensorId);
        if (!sSensor) {
          lastPollingTime[gpio] = millis();
          return;
        }

        ESP_LOGW(
          L_SENSORS_DHT, "GPIO %hhu, sensor #%hhu '%s': failed receiving data (err: %s)",
          sSensor->gpio, sensorId, sSensor->name, DHT::statusToString(instance.getStatus())
        );

        Sensors::adjustSignalQuality(sensorId, -1);
        lastPollingTime[gpio] = millis();
      });

      if (instance.setup(sensorGpio, sensorType)) {
        ESP_LOGI(L_SENSORS_DHT, "Started on GPIO %hhu", sSensor->gpio);

      } else {
        ESP_LOGW(
          L_SENSORS_DHT, "Failed to start on GPIO %hhu (err: %s)",
          sSensor->gpio, DHT::statusToString(instance.getStatus())
        );
      }
    }

    lastPollingSensorId = sensorId;
    if (!instance.poll()) {
      lastPollingTime[sSensor->gpio] = millis();
    }

    break;
  }
});

static Task maintainDallasSensorsTask("Maintain Dallas sensors", [](void*) {
  cleanDallasInstances();
  makeDallasInstances();
  searchDallasSensors();
  fillingAddressesDallasSensors();
  pollingDallasSensors();
});

static Task readNtcSensorsTask("Read NTC sensors", [](void*) {
  for (uint8_t sensorId = 0; sensorId <= Sensors::getMaxSensorId(); sensorId++) {
    const auto sSensor = Sensors::getSettings(sensorId);
    if (!sSensor) {
      continue;
    }

    if (!sSensor->enabled || sSensor->type != Sensors::Type::NTC_10K_TEMP || sSensor->purpose == Sensors::Purpose::NOT_CONFIGURED) {
      continue;
    }

    const auto value = analogReadMilliVolts(sSensor->gpio);
    if (value < DEFAULT_NTC_VLOW_THRESHOLD || value > DEFAULT_NTC_VHIGH_THRESHOLD) {
      Sensors::setConnectionStatusById(sensorId, false, false);

      ESP_LOGW(
        L_SENSORS_NTC, "GPIO %hhu, sensor #%hhu '%s', voltage is out of threshold: %.3f",
        sSensor->gpio, sensorId, sSensor->name, (value / 1000.0f)
      );

      continue;
    }

    const float sensorResistance = value > 1
      ? DEFAULT_NTC_REF_RESISTANCE / (DEFAULT_NTC_VREF / static_cast<float>(value) - 1.0f)
      : 0.0f;
    const float rawTemp = 1.0f / (
      1.0f / (DEFAULT_NTC_NOMINAL_TEMP + 273.15f) +
      log(sensorResistance / DEFAULT_NTC_NOMINAL_RESISTANCE) / DEFAULT_NTC_BETA_FACTOR
    ) - 273.15f;

    ESP_LOGD(
      L_SENSORS_NTC, "GPIO %hhu, sensor #%hhu '%s', raw temp: %.2f, raw voltage: %.3f, raw resistance: %.2f",
      sSensor->gpio, sensorId, sSensor->name, rawTemp, (value / 1000.0f), sensorResistance
    );

    // set temp
    Sensors::setValueById(
      sensorId, rawTemp,
      Sensors::ValueType::TEMPERATURE, true, true
    );
  }
});

#if USE_BLE
static Task scanBleSensorsTask("Scan BLE sensors", [](void*) {
  scanBleSensors();
});
#endif

static Task updateSensorsTask("Update sensor states", [](void*) {
  updateConnectionStatus();
  updateMasterValues();
});

void initSensors() {
  dallasLastPollingTime.reserve(2);
  owInstances.reserve(2);
  dallasInstances.reserve(2);
  dallasSearchTime.reserve(2);
  dallasPolling.reserve(2);

  readDallasSensorsTask.setInterval(1000)
    .setEnabledWhen([] {
      return canRunSensors() && isPollingDallasSensors();
    });
  sensorsTaskManager.addTask(readDallasSensorsTask);

  pollDhtSensorsTask.setInterval(1000)
    .setEnabledWhen([] {
      return canRunSensors() && (
        Sensors::getAmountByType(Sensors::Type::DHT11, true) ||
        Sensors::getAmountByType(Sensors::Type::DHT22, true)
      );
    });
  sensorsTaskManager.addTask(pollDhtSensorsTask);

  maintainDallasSensorsTask.setEnabledWhen([] {
      return canRunSensors() && (
        !dallasInstances.empty() ||
        Sensors::getAmountByType(Sensors::Type::DALLAS_TEMP, true)
      );
    })
    .resume(15000);
  sensorsTaskManager.addTask(maintainDallasSensorsTask);

  readNtcSensorsTask.setEnabledWhen([] {
      return canRunSensors() && Sensors::getAmountByType(Sensors::Type::NTC_10K_TEMP, true);
    })
    .resume(15000);
  sensorsTaskManager.addTask(readNtcSensorsTask);

  #if USE_BLE
  scanBleSensorsTask.setEnabledWhen([] {
      return canRunSensors() && (
        NimBLEDevice::isInitialized() ||
        Sensors::getAmountByType(Sensors::Type::BLUETOOTH, true)
      );
    })
    .resume(15000);
  sensorsTaskManager.addTask(scanBleSensorsTask);
  #endif

  updateSensorsTask.setInterval(1000)
    .setEnabledWhen(canRunSensors);
  sensorsTaskManager.addTask(updateSensorsTask);
}
