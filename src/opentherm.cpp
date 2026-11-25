#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>

#include <ESP32OT.h>
#include <atomic>

// Application state and ID polling belong to this manager; the library owns RMT.

static const unsigned short masterReadyTime = 60000u;
static const unsigned int resetBusInterval = 120000u;
static const unsigned short heatingSetpointWriteInterval = 60000u;
static const unsigned short roomSetpointWriteInterval = 60000u;
static const unsigned short dhwSetpointWriteInterval = 60000u;
static const unsigned short ch2SetpointWriteInterval = 60000u;
static const unsigned int initializingInterval = 1800000u;
static const char responseMismatchMessage[] = "Response mismatch: expected %04x, received %04x";
static const char unrepresentableTemperatureLimitsMessage[] = "Received temperature limits cannot be represented in system units";

static ESP32OT::ESP32OT slave;
static unsigned long busConfiguredTime = 0;
static unsigned long connectedTime = 0;
static unsigned long disconnectedTime = 0;
static unsigned long resetBusTime = 0;
static unsigned long initializedTime = 0;
static unsigned long lastSuccessStatusResponse = 0;
static unsigned long heatingSetpointWriteTime = 0;
static unsigned long dhwSetpointWriteTime = 0;
static unsigned long ch2SetpointWriteTime = 0;
static unsigned long roomSetpointWriteTime = 0;
static unsigned long ch2RoomSetpointWriteTime = 0;
static std::atomic<uint8_t> configuredRxLedGpio{GPIO_IS_NOT_CONFIGURED};


static bool canUseBus() {
  return !restartTask.scheduled() && slave.isInitialized() &&
         slave.getConfig().rxGpio == settings.opentherm.inGpio &&
         slave.getConfig().txGpio == settings.opentherm.outGpio;
}

static bool isMasterReady() {
  return millis() - busConfiguredTime > masterReadyTime;
}

static bool getSlaveChannelEnabled(const bool enabled, const ChannelDisablingAction action);
static float getSlaveChannelSetpoint(const bool enabled, const float temperature, const ChannelDisablingAction action);
static bool needWriteRoomSetpoint(const float target);
static bool needWriteRoomSetpointCh2(const float target);
static bool needWriteDhwSetpoint(const float target);
static bool needWriteHeatingSetpoint(const float target);
static bool needWriteCh2Setpoint(const float target);

static bool getSlaveChannelEnabled(const bool enabled, const ChannelDisablingAction action) {
  return enabled || action == ChannelDisablingAction::ZERO_SETPOINT;
}

static float getSlaveChannelSetpoint(const bool enabled, const float temperature, const ChannelDisablingAction action) {
  if (!enabled && action != ChannelDisablingAction::OFF) {
    return 0.0f;
  }

  return temperature;
}

static float getHeatingChannelSetpoint() {
  return getSlaveChannelSetpoint(
    vars.master.heating.enabled,
    settings.opentherm.options.nativeOTC
      ? vars.master.heating.targetTemp
      : vars.master.heating.setpointTemp,
    settings.opentherm.heatingDisablingAction
  );
}

static float getDhwChannelSetpoint() {
  return getSlaveChannelSetpoint(
    vars.master.dhw.enabled, vars.master.dhw.targetTemp, settings.opentherm.dhwDisablingAction
  );
}

static float getCh2ChannelSetpoint() {
  return getSlaveChannelSetpoint(
    vars.master.ch2.enabled, vars.master.ch2.targetTemp, settings.opentherm.ch2DisablingAction
  );
}

static bool needWriteRoomSetpoint(const float target) {
  return !roomSetpointWriteTime ||
         millis() - roomSetpointWriteTime > roomSetpointWriteInterval ||
         isFloatDifferent(target, vars.slave.heating.roomTargetTemp, 0.05f);
}

static bool needWriteRoomSetpointCh2(const float target) {
  return !ch2RoomSetpointWriteTime ||
         millis() - ch2RoomSetpointWriteTime > roomSetpointWriteInterval ||
         isFloatDifferent(target, vars.slave.ch2.roomTargetTemp, 0.05f);
}

static bool needWriteDhwSetpoint(const float target) {
  return !dhwSetpointWriteTime ||
         millis() - dhwSetpointWriteTime > dhwSetpointWriteInterval ||
         isFloatDifferent(target, vars.slave.dhw.targetTemp, 0.05f);
}

static bool needWriteHeatingSetpoint(const float target) {
  return !heatingSetpointWriteTime ||
         millis() - heatingSetpointWriteTime > heatingSetpointWriteInterval ||
         isFloatDifferent(target, vars.slave.heating.targetTemp, 0.05f);
}

static bool needWriteCh2Setpoint(const float target) {
  return !ch2SetpointWriteTime ||
         millis() - ch2SetpointWriteTime > ch2SetpointWriteInterval ||
         isFloatDifferent(target, vars.slave.ch2.targetTemp, 0.05f);
}

// ONCE identification steps keep their own callbacks and completion handling.
static Task readSlaveVersionTask("Read slave version", Task::Type::ONCE, [](void*) {
  const auto response = slave.sendRequest(
    ESP32OT::Messages::ProductVersion(ESP32OT::MessageID::SlaveVersion)
  );

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive slave version");
    return;
  }

  vars.slave.appVersion = response.getVersion();
  vars.slave.type = response.getProductType();

  ESP_LOGI(
    L_OT, "Received slave app version: %u, type: %u",
    vars.slave.appVersion, vars.slave.type
  );
});

static Task writeMasterVersionTask("Write master version", Task::Type::ONCE, [](void*) {
  const ESP32OT::Messages::ProductVersion request(
    ESP32OT::MessageID::MasterVersion,
    vars.master.type,
    vars.master.appVersion
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess() || response.getVersion() != request.getVersion() || response.getProductType() != request.getProductType()) {
    ESP_LOGW(L_OT, "Failed set master version");
    return;
  }

  ESP_LOGI(
    L_OT, "Set master version: %u, type: %u",
    vars.master.appVersion, vars.master.type
  );
});

static Task readSlaveProtocolTask("Read slave protocol", Task::Type::ONCE, [](void*) {
  const auto response = slave.sendRequest(
    ESP32OT::Messages::Float(ESP32OT::MessageID::OpenThermVersionSlave)
  );

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive slave OT version");
    return;
  }

  vars.slave.protocolVersion = response.get();

  ESP_LOGI(L_OT, "Received slave OT version: %f", vars.slave.protocolVersion);
});

static Task writeMasterProtocolTask("Write master protocol", Task::Type::ONCE, [](void*) {
  const ESP32OT::Messages::Float request(
    ESP32OT::MessageID::OpenThermVersionMaster,
    vars.master.protocolVersion
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess() || response.getData() != request.getData()) {
    ESP_LOGW(L_OT, "Failed set master OT version");
    return;
  }

  ESP_LOGI(L_OT, "Set master OT version: %f", vars.master.protocolVersion);
});

static Task readSlaveConfigTask("Read slave config", Task::Type::ONCE, [](void*) {
  const auto response = slave.sendRequest(ESP32OT::Messages::SlaveConfig());

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive slave config");
    return;
  }

  vars.slave.memberId = response.getMemberId();
  vars.slave.flags = response.getHighByte();

  ESP_LOGI(
    L_OT, "Received slave member id: %u, flags: %u",
    vars.slave.memberId, vars.slave.flags
  );
});

static Task writeMasterConfigTask("Write master config", Task::Type::ONCE, [](void*) {
  const uint8_t id = vars.master.memberId;
  const uint8_t flags = vars.master.flags;
  const uint8_t memberId = id > 0 ? id : vars.slave.memberId;
  const uint8_t configFlags = flags > 0 ? flags : vars.slave.flags;
  const ESP32OT::Messages::MasterConfig request(configFlags, memberId);
  if (request.getData()) {
    const auto response = slave.sendRequest(request);

    if (!response.isSuccess() || response.getData() != request.getData()) {
      ESP_LOGW(L_OT, "Failed set master config");
      return;
    }
  }

  ESP_LOGI(
    L_OT, "Set master member id: %u, flags: %u",
    vars.master.memberId, vars.master.flags
  );
});

// Keep initialization scheduled until completion; ONCE pauses before its callback.
static Task initializeSlaveTask("Initialize slave", [](void*) {
  ESP_LOGI(L_OT, "Initializing...");

  readSlaveVersionTask.resume().tryRun();
  writeMasterVersionTask.resume().tryRun();
  readSlaveProtocolTask.resume().tryRun();
  writeMasterProtocolTask.resume().tryRun();
  readSlaveConfigTask.resume().tryRun();
  writeMasterConfigTask.resume().tryRun();

  if (!canUseBus() || !vars.slave.connected) {
    return;
  }

  initializeSlaveTask.pause();
  initializedTime = millis();
});

static void onDisconnectSlave() {
  if (vars.slave.connected) {
    ESP_LOGW(
      L_OT,
      "Disconnected, uptime: %lu s.",
      (millis() - connectedTime) / 1000
    );

    // Mark sensors as disconnected
    Sensors::setConnectionStatusByType(Sensors::Type::OT_OUTDOOR_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_RETURN_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_TEMP2, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_FLOW_RATE, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_CH2_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_EXHAUST_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_HEAT_EXCHANGER_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_PRESSURE, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_MODULATION_LEVEL, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_CURRENT_POWER, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_EXHAUST_CO2, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_EXHAUST_FAN_SPEED, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_SUPPLY_FAN_SPEED, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_SOLAR_STORAGE_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_SOLAR_COLLECTOR_TEMP, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_FAN_SPEED_SETPOINT, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_FAN_SPEED_CURRENT, false);

    Sensors::setConnectionStatusByType(Sensors::Type::OT_BURNER_STARTS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_BURNER_STARTS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_PUMP_STARTS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_PUMP_STARTS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_BURNER_HOURS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_BURNER_HOURS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_HEATING_PUMP_HOURS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_DHW_PUMP_HOURS, false);
    Sensors::setConnectionStatusByType(Sensors::Type::OT_COOLING_HOURS, false);

    initializeSlaveTask.resume();
    disconnectedTime = millis();
    vars.slave.connected = false;
  }

  vars.slave.heating.enabled = false;
  vars.slave.heating.active = false;
  vars.slave.dhw.enabled = false;
  vars.slave.dhw.active = false;
  vars.slave.ch2.active = false;
  vars.slave.flame = false;
  vars.slave.cooling.active = false;
  vars.slave.cooling.setpoint = 0;
  vars.slave.fault.active = false;
  vars.slave.fault.code = 0;
  vars.slave.diag.active = false;
  vars.slave.diag.code = 0;
}

static bool isSlaveReady() {
  return canUseBus() && vars.slave.connected && !initializeSlaveTask.scheduled();
}

static bool isInitializingSlave() {
  return canUseBus() && vars.slave.connected && initializeSlaveTask.running();
}


static void updateMasterState() {
  // Heating settings
  vars.master.heating.enabled =
      isMasterReady() &&
      settings.heating.enabled &&
      (vars.master.heating.freezing || (vars.cascadeControl.input && !vars.master.heating.blocking)) &&
      !vars.master.heating.overheat;

  // DHW settings
  vars.master.dhw.enabled =
      isMasterReady() &&
      settings.opentherm.options.dhwSupport &&
      settings.dhw.enabled &&
      !vars.master.dhw.overheat;
  vars.master.dhw.targetTemp = settings.dhw.target;

  // CH2 settings
  vars.master.ch2.enabled =
      settings.opentherm.options.ch2AlwaysEnabled ||
      (settings.opentherm.options.heatingToCh2 && vars.master.heating.enabled) ||
      (settings.opentherm.options.dhwToCh2 && vars.master.dhw.enabled);

  if (settings.opentherm.options.heatingToCh2) {
    vars.master.ch2.targetTemp = !settings.opentherm.options.nativeOTC
      ? vars.master.heating.setpointTemp
      : vars.master.heating.targetTemp;

  } else if (settings.opentherm.options.dhwToCh2) {
    vars.master.ch2.targetTemp = vars.master.dhw.targetTemp;
  }

  if (isSlaveReady()) {
    if (vars.master.heating.enabled != vars.slave.heating.enabled) {
      vars.slave.heating.enabled = vars.master.heating.enabled;
      ESP_LOGI(L_OT_HEATING, vars.master.heating.enabled ? "Enabled" : "Disabled");
    }

    if (vars.master.dhw.enabled != vars.slave.dhw.enabled) {
      vars.slave.dhw.enabled = vars.master.dhw.enabled;
      ESP_LOGI(L_OT_DHW, vars.master.dhw.enabled ? "Enabled" : "Disabled");
    }
  }
}

static Task configureDriverTask("Configure OpenTherm driver", Task::Type::ONCE, [](void*) {
  // Stop the current bus before changing GPIO.
  if (!slave.end()) {
    ESP_LOGW(L_OT, "Failed stop");
    return;
  }

  onDisconnectSlave();

  if (!GPIO_IS_VALID_INPUT(settings.opentherm.inGpio) || !GPIO_IS_VALID_OUTPUT(settings.opentherm.outGpio)) {
    ESP_LOGW(
      L_OT,
      "Not started. GPIO IN: %hhu or GPIO OUT: %hhu is not valid",
      settings.opentherm.inGpio,
      settings.opentherm.outGpio
    );
    return;
  }

  #ifdef OT_BYPASS_RELAY_GPIO
  pinMode(OT_BYPASS_RELAY_GPIO, OUTPUT);
  digitalWrite(OT_BYPASS_RELAY_GPIO, true);
  #endif

  // Configure the existing driver.
  ESP32OT::ESP32OT::Config config;
  config.rxGpio = static_cast<gpio_num_t>(settings.opentherm.inGpio);
  config.txGpio = static_cast<gpio_num_t>(settings.opentherm.outGpio);

  // Log formatting stays on the RMT worker; transport output runs on System.
  config.stackSize = 4096;
  if (!slave.setConfig(config)) {
    ESP_LOGW(L_OT, "Failed configure");
    return;
  }

  // flags
  busConfiguredTime = millis();
  resetBusTime = millis();
  initializeSlaveTask.resume();
  heatingSetpointWriteTime = 0;
  dhwSetpointWriteTime = 0;
  ch2SetpointWriteTime = 0;
  roomSetpointWriteTime = 0;
  ch2RoomSetpointWriteTime = 0;

  ESP_LOGI(L_OT, "Started. GPIO IN: %hhu, GPIO OUT: %hhu", settings.opentherm.inGpio, settings.opentherm.outGpio);

  slave.onStatusChange([](const ESP32OT::ESP32OT::StatusEvent& event) {
    const auto& response = event.message;
    if (!response.isComplete()) {
      return;
    }

    ESP_LOGD(
      L_OT,
      "ID: %3d; Request: %8lx; Response: %8lx; Msg type: %s; Attempt: %2d; Status: %s",
      static_cast<int>(ESP32OT::Protocol::Frame::getId(response.getRequest())),
      static_cast<unsigned long>(response.getRequest()),
      static_cast<unsigned long>(response.hasFrame() ? response.getRaw() : 0),
      response.hasFrame() ? ESP32OT::Protocol::Frame::typeToString(response.getType()) : "NONE",
      response.getAttempt(),
      ESP32OT::Protocol::Frame::statusToString(response.getStatus())
    );

    if (response.isSuccess()) {
      // The manager may reconfigure the LED after sendRequest() returns.
      const uint8_t rxLedGpio = configuredRxLedGpio.load();
      if (rxLedGpio != GPIO_IS_NOT_CONFIGURED) {
        digitalWrite(rxLedGpio, HIGH);
        delayMicroseconds(2000);
        digitalWrite(rxLedGpio, LOW);
      }
    }
  });
});

static Task maintainOpenThermTask("Maintain OpenTherm", [](void*) {
  if (slave.getConfig().rxGpio != settings.opentherm.inGpio || slave.getConfig().txGpio != settings.opentherm.outGpio) {
    // Apply changed GPIO before any bus exchange in this manager pass.
    configureDriverTask.resume().tryRun();

  } else if (vars.master.memberId != settings.opentherm.memberId || vars.master.flags != settings.opentherm.flags) {
    initializeSlaveTask.resume();
    vars.master.memberId = settings.opentherm.memberId;
    vars.master.flags = settings.opentherm.flags;

  } else if (millis() - initializedTime > initializingInterval) {
    initializeSlaveTask.resume();
  }

  // Apply channel settings even while the bus is unavailable.
  updateMasterState();

  if (slave.getConfig().rxGpio != settings.opentherm.inGpio || slave.getConfig().txGpio != settings.opentherm.outGpio) {
    maintainOpenThermTask.setInterval(5000);
    return;

  } else if (!slave.isInitialized()) {
    // A fatal error can leave the RMT worker alive until end() succeeds.
    onDisconnectSlave();

    if (!slave.end() || !slave.begin()) {
      ESP_LOGW(L_OT, "Failed begin");

      maintainOpenThermTask.setInterval(5000);
      return;
    }
  }

  // Retry failures after 5 seconds; otherwise check settings on each pass.
  maintainOpenThermTask.setInterval(0);

  // RX LED GPIO setup
  const uint8_t rxLedGpio = configuredRxLedGpio.load();
  if (settings.opentherm.rxLedGpio != rxLedGpio) {
    configuredRxLedGpio.store(GPIO_IS_NOT_CONFIGURED);
    if (rxLedGpio != GPIO_IS_NOT_CONFIGURED) {
      digitalWrite(rxLedGpio, LOW);
    }

    if (GPIO_IS_VALID_OUTPUT(settings.opentherm.rxLedGpio)) {
      pinMode(settings.opentherm.rxLedGpio, OUTPUT);
      digitalWrite(settings.opentherm.rxLedGpio, LOW);
      configuredRxLedGpio.store(settings.opentherm.rxLedGpio);
    }
  }
});

static Task readStatusTask("Read boiler status", [](void*) {
  updateMasterState();

  // Set boiler status LB
  // Some boilers require this, although this is against protocol
  uint8_t statusLb = 0;

  // Immergas fix
  // https://arduino.ru/forum/programmirovanie/termostat-opentherm-na-esp8266?page=15#comment-649392
  if (settings.opentherm.options.immergasFix) {
    statusLb = 0xCA;
  }

  // Mode flags follow the channel request, before applying its disabling action.

  // Summer/winter mode
  bool summerWinterMode = settings.opentherm.options.summerWinterMode;
  if (settings.opentherm.options.heatingStateToSummerWinterMode) {
    summerWinterMode = vars.master.heating.enabled == summerWinterMode;
  }

  // DHW blocking
  bool dhwBlocking = settings.opentherm.options.dhwBlocking;
  if (settings.opentherm.options.dhwStateAsDhwBlocking) {
    dhwBlocking = vars.master.dhw.enabled == dhwBlocking;
  }

  ESP32OT::Messages::Status request(0, statusLb);
  request
    .setCentralHeatingEnabled(getSlaveChannelEnabled(
      vars.master.heating.enabled, settings.opentherm.heatingDisablingAction
    ))
    .setHotWaterEnabled(
      settings.opentherm.options.dhwSupport
      && getSlaveChannelEnabled(vars.master.dhw.enabled, settings.opentherm.dhwDisablingAction)
    )
    .setCoolingEnabled(settings.opentherm.options.coolingSupport)
    .setOutsideTemperatureCompensationEnabled(settings.opentherm.options.nativeOTC)
    .setCh2Enabled(getSlaveChannelEnabled(
      vars.master.ch2.enabled, settings.opentherm.ch2DisablingAction
    ))
    .setSummerWinterMode(summerWinterMode)
    .setDhwBlocking(dhwBlocking);

  const auto response = slave.sendRequest(request);
  const bool validStatus = response.isSuccess();
  if (!validStatus) {
    ESP_LOGW(
      L_OT,
      "Failed receive boiler status: %s",
      ESP32OT::Protocol::Frame::statusToString(response.getStatus())
    );

  } else {
    lastSuccessStatusResponse = millis();
    vars.slave.heating.active = response.isCentralHeatingActive();
    vars.slave.dhw.active = settings.opentherm.options.dhwSupport ? response.isHotWaterActive() : false;
    vars.slave.flame = response.isFlameOn();
    vars.slave.cooling.active = response.isCoolingActive();
    vars.slave.ch2.active = response.isCh2Active();
    vars.slave.fault.active = response.isFault();

    if (!settings.opentherm.options.ignoreDiagState) {
      vars.slave.diag.active = response.isDiagnostic();

    } else if (vars.slave.diag.active) {
      vars.slave.diag.active = false;
    }

    if (!vars.slave.fault.active && vars.slave.fault.code != 0) {
      vars.slave.fault.code = 0;
    }

    if (!vars.slave.diag.active && vars.slave.diag.code != 0) {
      vars.slave.diag.code = 0;
    }

    ESP_LOGI(
      L_OT, "Received boiler status. Heating: %hhu; DHW: %hhu; flame: %hhu; cooling: %hhu; channel 2: %hhu; fault: %hhu; diag: %hhu",
      vars.slave.heating.active, vars.slave.dhw.active,
      vars.slave.flame, vars.slave.cooling.active, vars.slave.ch2.active, vars.slave.fault.active, vars.slave.diag.active
    );
  }

  if (!vars.slave.connected && validStatus) {
    ESP_LOGI(
      L_OT,
      "Connected, downtime: %lu s.",
      (millis() - disconnectedTime) / 1000
    );

    connectedTime = millis();
    vars.slave.connected = true;
  }

  // 5 request retries
  // 1000ms maximum response waiting time
  // 100ms delay between requests
  // +15%
  // 5 * (1000 + 100) * 1.15 = 6325 ms
  if (vars.slave.connected && !validStatus && millis() - lastSuccessStatusResponse > 6325) {
    onDisconnectSlave();
  }

  // If boiler is disconnected, no need try setting other OT stuff
  if (!vars.slave.connected) {
    // reset bus
    if (millis() - disconnectedTime > resetBusInterval) {
      if (millis() - resetBusTime > resetBusInterval) {
        ESP_LOGI(L_OT, "Reset bus...");

        if (!slave.end()) {
          ESP_LOGW(L_OT, "Failed stop");
        }

        resetBusTime = millis();
        ::delay(5000);
      }
    }

  } else {
    std::unique_lock<std::mutex> mqttLock(mqttMutex, std::try_to_lock);
    if (mqttLock.owns_lock() && mqttPublishStateTask) {
      mqttPublishStateTask->requestEarlyRun();
    }
  }
});

static Task writeHeatingSetpointTask("Write heating setpoint", [](void*) {
  // Target heating temp
  const float targetTemp = getHeatingChannelSetpoint();

  // Set max heating temp
  if (settings.opentherm.options.maxTempSyncWithTargetTemp) {
    const auto request = ESP32OT::Messages::Float::writeTemperature(
      ESP32OT::MessageID::MaxTSet,
      targetTemp
    );
    const auto response = slave.sendRequest(request);

    if (response.isSuccess() && response.getData() == request.getData()) {
      ESP_LOGI(
        L_OT_HEATING, "Set max heating temp: %.2f",
        targetTemp
      );

    } else {
      ESP_LOGW(
        L_OT_HEATING, "Failed set max heating temp: %.2f",
        targetTemp
      );
    }
  }

  // Set target heating temp
  const auto request = ESP32OT::Messages::Float::writeTemperature(
    ESP32OT::MessageID::TSet,
    targetTemp
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_HEATING, "Failed set target temp");
    return;
  }

  vars.slave.heating.targetTemp = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT_HEATING, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  heatingSetpointWriteTime = millis();

  ESP_LOGI(
    L_OT_HEATING, "Set target temp: %.2f (response: %.2f)",
    targetTemp, vars.slave.heating.targetTemp
  );
});

static Task writeDhwSetpointTask("Write DHW setpoint", [](void*) {
  // Target dhw temp
  const float targetTemp = getDhwChannelSetpoint();

  // Set DHW temp
  const auto request = ESP32OT::Messages::Float::writeTemperature(
    ESP32OT::MessageID::TdhwSet,
    targetTemp
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_DHW, "Failed set temp");
    return;
  }

  vars.slave.dhw.targetTemp = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT_DHW, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  dhwSetpointWriteTime = millis();

  ESP_LOGI(
    L_OT_DHW, "Set temp: %.2f (response: %.2f)",
    targetTemp, vars.slave.dhw.targetTemp
  );
});

static Task writeCh2SetpointTask("Write CH2 setpoint", [](void*) {
  // Target CH2 heating temp
  const float targetTemp = getCh2ChannelSetpoint();

  // Set CH2 temp
  const auto request = ESP32OT::Messages::Float::writeTemperature(
    ESP32OT::MessageID::TsetCH2,
    targetTemp
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_CH2, "Failed set temp");
    return;
  }

  vars.slave.ch2.targetTemp = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT_CH2, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  ch2SetpointWriteTime = millis();

  ESP_LOGI(
    L_OT_CH2, "Set temp: %.2f (response: %.2f)",
    targetTemp, vars.slave.ch2.targetTemp
  );
});

static Task writeRoomSetpointTask("Write room setpoint", [](void*) {
  // Target indoor temp
  const float& targetTemp = vars.master.heating.targetTemp;

  // Set target indoor temp
  const auto request = ESP32OT::Messages::Float::writeTemperature(
    ESP32OT::MessageID::TrSet,
    targetTemp
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_HEATING, "Failed set target indoor temp");
    return;
  }

  vars.slave.heating.roomTargetTemp = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT_HEATING, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  roomSetpointWriteTime = millis();

  ESP_LOGI(
    L_OT_HEATING, "Set target indoor temp: %.2f (response: %.2f)",
    targetTemp, vars.slave.heating.roomTargetTemp
  );
});

static Task writeRoomSetpointCh2Task("Write CH2 room setpoint", [](void*) {
  // Target indoor temp
  const float& targetTemp = vars.master.heating.targetTemp;

  // Set target CH2 temp
  const auto request = ESP32OT::Messages::Float::writeTemperature(
    ESP32OT::MessageID::TrSetCH2,
    targetTemp
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_HEATING, "Failed set target CH2 indoor temp");
    return;
  }

  vars.slave.ch2.roomTargetTemp = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT_HEATING, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  ch2RoomSetpointWriteTime = millis();

  ESP_LOGI(
    L_OT_HEATING, "Set target CH2 indoor temp: %.2f (response: %.2f)",
    targetTemp, vars.slave.ch2.roomTargetTemp
  );
});

static Task writeMaxModulationTask("Write maximum modulation", [](void*) {
  // Set max modulation level
  uint8_t maxModulation = vars.slave.modulation.max;
  if (vars.slave.heating.active) {
    maxModulation = settings.heating.maxModulation;

  } else if (vars.slave.dhw.active) {
    maxModulation = settings.dhw.maxModulation;

  } else {
    // This fixes the inability to enable channels if
    // the maximum modulation level has become zero for some reason.

    if (vars.master.heating.enabled && maxModulation < settings.heating.maxModulation) {
      maxModulation = settings.heating.maxModulation;
    }

    if (vars.master.dhw.enabled && maxModulation < settings.dhw.maxModulation) {
      maxModulation = settings.dhw.maxModulation;
    }
  }

  const ESP32OT::Messages::Float request(ESP32OT::MessageID::MaxRelModLevelSetting, maxModulation);
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(
      L_OT, "Failed set max modulation: %hhu%% (response: %hhu%%)",
      maxModulation, vars.slave.modulation.max
    );
    return;
  }

  vars.slave.modulation.max = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  ESP_LOGI(
    L_OT, "Set max modulation: %hhu%% (response: %hhu%%)",
    maxModulation, vars.slave.modulation.max
  );
});

static Task writeCoolingSetpointTask("Write cooling setpoint", [](void*) {
  // Set cooling setpoint = heating max modulation
  const ESP32OT::Messages::Float request(
    ESP32OT::MessageID::CoolingControl,
    settings.heating.maxModulation
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(
      L_OT, "Failed set cooling setpoint: %hhu%% (response: %hhu%%)",
      settings.heating.maxModulation, vars.slave.cooling.setpoint
    );
    return;
  }

  vars.slave.cooling.setpoint = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  ESP_LOGI(
    L_OT, "Set cooling setpoint: %hhu%% (response: %hhu%%)",
    settings.heating.maxModulation, vars.slave.cooling.setpoint
  );
});

// Fault reset action
static Task writeFaultResetTask("Write fault reset", [](void*) {
  const auto response = slave.sendRequest(ESP32OT::Messages::RemoteRequest(1));
  if (!response.isSuccess() || response.getRequestCode() != 1 || response.getResponseCode() < 128) {
    ESP_LOGE(L_OT, "Boiler fault reset failed");
    return;
  }

  slave.sendRequest(ESP32OT::Messages::RemoteRequest(0));
  ESP_LOGI(L_OT, "Boiler fault reset successfully");
});

// Diag reset action
static Task writeDiagnosticResetTask("Write diagnostic reset", [](void*) {
  const auto response = slave.sendRequest(ESP32OT::Messages::RemoteRequest(10));
  if (!response.isSuccess() || response.getRequestCode() != 10 || response.getResponseCode() < 128) {
    ESP_LOGE(L_OT, "Boiler diagnostic reset failed");
    return;
  }

  slave.sendRequest(ESP32OT::Messages::RemoteRequest(0));
  ESP_LOGI(L_OT, "Boiler diagnostic reset successfully");
});

Task writeRoomTemperatureTask("Write room temperature", [](void*) {
  // Current indoor temp
  const float& indoorTemp = vars.master.heating.indoorTemp;

  // Set current indoor temp
  const auto request = ESP32OT::Messages::Float::writeTemperature(
    ESP32OT::MessageID::Tr,
    indoorTemp
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_HEATING, "Failed set current indoor temp");
    return;
  }

  vars.slave.heating.indoorTemp = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT_HEATING, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  ESP_LOGI(
    L_OT_HEATING, "Set current indoor temp: %.2f (response: %.2f)",
    indoorTemp, vars.slave.heating.indoorTemp
  );
});

Task writeRoomTemperatureCh2Task("Write CH2 room temperature", [](void*) {
  // Current indoor temp
  const float& indoorTemp = vars.master.heating.indoorTemp;

  const auto request = ESP32OT::Messages::Float::writeTemperature(
    ESP32OT::MessageID::TrCH2,
    indoorTemp
  );
  const auto response = slave.sendRequest(request);

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_HEATING, "Failed set current CH2 indoor temp");
    return;
  }

  vars.slave.ch2.indoorTemp = response.get();

  if (response.getData() != request.getData()) {
    ESP_LOGW(
      L_OT_HEATING, responseMismatchMessage,
      request.getData(), response.getData()
    );
    return;
  }

  ESP_LOGI(
    L_OT_HEATING, "Set current CH2 indoor temp: %.2f (response: %.2f)",
    indoorTemp, vars.slave.ch2.indoorTemp
  );
});

static Task readModulationTask("Read modulation", [](void*) {
  // Update modulation level
  if (vars.slave.flame) {
    const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::RelModLevel));
    const float value = response.get();

    if (!response.isSuccess() || value < 0) {
      ESP_LOGW(L_OT, "Failed receive modulation level");
      return;
    }

    vars.slave.modulation.current = value;

    float power = 0.0f;
    if (settings.opentherm.maxPower > 0.1f) {
      power += settings.opentherm.minPower;

      if (vars.slave.modulation.current > 0) {
        power += (
          settings.opentherm.maxPower - settings.opentherm.minPower
        ) / 100.0f * vars.slave.modulation.current;
      }
    }
    vars.slave.power.current = power;

    ESP_LOGI(
      L_OT, "Received modulation level: %hhu%%, power: %.2f of %.2f kW (min: %.2f kW)",
      vars.slave.modulation.current, vars.slave.power.current,
      settings.opentherm.maxPower, settings.opentherm.minPower
    );

    // Modulation level sensors
    Sensors::setValueByType(
      Sensors::Type::OT_MODULATION_LEVEL, vars.slave.modulation.current,
      Sensors::ValueType::PRIMARY, true, true
    );

    // Power sensors
    Sensors::setValueByType(
      Sensors::Type::OT_CURRENT_POWER, vars.slave.power.current,
      Sensors::ValueType::PRIMARY, true, true
    );

  } else {
    vars.slave.modulation.current = 0;
    vars.slave.power.current = 0.0f;

    // Modulation level sensors
    Sensors::setValueByType(
      Sensors::Type::OT_MODULATION_LEVEL, vars.slave.modulation.current,
      Sensors::ValueType::PRIMARY, true, true
    );

    // Power sensors
    Sensors::setValueByType(
      Sensors::Type::OT_CURRENT_POWER, vars.slave.power.current,
      Sensors::ValueType::PRIMARY, true, true
    );
  }
});

static Task readDhwTemperatureTask("Read DHW temperature", [](void*) {
  // Update DHW temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::Tdhw));
  const float value = response.get();

  if (!response.isSuccess() || value <= 0) {
    ESP_LOGW(L_OT_DHW, "Failed receive temp");
    return;
  }

  vars.slave.dhw.currentTemp = value;

  ESP_LOGI(
    L_OT_DHW, "Received temp: %.2f",
    vars.slave.dhw.currentTemp
  );

  Sensors::setValueByType(
    Sensors::Type::OT_DHW_TEMP, vars.slave.dhw.currentTemp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readDhwTemperature2Task("Read DHW temperature 2", [](void*) {
  // Update DHW temp 2
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::Tdhw2));
  const float value = response.get();

  if (!response.isSuccess() || value <= 0) {
    ESP_LOGW(L_OT_DHW, "Failed receive temp 2");
    return;
  }

  vars.slave.dhw.currentTemp2 = value;

  ESP_LOGI(
    L_OT_DHW, "Received temp 2: %.2f",
    vars.slave.dhw.currentTemp2
  );

  Sensors::setValueByType(
    Sensors::Type::OT_DHW_TEMP2, vars.slave.dhw.currentTemp2,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readDhwFlowTask("Read DHW flow", [](void*) {
  // Update DHW flow rate
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::DHWFlowRate));
  float value = response.get();

  if (!response.isSuccess() || value < 0) {
    ESP_LOGW(L_OT_DHW, "Failed receive flow rate");
    return;
  }

  // no minuscule values
  // some boilers send a response of 0.06 when there is no flow
  if (value < 0.1f) {
    value = 0.0f;
  }

  vars.slave.dhw.flowRate = value;

  ESP_LOGI(
    L_OT_DHW, "Received flow rate: %.2f",
    vars.slave.dhw.flowRate
  );

  Sensors::setValueByType(
    Sensors::Type::OT_DHW_FLOW_RATE, vars.slave.dhw.flowRate,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readHeatingTemperatureTask("Read heating temperature", [](void*) {
  // Update heating temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::Tboiler));
  const float value = response.get();

  if (!response.isSuccess() || value <= 0) {
    ESP_LOGW(L_OT_HEATING, "Failed receive temp");
    return;
  }

  vars.slave.heating.currentTemp = value;

  ESP_LOGI(
    L_OT_HEATING, "Received temp: %.2f",
    vars.slave.heating.currentTemp
  );

  Sensors::setValueByType(
    Sensors::Type::OT_HEATING_TEMP, vars.slave.heating.currentTemp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readHeatingReturnTemperatureTask("Read return temperature", [](void*) {
  // Update heating return temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::Tret));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_HEATING, "Failed receive return temp");
    return;
  }

  vars.slave.heating.returnTemp = response.get();

  ESP_LOGI(
    L_OT_HEATING, "Received return temp: %.2f",
    vars.slave.heating.returnTemp
  );

  Sensors::setValueByType(
    Sensors::Type::OT_HEATING_RETURN_TEMP, vars.slave.heating.returnTemp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readCh2TemperatureTask("Read CH2 temperature", [](void*) {
  // Update CH2 temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::TflowCH2));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT_CH2, "Failed receive temp");
    return;
  }

  vars.slave.ch2.currentTemp = response.get();

  ESP_LOGI(
    L_OT_CH2, "Received temp: %.2f",
    vars.slave.ch2.currentTemp
  );

  Sensors::setValueByType(
    Sensors::Type::OT_CH2_TEMP, vars.slave.ch2.currentTemp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readExhaustTemperatureTask("Read exhaust temperature", [](void*) {
  // Update exhaust temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Int16(ESP32OT::MessageID::Texhaust));
  const float value = response.getInt();

  if (!response.isSuccess() || !isValidTemp(value, UnitSystem::METRIC, -40, 500)) {
    ESP_LOGW(L_OT, "Failed receive exhaust temp");
    return;
  }

  vars.slave.exhaust.temp = value;

  ESP_LOGI(
    L_OT, "Received exhaust temp: %.2f",
    vars.slave.exhaust.temp
  );

  Sensors::setValueByType(
    Sensors::Type::OT_EXHAUST_TEMP, vars.slave.exhaust.temp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readHeatExchangerTemperatureTask("Read heat exchanger temperature", [](void*) {
  // Update heat exchanger temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::TboilerHeatExchanger));
  const float value = response.get();

  if (!response.isSuccess() || value <= 0) {
    ESP_LOGW(L_OT, "Failed receive heat exchanger temp");
    return;
  }

  vars.slave.heatExchangerTemp = value;

  ESP_LOGI(
    L_OT, "Received heat exchanger temp: %.2f",
    vars.slave.heatExchangerTemp
  );

  Sensors::setValueByType(
    Sensors::Type::OT_HEAT_EXCHANGER_TEMP, vars.slave.heatExchangerTemp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readOutdoorTemperatureTask("Read outdoor temperature", [](void*) {
  // Update outdoor temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::Toutside));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive outdoor temp");
    return;
  }

  vars.slave.heating.outdoorTemp = response.get();

  ESP_LOGI(
    L_OT, "Received outdoor temp: %.2f",
    vars.slave.heating.outdoorTemp
  );

  Sensors::setValueByType(
    Sensors::Type::OT_OUTDOOR_TEMP, vars.slave.heating.outdoorTemp,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readSolarStorageTemperatureTask("Read solar storage temperature", [](void*) {
  // Update solar storage temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::Tstorage));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive solar storage temp");
    return;
  }

  vars.slave.solar.storage = response.get();

  ESP_LOGI(
    L_OT, "Received solar storage temp: %.2f",
    vars.slave.solar.storage
  );

  Sensors::setValueByType(
    Sensors::Type::OT_SOLAR_STORAGE_TEMP, vars.slave.solar.storage,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readSolarCollectorTemperatureTask("Read solar collector temperature", [](void*) {
  // Update solar collector temp
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::Tcollector));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive solar collector temp");
    return;
  }

  vars.slave.solar.collector = response.get();

  ESP_LOGI(
    L_OT, "Received solar collector temp: %.2f",
    vars.slave.solar.collector
  );

  Sensors::setValueByType(
    Sensors::Type::OT_SOLAR_COLLECTOR_TEMP, vars.slave.solar.collector,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readFanSpeedTask("Read fan speed", [](void*) {
  // Update fan speed
  const auto response = slave.sendRequest(ESP32OT::Messages::FanSpeed());

  if (!response.isSuccess()) {
    return;
  }

  vars.slave.fanSpeed.setpoint = response.getSetpointHz();
  vars.slave.fanSpeed.current = response.getActualHz();

  ESP_LOGI(
    L_OT, "Received fan speed, setpoint: %hhu%%, current: %hhu%%",
    vars.slave.fanSpeed.setpoint, vars.slave.fanSpeed.current
  );

  Sensors::setValueByType(
    Sensors::Type::OT_FAN_SPEED_SETPOINT, vars.slave.fanSpeed.setpoint,
    Sensors::ValueType::PRIMARY, true, true
  );
  Sensors::setValueByType(
    Sensors::Type::OT_FAN_SPEED_CURRENT, vars.slave.fanSpeed.current,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readPressureTask("Read pressure", [](void*) {
  // Update pressure
  const auto response = slave.sendRequest(ESP32OT::Messages::Float(ESP32OT::MessageID::CHPressure));
  const float value = response.get();

  if (!response.isSuccess() || value < 0) {
    ESP_LOGW(L_OT, "Failed receive pressure");
    return;
  }

  vars.slave.pressure = value;

  ESP_LOGI(
    L_OT, "Received pressure: %.2f",
    vars.slave.pressure
  );

  Sensors::setValueByType(
    Sensors::Type::OT_PRESSURE, vars.slave.pressure,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readExhaustCo2Task("Read exhaust CO2", [](void*) {
  // Update exhaust CO2
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::CO2exhaust));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive exhaust CO2");
    return;
  }

  vars.slave.exhaust.co2 = response.getUInt();

  ESP_LOGI(
    L_OT, "Received exhaust CO2: %hu ppm",
    vars.slave.exhaust.co2
  );

  Sensors::setValueByType(
    Sensors::Type::OT_EXHAUST_CO2, vars.slave.exhaust.co2,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readExhaustFanSpeedTask("Read exhaust fan speed", [](void*) {
  // Update exhaust fan speed
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::RPMexhaust));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive exhaust fan speed");
    return;
  }

  vars.slave.exhaust.fanSpeed = response.getUInt();

  ESP_LOGI(
    L_OT, "Received exhaust fan speed: %hu rpm",
    vars.slave.exhaust.fanSpeed
  );

  Sensors::setValueByType(
    Sensors::Type::OT_EXHAUST_FAN_SPEED, vars.slave.exhaust.fanSpeed,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readSupplyFanSpeedTask("Read supply fan speed", [](void*) {
  // Update supply fan speed
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::RPMsupply));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive supply fan speed");
    return;
  }

  vars.slave.fanSpeed.supply = response.getUInt();

  ESP_LOGI(
    L_OT, "Received supply fan speed: %hu rpm",
    vars.slave.fanSpeed.supply
  );

  Sensors::setValueByType(
    Sensors::Type::OT_SUPPLY_FAN_SPEED, vars.slave.fanSpeed.supply,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task writeYearTask("Write year", [](void*) {
  struct tm boilerTime;
  if (getLocalTime(&boilerTime, 0)) {
    const ESP32OT::Messages::UInt16 request(ESP32OT::MessageID::Year, (boilerTime.tm_year + 1900) & 0xFFFF);
    const auto response = slave.sendRequest(request);

    if (!response.isSuccess() || response.getData() != request.getData()) {
      ESP_LOGI(L_OT, "Failed set year of date");
      return;
    }

    ESP_LOGI(L_OT, "Year of date set successfully");
  }
});

static Task writeDayAndMonthTask("Write date", [](void*) {
  // Other operations can run between clock writes; use the current date.
  struct tm boilerTime;
  if (getLocalTime(&boilerTime, 0)) {
    const uint8_t month = (boilerTime.tm_mon + 1) & 0xFF;
    const ESP32OT::Messages::Date request(month, boilerTime.tm_mday & 0xFF);
    const auto response = slave.sendRequest(request);

    if (!response.isSuccess() || response.getData() != request.getData()) {
      ESP_LOGI(L_OT, "Failed set day and month of date");
      return;
    }

    ESP_LOGI(L_OT, "Day and month of date set successfully");
  }
});

static Task writeTimeTask("Write time", [](void*) {
  struct tm boilerTime;
  if (getLocalTime(&boilerTime, 0)) {
    const uint8_t dayOfWeek = boilerTime.tm_wday == 0 ? 7 : boilerTime.tm_wday;
    const ESP32OT::Messages::DayTime request(dayOfWeek, boilerTime.tm_hour, boilerTime.tm_min & 0x3F);

    const auto response = slave.sendRequest(request);

    if (!response.isSuccess() || response.getData() != request.getData()) {
      ESP_LOGI(L_OT, "Failed set time");
      return;
    }

    ESP_LOGI(L_OT, "Time set successfully");
  }
});

static Task readPowerLimitsTask("Read power limits", [](void*) {
  // Get min modulation level & max power
  const auto response = slave.sendRequest(ESP32OT::Messages::Capacity());

  if (!response.isSuccess() || response.getMinModulation() > 100) {
    ESP_LOGW(L_OT, "Failed receive min modulation and max power");
    return;
  }

  vars.slave.modulation.min = response.getMinModulation();
  vars.slave.power.max = response.getMaxCapacity();
  vars.slave.power.min = vars.slave.modulation.min > 0 && vars.slave.power.max > 0.1f
    ? (vars.slave.modulation.min * 0.01f) * vars.slave.power.max
    : 0.0f;

  ESP_LOGI(
    L_OT, "Received min modulation: %hhu%%, max power: %.2f kW",
    vars.slave.modulation.min, vars.slave.power.max
  );

  if (settings.heating.maxModulation < vars.slave.modulation.min) {
    settings.heating.maxModulation = vars.slave.modulation.min;
    requestSaveSettings();

    ESP_LOGW(
      L_SETTINGS_HEATING, "Updated min modulation: %hhu%%",
      settings.heating.maxModulation
    );
  }

  if (settings.dhw.maxModulation < vars.slave.modulation.min) {
    settings.dhw.maxModulation = vars.slave.modulation.min;
    requestSaveSettings();

    ESP_LOGW(
      L_SETTINGS_DHW, "Updated min modulation: %hhu%%",
      settings.dhw.maxModulation
    );
  }

  if (fabsf(settings.opentherm.maxPower) < 0.1f && vars.slave.power.max > 0.1f) {
    settings.opentherm.maxPower = vars.slave.power.max;
    settings.opentherm.minPower = vars.slave.power.min;

    requestSaveSettings();
    ESP_LOGW(
      L_SETTINGS_OT, "Updated power, min: %.2f kW, max: %.2f kW",
      settings.opentherm.minPower, settings.opentherm.maxPower
    );
  }
});

static Task readDhwLimitsTask("Read DHW limits", [](void*) {
  // Get DHW min/max temp (if necessary)
  if (settings.opentherm.options.dhwSupport && settings.opentherm.options.getMinMaxTemp) {
    const auto response = slave.sendRequest(
      ESP32OT::Messages::TemperatureBounds(ESP32OT::MessageID::TdhwSetUBTdhwSetLB)
    );
    int8_t minTemp = response.getLower();
    int8_t maxTemp = response.getUpper();

    if (response.isSuccess() && minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.slave.dhw.minTemp = minTemp;
      vars.slave.dhw.maxTemp = maxTemp;

      float convertedMinTemp = ceilf(vars.slave.dhw.minTemp);
      float convertedMaxTemp = floorf(vars.slave.dhw.maxTemp);

      convertedMinTemp = constrain(convertedMinTemp, 0.0f, 255.0f);
      convertedMaxTemp = constrain(convertedMaxTemp, 0.0f, 255.0f);

      // Integer settings need two distinct limits within their storage range.
      if (convertedMinTemp >= convertedMaxTemp) {
        ESP_LOGW(L_OT_DHW, unrepresentableTemperatureLimitsMessage);
        return;
      }

      ESP_LOGI(
        L_OT_DHW, "Received min temp: %hhu (converted: %.0f), max temp: %hhu (converted: %.0f)",
        vars.slave.dhw.minTemp, convertedMinTemp, vars.slave.dhw.maxTemp, convertedMaxTemp
      );

      if (settings.dhw.minTemp < convertedMinTemp) {
        settings.dhw.minTemp = static_cast<uint8_t>(convertedMinTemp);
        requestSaveSettings();

        ESP_LOGW(L_SETTINGS_DHW, "Updated min temp: %hhu", settings.dhw.minTemp);
      }

      if (settings.dhw.maxTemp > convertedMaxTemp) {
        settings.dhw.maxTemp = static_cast<uint8_t>(convertedMaxTemp);
        requestSaveSettings();

        ESP_LOGW(L_SETTINGS_DHW, "Updated max temp: %hhu", settings.dhw.maxTemp);
      }

      if (settings.dhw.minTemp >= settings.dhw.maxTemp) {
        settings.dhw.minTemp = static_cast<uint8_t>(convertedMinTemp);
        settings.dhw.maxTemp = static_cast<uint8_t>(convertedMaxTemp);
        requestSaveSettings();

        ESP_LOGW(L_SETTINGS_DHW, "Updated min temp: %hhu, max temp: %hhu", settings.dhw.minTemp, settings.dhw.maxTemp);
      }

    } else {
      ESP_LOGW(L_OT_DHW, "Failed receive min/max temp");
    }
  }

  if (settings.dhw.minTemp >= settings.dhw.maxTemp) {
    settings.dhw.minTemp = DEFAULT_DHW_MIN_TEMP;
    settings.dhw.maxTemp = DEFAULT_DHW_MAX_TEMP;
    requestSaveSettings();
  }
});

static Task readHeatingLimitsTask("Read heating limits", [](void*) {
  // Get heating min/max temp
  if (settings.opentherm.options.getMinMaxTemp) {
    const auto response = slave.sendRequest(
      ESP32OT::Messages::TemperatureBounds(ESP32OT::MessageID::MaxTSetUBMaxTSetLB)
    );
    int8_t minTemp = response.getLower();
    int8_t maxTemp = response.getUpper();

    if (response.isSuccess() && minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp) {
      vars.slave.heating.minTemp = minTemp;
      vars.slave.heating.maxTemp = maxTemp;

      float convertedMinTemp = ceilf(vars.slave.heating.minTemp);
      float convertedMaxTemp = floorf(vars.slave.heating.maxTemp);

      convertedMinTemp = constrain(convertedMinTemp, 0.0f, 255.0f);
      convertedMaxTemp = constrain(convertedMaxTemp, 0.0f, 255.0f);

      // Integer settings need two distinct limits within their storage range.
      if (convertedMinTemp >= convertedMaxTemp) {
        ESP_LOGW(L_OT_HEATING, unrepresentableTemperatureLimitsMessage);
        return;
      }

      ESP_LOGI(
        L_OT_HEATING, "Received min temp: %hhu (converted: %.0f), max temp: %hhu (converted: %.0f)",
        vars.slave.heating.minTemp, convertedMinTemp, vars.slave.heating.maxTemp, convertedMaxTemp
      );

      if (settings.heating.minTemp < convertedMinTemp) {
        settings.heating.minTemp = static_cast<uint8_t>(convertedMinTemp);
        requestSaveSettings();

        ESP_LOGW(L_SETTINGS_HEATING, "Updated min temp: %hhu", settings.heating.minTemp);
      }

      if (settings.heating.maxTemp > convertedMaxTemp) {
        settings.heating.maxTemp = static_cast<uint8_t>(convertedMaxTemp);
        requestSaveSettings();

        ESP_LOGW(L_SETTINGS_HEATING, "Updated max temp: %hhu", settings.heating.maxTemp);
      }

      if (settings.heating.minTemp >= settings.heating.maxTemp) {
        settings.heating.minTemp = static_cast<uint8_t>(convertedMinTemp);
        settings.heating.maxTemp = static_cast<uint8_t>(convertedMaxTemp);
        requestSaveSettings();

        ESP_LOGW(L_SETTINGS_HEATING, "Updated min temp: %hhu, max temp: %hhu", settings.heating.minTemp, settings.heating.maxTemp);
      }

    } else {
      ESP_LOGW(L_OT_HEATING, "Failed receive min/max temp");
    }
  }

  if (settings.heating.minTemp >= settings.heating.maxTemp) {
    settings.heating.minTemp = DEFAULT_HEATING_MIN_TEMP;
    settings.heating.maxTemp = DEFAULT_HEATING_MAX_TEMP;
    requestSaveSettings();
  }
});

static Task readFaultTask("Read fault code", [](void*) {
  // Get fault code
  const auto response = slave.sendRequest(ESP32OT::Messages::Fault());

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive fault code");
    return;
  }

  vars.slave.fault.code = response.getOemFaultCode();

  ESP_LOGI(
    L_OT, "Received fault code: %hhu (0x%02X)",
    vars.slave.fault.code, vars.slave.fault.code
  );
});

static Task readDiagnosticTask("Read diagnostic code", [](void*) {
  // Get diagnostic code
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::OEMDiagnosticCode));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive diag code");
    return;
  }

  vars.slave.diag.code = response.getUInt();

  ESP_LOGI(
    L_OT, "Received diag code: %hu (0x%02X)",
    vars.slave.diag.code, vars.slave.diag.code
  );
});

static Task readBurnerStartsTask("Read burner starts", [](void*) {
  // Update burner starts
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::SuccessfulBurnerStarts));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive burner starts");
    return;
  }

  vars.slave.stats.burnerStarts = response.getUInt();

  ESP_LOGI(L_OT, "Received burner starts: %hu", vars.slave.stats.burnerStarts);

  Sensors::setValueByType(
    Sensors::Type::OT_BURNER_STARTS, vars.slave.stats.burnerStarts,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readDhwBurnerStartsTask("Read DHW burner starts", [](void*) {
  // Update DHW burner starts
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::DHWBurnerStarts));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive DHW burner starts");
    return;
  }

  vars.slave.stats.dhwBurnerStarts = response.getUInt();

  ESP_LOGI(L_OT, "Received DHW burner starts: %hu", vars.slave.stats.dhwBurnerStarts);

  Sensors::setValueByType(
    Sensors::Type::OT_DHW_BURNER_STARTS, vars.slave.stats.dhwBurnerStarts,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readHeatingPumpStartsTask("Read heating pump starts", [](void*) {
  // Update heating pump starts
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::CHPumpStarts));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive heating pump starts");
    return;
  }

  vars.slave.stats.heatingPumpStarts = response.getUInt();

  ESP_LOGI(L_OT, "Received heating pump starts: %hu", vars.slave.stats.heatingPumpStarts);

  Sensors::setValueByType(
    Sensors::Type::OT_HEATING_PUMP_STARTS, vars.slave.stats.heatingPumpStarts,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readDhwPumpStartsTask("Read DHW pump starts", [](void*) {
  // Update DHW pump starts
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::DHWPumpValveStarts));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive DHW pump starts");
    return;
  }

  vars.slave.stats.dhwPumpStarts = response.getUInt();

  ESP_LOGI(L_OT, "Received DHW pump starts: %hu", vars.slave.stats.dhwPumpStarts);

  Sensors::setValueByType(
    Sensors::Type::OT_DHW_PUMP_STARTS, vars.slave.stats.dhwPumpStarts,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readBurnerHoursTask("Read burner hours", [](void*) {
  // Update burner hours
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::BurnerOperationHours));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive burner hours");
    return;
  }

  vars.slave.stats.burnerHours = response.getUInt();

  ESP_LOGI(L_OT, "Received burner hours: %hu", vars.slave.stats.burnerHours);

  Sensors::setValueByType(
    Sensors::Type::OT_BURNER_HOURS, vars.slave.stats.burnerHours,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readDhwBurnerHoursTask("Read DHW burner hours", [](void*) {
  // Update DHW burner hours
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::DHWBurnerOperationHours));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive DHW burner hours");
    return;
  }

  vars.slave.stats.dhwBurnerHours = response.getUInt();

  ESP_LOGI(L_OT, "Received DHW burner hours: %hu", vars.slave.stats.dhwBurnerHours);

  Sensors::setValueByType(
    Sensors::Type::OT_DHW_BURNER_HOURS, vars.slave.stats.dhwBurnerHours,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readHeatingPumpHoursTask("Read heating pump hours", [](void*) {
  // Update heating pump hours
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::CHPumpOperationHours));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive heating pump hours");
    return;
  }

  vars.slave.stats.heatingPumpHours = response.getUInt();

  ESP_LOGI(L_OT, "Received heating pump hours: %hu", vars.slave.stats.heatingPumpHours);

  Sensors::setValueByType(
    Sensors::Type::OT_HEATING_PUMP_HOURS, vars.slave.stats.heatingPumpHours,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readDhwPumpHoursTask("Read DHW pump hours", [](void*) {
  // Update DHW pump hours
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::DHWPumpValveOperationHours));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive DHW pump hours");
    return;
  }

  vars.slave.stats.dhwPumpHours = response.getUInt();

  ESP_LOGI(L_OT, "Received DHW pump hours: %hu", vars.slave.stats.dhwPumpHours);

  Sensors::setValueByType(
    Sensors::Type::OT_DHW_PUMP_HOURS, vars.slave.stats.dhwPumpHours,
    Sensors::ValueType::PRIMARY, true, true
  );
});

static Task readCoolingHoursTask("Read cooling hours", [](void*) {
  // Update cooling hours
  const auto response = slave.sendRequest(ESP32OT::Messages::UInt16(ESP32OT::MessageID::CoolingOperationHours));

  if (!response.isSuccess()) {
    ESP_LOGW(L_OT, "Failed receive cooling hours");
    return;
  }

  vars.slave.stats.coolingHours = response.getUInt();

  ESP_LOGI(L_OT, "Received cooling hours: %hu", vars.slave.stats.coolingHours);

  Sensors::setValueByType(
    Sensors::Type::OT_COOLING_HOURS, vars.slave.stats.coolingHours,
    Sensors::ValueType::PRIMARY, true, true
  );
});

// Status may force this task while running its own onDone.
static void onOperationDone(const Task&, uint32_t) {
  if (readStatusTask.idle()) {
    readStatusTask.tryRun();
  }

  if (esp_log_level_get(L_OT) >= ESP_LOG_VERBOSE) {
    static uint32_t lastManagerStack = UINT32_MAX;
    static uint32_t lastWorkerStack = UINT32_MAX;
    const uint32_t managerStack = uxTaskGetStackHighWaterMark(nullptr);
    const uint32_t workerStack = slave.getWorkerStackHighWaterMark();
    if (workerStack > 0 && (managerStack < lastManagerStack || workerStack < lastWorkerStack)) {
      ESP_LOGV(
        L_OT, "Stack free: manager %lu, RMT worker %lu bytes",
        static_cast<unsigned long>(managerStack), static_cast<unsigned long>(workerStack)
      );
      lastManagerStack = managerStack;
      lastWorkerStack = workerStack;
    }
  }
}

void initOpenTherm() {
  // Configure OpenTherm before polling the boiler.
  configureDriverTask.setEnabledWhen([] {
    return !restartTask.scheduled();
  }).resume();
  openthermTaskManager.addTask(configureDriverTask);

  // Reconfigure GPIO and track initialization requirements.
  maintainOpenThermTask.setEnabledWhen([] {
    return !restartTask.scheduled();
  });
  openthermTaskManager.addTask(maintainOpenThermTask);

  // Exchange boiler status and monitor the connection.
  readStatusTask.setInterval(750)
    .setEnabledWhen(canUseBus)
    .onDone([](const Task&, uint32_t) {
      // Both tasks can reach each other's onDone; never re-enter an active task.
      if (vars.slave.flame || vars.slave.modulation.current > 0) {
        if (readModulationTask.idle() && readModulationTask.enabled()) {
          readModulationTask.forceRun();
        }
      }
    });
  openthermTaskManager.addTask(readStatusTask);

  // Identify the slave before enabling ordinary operations.
  initializeSlaveTask.setEnabledWhen([] {
    return canUseBus() && vars.slave.connected;
  });
  openthermTaskManager.addTask(initializeSlaveTask);

  // Read the boiler application version.
  readSlaveVersionTask.setEnabledWhen(isInitializingSlave)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readSlaveVersionTask);

  // Write the master application version.
  writeMasterVersionTask.setEnabledWhen(isInitializingSlave)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeMasterVersionTask);

  // Read the boiler OpenTherm version.
  readSlaveProtocolTask.setEnabledWhen(isInitializingSlave)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readSlaveProtocolTask);

  // Write the master OpenTherm version.
  writeMasterProtocolTask.setEnabledWhen(isInitializingSlave)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeMasterProtocolTask);

  // Read the boiler configuration.
  readSlaveConfigTask.setEnabledWhen(isInitializingSlave)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readSlaveConfigTask);

  // Write the master configuration.
  writeMasterConfigTask.setEnabledWhen(isInitializingSlave)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeMasterConfigTask);

  // Write the heating water setpoint and optional maximum temperature.
  writeHeatingSetpointTask.setInterval(750)
    .setEnabledWhen([] {
      return isSlaveReady() && needWriteHeatingSetpoint(getHeatingChannelSetpoint());
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeHeatingSetpointTask);

  // Write the DHW temperature setpoint.
  writeDhwSetpointTask.setInterval(750)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             settings.opentherm.options.dhwSupport &&
             needWriteDhwSetpoint(getDhwChannelSetpoint());
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeDhwSetpointTask);

  // Write the CH2 water temperature setpoint.
  writeCh2SetpointTask.setInterval(750)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             (settings.opentherm.options.heatingToCh2 || settings.opentherm.options.dhwToCh2 ||
              (!vars.master.ch2.enabled && settings.opentherm.ch2DisablingAction != ChannelDisablingAction::OFF)) &&
             needWriteCh2Setpoint(getCh2ChannelSetpoint());
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeCh2SetpointTask);

  // Write the heating room setpoint for native OTC.
  writeRoomSetpointTask.setInterval(750)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             settings.opentherm.options.nativeOTC &&
             needWriteRoomSetpoint(vars.master.heating.targetTemp);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeRoomSetpointTask);

  // Write the mirrored CH2 room setpoint for native OTC.
  writeRoomSetpointCh2Task.setInterval(750)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             settings.opentherm.options.nativeOTC && settings.opentherm.options.heatingToCh2 &&
             needWriteRoomSetpointCh2(vars.master.heating.targetTemp);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeRoomSetpointCh2Task);

  // Write the maximum relative modulation level.
  writeMaxModulationTask.setInterval(5000)
    .setEnabledWhen(isSlaveReady)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeMaxModulationTask);

  // Write the cooling control signal.
  writeCoolingSetpointTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && settings.opentherm.options.coolingSupport;
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeCoolingSetpointTask);

  // Execute pending boiler fault reset requests.
  writeFaultResetTask.setInterval(750)
    .setEnabledWhen([] {
      return isSlaveReady() && vars.slave.fault.active &&
             (vars.actions.resetFault || settings.opentherm.options.autoFaultReset);
    })
    .onDone([](const Task& task, uint32_t elapsed) {
      vars.actions.resetFault = false;
      onOperationDone(task, elapsed);
    });
  openthermTaskManager.addTask(writeFaultResetTask);

  // Execute pending service reset requests.
  writeDiagnosticResetTask.setInterval(750)
    .setEnabledWhen([] {
      return isSlaveReady() && vars.slave.diag.active &&
             (vars.actions.resetDiagnostic || settings.opentherm.options.autoDiagReset);
    })
    .onDone([](const Task& task, uint32_t elapsed) {
      vars.actions.resetDiagnostic = false;
      onOperationDone(task, elapsed);
    });
  openthermTaskManager.addTask(writeDiagnosticResetTask);

  // Send the current indoor temperature.
  writeRoomTemperatureTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             (settings.opentherm.options.nativeOTC || settings.opentherm.options.alwaysSendIndoorTemp);
    })
    .onDone([](const Task& task, uint32_t elapsed) {
      writeRoomTemperatureTask.setInterval(60000);
      onOperationDone(task, elapsed);
    });
  openthermTaskManager.addTask(writeRoomTemperatureTask);

  // Send the current indoor temperature to CH2.
  writeRoomTemperatureCh2Task.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             (settings.opentherm.options.nativeOTC || settings.opentherm.options.alwaysSendIndoorTemp) &&
             settings.opentherm.options.heatingToCh2;
    })
    .onDone([](const Task& task, uint32_t elapsed) {
      writeRoomTemperatureCh2Task.setInterval(60000);
      onOperationDone(task, elapsed);
    });
  openthermTaskManager.addTask(writeRoomTemperatureCh2Task);

  // Read relative modulation and calculate current power.
  readModulationTask.setInterval(15000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             (Sensors::getAmountByType(Sensors::Type::OT_MODULATION_LEVEL, true) ||
              Sensors::getAmountByType(Sensors::Type::OT_CURRENT_POWER, true));
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readModulationTask);

  // Read the primary DHW temperature.
  readDhwTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             settings.opentherm.options.dhwSupport &&
             Sensors::getAmountByType(Sensors::Type::OT_DHW_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwTemperatureTask);

  // Read the second DHW temperature.
  readDhwTemperature2Task.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             settings.opentherm.options.dhwSupport &&
             Sensors::getAmountByType(Sensors::Type::OT_DHW_TEMP2, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwTemperature2Task);

  // Read the DHW flow rate.
  readDhwFlowTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             settings.opentherm.options.dhwSupport &&
             Sensors::getAmountByType(Sensors::Type::OT_DHW_FLOW_RATE, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwFlowTask);

  // Read the heating supply temperature.
  readHeatingTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_HEATING_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readHeatingTemperatureTask);

  // Read the heating return temperature.
  readHeatingReturnTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_HEATING_RETURN_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readHeatingReturnTemperatureTask);

  // Read the CH2 water temperature.
  readCh2TemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             Sensors::getAmountByType(Sensors::Type::OT_CH2_TEMP, true) &&
             vars.master.ch2.enabled && !settings.opentherm.options.nativeOTC;
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readCh2TemperatureTask);

  // Read the exhaust temperature.
  readExhaustTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_EXHAUST_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readExhaustTemperatureTask);

  // Read the boiler heat exchanger temperature.
  readHeatExchangerTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_HEAT_EXCHANGER_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readHeatExchangerTemperatureTask);

  // Read the outdoor temperature.
  readOutdoorTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_OUTDOOR_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readOutdoorTemperatureTask);

  // Read the solar storage temperature.
  readSolarStorageTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_SOLAR_STORAGE_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readSolarStorageTemperatureTask);

  // Read the solar collector temperature.
  readSolarCollectorTemperatureTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_SOLAR_COLLECTOR_TEMP, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readSolarCollectorTemperatureTask);

  // Read the requested and actual relative fan speeds.
  readFanSpeedTask.setInterval(5000)
    .setEnabledWhen([] {
      return isSlaveReady() &&
             (Sensors::getAmountByType(Sensors::Type::OT_FAN_SPEED_SETPOINT, true) ||
              Sensors::getAmountByType(Sensors::Type::OT_FAN_SPEED_CURRENT, true));
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readFanSpeedTask);

  // Read the heating circuit pressure.
  readPressureTask.setInterval(15000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_PRESSURE, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readPressureTask);

  // Read the exhaust CO2 concentration.
  readExhaustCo2Task.setInterval(15000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_EXHAUST_CO2, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readExhaustCo2Task);

  // Read the exhaust fan speed.
  readExhaustFanSpeedTask.setInterval(15000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_EXHAUST_FAN_SPEED, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readExhaustFanSpeedTask);

  // Read the supply fan speed.
  readSupplyFanSpeedTask.setInterval(15000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_SUPPLY_FAN_SPEED, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readSupplyFanSpeedTask);

  // Write the current year.
  writeYearTask.setInterval(600000)
    .setEnabledWhen([] {
      return isSlaveReady() && settings.opentherm.options.setDateAndTime;
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeYearTask);

  // Write the current day and month.
  writeDayAndMonthTask.setInterval(600000)
    .setEnabledWhen([] {
      return isSlaveReady() && settings.opentherm.options.setDateAndTime;
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeDayAndMonthTask);

  // Write the current weekday and time.
  writeTimeTask.setInterval(600000)
    .setEnabledWhen([] {
      return isSlaveReady() && settings.opentherm.options.setDateAndTime;
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(writeTimeTask);

  // Read minimum modulation and boiler power limits.
  readPowerLimitsTask.setInterval(60000)
    .setEnabledWhen(isSlaveReady)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readPowerLimitsTask);

  // Read and apply DHW temperature limits.
  readDhwLimitsTask.setInterval(60000)
    .setEnabledWhen(isSlaveReady)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwLimitsTask);

  // Read and apply heating temperature limits.
  readHeatingLimitsTask.setInterval(60000)
    .setEnabledWhen(isSlaveReady)
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readHeatingLimitsTask);

  // Read or clear the boiler fault code.
  readFaultTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && vars.slave.fault.active;
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readFaultTask);

  // Read or clear the diagnostic code.
  readDiagnosticTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && (vars.slave.diag.active || vars.slave.fault.active);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDiagnosticTask);

  // Read the burner start counter.
  readBurnerStartsTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_BURNER_STARTS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readBurnerStartsTask);

  // Read the DHW burner start counter.
  readDhwBurnerStartsTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_DHW_BURNER_STARTS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwBurnerStartsTask);

  // Read the heating pump start counter.
  readHeatingPumpStartsTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_HEATING_PUMP_STARTS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readHeatingPumpStartsTask);

  // Read the DHW pump start counter.
  readDhwPumpStartsTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_DHW_PUMP_STARTS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwPumpStartsTask);

  // Read accumulated burner operating hours.
  readBurnerHoursTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_BURNER_HOURS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readBurnerHoursTask);

  // Read accumulated DHW burner operating hours.
  readDhwBurnerHoursTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_DHW_BURNER_HOURS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwBurnerHoursTask);

  // Read accumulated heating pump operating hours.
  readHeatingPumpHoursTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_HEATING_PUMP_HOURS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readHeatingPumpHoursTask);

  // Read accumulated DHW pump operating hours.
  readDhwPumpHoursTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_DHW_PUMP_HOURS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readDhwPumpHoursTask);

  // Read accumulated cooling operating hours.
  readCoolingHoursTask.setInterval(60000)
    .setEnabledWhen([] {
      return isSlaveReady() && Sensors::getAmountByType(Sensors::Type::OT_COOLING_HOURS, true);
    })
    .onDone(onOperationDone);
  openthermTaskManager.addTask(readCoolingHoursTask);
}
