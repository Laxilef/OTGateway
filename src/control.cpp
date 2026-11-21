#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>

#include <algorithm>

Variables vars;

enum class PumpStartReason {NONE, HEATING, ANTISTUCK};

static bool heatingEnabled = false;
static unsigned long heatingDisabledTime = 0;
static PumpStartReason extPumpStartReason = PumpStartReason::NONE;
static unsigned long externalPumpStartTime = 0;
static bool emergencyDetected = false;
static unsigned long emergencyFlipTime = 0;

static Task freezeProtectionTask("Freeze protection", [](void*) {
  float lowTemp = 255.0f;
  uint8_t availableSensors = 0;

  if (Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::INDOOR_TEMP)) {
    auto value = Sensors::getMeanValueByPurpose(Sensors::Purpose::INDOOR_TEMP, Sensors::ValueType::PRIMARY, settings.heating.indoorTempAvgType);
    if (value < lowTemp) {
      lowTemp = value;
    }

    availableSensors++;
  }

  if (Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::HEATING_TEMP)) {
    auto value = Sensors::getMeanValueByPurpose(Sensors::Purpose::HEATING_TEMP, Sensors::ValueType::PRIMARY);
    if (value < lowTemp) {
      lowTemp = value;
    }

    availableSensors++;
  }

  if (Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::HEATING_RETURN_TEMP)) {
    auto value = Sensors::getMeanValueByPurpose(Sensors::Purpose::HEATING_RETURN_TEMP, Sensors::ValueType::PRIMARY);
    if (value < lowTemp) {
      lowTemp = value;
    }

    availableSensors++;
  }

  if (availableSensors) {
    if (vars.master.heating.freezing) {
      if (lowTemp - static_cast<float>(settings.heating.freezeProtection.highTemp) + 0.0001f >= 0.0f) {
        vars.master.heating.freezing = false;

        ESP_LOGI(
          L_MAIN,
          "No freezing detected. Current low temp: %.2f, threshold (high): %hhu",
          lowTemp, settings.heating.freezeProtection.highTemp
        );
      }

    } else {
      if (static_cast<float>(settings.heating.freezeProtection.lowTemp) - lowTemp + 0.0001f >= 0.0f) {
        vars.master.heating.freezing = true;

        if (!settings.heating.enabled) {
          settings.heating.enabled = true;
          requestSaveSettings();
        }

        ESP_LOGI(
          L_MAIN,
          "Freezing detected! Current low temp: %.2f, threshold (low): %hhu",
          lowTemp, settings.heating.freezeProtection.lowTemp
        );
      }
    }

  } else if (vars.master.heating.freezing) {
    vars.master.heating.freezing = false;

    ESP_LOGI(L_MAIN, "No sensors available, freeze protection unavailable!");
  }
});

static Task emergencyTask("Emergency mode", [](void*) {
  // flags
  uint8_t emergencyFlags = 0b00000000;

  // set outdoor sensor flag
  if (settings.equitherm.enabled) {
    if (!Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::OUTDOOR_TEMP)) {
      emergencyFlags |= 0b00000001;
    }
  }

  // set indoor sensor flags
  if (!Sensors::existsConnectedSensorsByPurpose(Sensors::Purpose::INDOOR_TEMP)) {
    if (!settings.equitherm.enabled && settings.pid.enabled) {
      emergencyFlags |= 0b00000010;
    }

    if (settings.opentherm.options.nativeOTC) {
      emergencyFlags |= 0b00000100;
    }
  }

  // if any flags is true
  if ((emergencyFlags & 0b00001111) != 0) {
    if (!emergencyDetected) {
      // flip flag
      emergencyDetected = true;
      emergencyFlipTime = millis();

    } else if (emergencyDetected && !vars.emergency.state) {
      // enable emergency
      if (millis() - emergencyFlipTime > (settings.emergency.tresholdTime * 1000)) {
        vars.emergency.state = true;
        ESP_LOGI(L_MAIN, "Emergency mode enabled (%hhu)", emergencyFlags);
      }
    }

  } else {
    if (emergencyDetected) {
      // flip flag
      emergencyDetected = false;
      emergencyFlipTime = millis();

    } else if (!emergencyDetected && vars.emergency.state) {
      // disable emergency
      if (millis() - emergencyFlipTime > (settings.emergency.tresholdTime * 1000)) {
        vars.emergency.state = false;
        ESP_LOGI(L_MAIN, "Emergency mode disabled");
      }
    }
  }
});

static Task overheatProtectionTask("Overheat protection", [](void*) {
  // Heating overheat control
  if (settings.heating.overheatProtection.highTemp > 0 && settings.heating.overheatProtection.lowTemp > 0) {
    const float highTemp = std::max({
      vars.slave.heating.currentTemp,
      vars.slave.heating.returnTemp,
      vars.slave.heatExchangerTemp
    });

    if (vars.master.heating.overheat) {
      if (static_cast<float>(settings.heating.overheatProtection.lowTemp) - highTemp + 0.0001f >= 0.0f) {
        vars.master.heating.overheat = false;

        ESP_LOGI(
          L_OT_HEATING, "Overheating not detected. Current high temp: %.2f, threshold (low): %hhu",
          highTemp, settings.heating.overheatProtection.lowTemp
        );
      }

    } else if (vars.slave.heating.active) {
      if (highTemp - static_cast<float>(settings.heating.overheatProtection.highTemp) + 0.0001f >= 0.0f) {
        vars.master.heating.overheat = true;

        ESP_LOGW(
          L_OT_HEATING, "Overheating detected! Current high temp: %.2f, threshold (high): %hhu",
          highTemp, settings.heating.overheatProtection.highTemp
        );
      }
    }

  } else if (vars.master.heating.overheat) {
    vars.master.heating.overheat = false;
  }

  // DHW overheat control
  if (settings.dhw.overheatProtection.highTemp > 0 && settings.dhw.overheatProtection.lowTemp > 0) {
    const float highTemp = std::max({
      vars.slave.heating.currentTemp,
      vars.slave.heating.returnTemp,
      vars.slave.heatExchangerTemp,
      vars.slave.dhw.currentTemp,
      vars.slave.dhw.currentTemp2,
      vars.slave.dhw.returnTemp
    });

    if (vars.master.dhw.overheat) {
      if (static_cast<float>(settings.dhw.overheatProtection.lowTemp) - highTemp + 0.0001f >= 0.0f) {
        vars.master.dhw.overheat = false;

        ESP_LOGI(
          L_OT_DHW, "Overheating not detected. Current high temp: %.2f, threshold (low): %hhu",
          highTemp, settings.dhw.overheatProtection.lowTemp
        );
      }

    } else if (vars.slave.dhw.active) {
      if (highTemp - static_cast<float>(settings.dhw.overheatProtection.highTemp) + 0.0001f >= 0.0f) {
        vars.master.dhw.overheat = true;

        ESP_LOGW(
          L_OT_DHW, "Overheating detected! Current high temp: %.2f, threshold (high): %hhu",
          highTemp, settings.dhw.overheatProtection.highTemp
        );
      }
    }

  } else if (vars.master.dhw.overheat) {
    vars.master.dhw.overheat = false;
  }
});

static Task cascadeTask("Cascade", [](void*) {
  static uint8_t configuredInputGpio = GPIO_IS_NOT_CONFIGURED;
  static uint8_t configuredOutputGpio = GPIO_IS_NOT_CONFIGURED;
  static bool pendingInputState = false;
  static unsigned long inputChangedTime = 0;
  static bool pendingOutputState = false;
  static unsigned long outputChangedTime = 0;

  // input
  if (settings.cascadeControl.input.enabled) {
    if (settings.cascadeControl.input.gpio != configuredInputGpio) {
      if (configuredInputGpio != GPIO_IS_NOT_CONFIGURED) {
        // Keep the old input high-impedance, including input-only GPIOs.
        pinMode(configuredInputGpio, INPUT);

        ESP_LOGI(L_CASCADE_INPUT, "Deinitialized on GPIO %hhu", configuredInputGpio);
      }

      if (GPIO_IS_VALID_INPUT(settings.cascadeControl.input.gpio)) {
        configuredInputGpio = settings.cascadeControl.input.gpio;
        pinMode(configuredInputGpio, INPUT);

        ESP_LOGI(L_CASCADE_INPUT, "Initialized on GPIO %hhu", configuredInputGpio);

      } else if (configuredInputGpio != GPIO_IS_NOT_CONFIGURED) {
        configuredInputGpio = GPIO_IS_NOT_CONFIGURED;

        ESP_LOGW(L_CASCADE_INPUT, "Failed initialize: GPIO %hhu is not valid!", configuredInputGpio);
      }
    }

    if (configuredInputGpio != GPIO_IS_NOT_CONFIGURED) {
      bool value = (digitalRead(configuredInputGpio) == HIGH) ^ settings.cascadeControl.input.invertState;

      if (value != vars.cascadeControl.input) {
        if (value != pendingInputState) {
          pendingInputState = value;
          inputChangedTime = millis();

        } else if (millis() - inputChangedTime >= settings.cascadeControl.input.thresholdTime * 1000u) {
          vars.cascadeControl.input = value;

          ESP_LOGI(
            L_CASCADE_INPUT,
            "State changed to %s",
            value ? "TRUE" : "FALSE"
          );
        }

      } else if (value != pendingInputState) {
        pendingInputState = value;
      }
    }
  }

  if (!settings.cascadeControl.input.enabled || configuredInputGpio == GPIO_IS_NOT_CONFIGURED) {
    if (!vars.cascadeControl.input) {
      vars.cascadeControl.input = true;

      ESP_LOGI(
        L_CASCADE_INPUT,
        "Disabled, state changed to %s",
        vars.cascadeControl.input ? "TRUE" : "FALSE"
      );
    }
  }


  // output
  if (settings.cascadeControl.output.gpio != configuredOutputGpio) {
    if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
      digitalWrite(configuredOutputGpio, LOW);
      pinMode(configuredOutputGpio, DISABLED);

      ESP_LOGI(L_CASCADE_OUTPUT, "Deinitialized on GPIO %hhu", configuredOutputGpio);
    }

    if (GPIO_IS_VALID_OUTPUT(settings.cascadeControl.output.gpio)) {
      configuredOutputGpio = settings.cascadeControl.output.gpio;
      pinMode(configuredOutputGpio, OUTPUT);

      ESP_LOGI(L_CASCADE_OUTPUT, "Initialized on GPIO %hhu", configuredOutputGpio);

    } else if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
      configuredOutputGpio = GPIO_IS_NOT_CONFIGURED;

      ESP_LOGW(L_CASCADE_OUTPUT, "Failed initialize: GPIO %hhu is not valid!", configuredOutputGpio);
    }
  }

  if (settings.cascadeControl.output.enabled) {
    if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
      bool value = false;
      if (settings.cascadeControl.output.onFault && vars.slave.fault.active) {
        value = true;

      } else if (settings.cascadeControl.output.onLossConnection && !vars.slave.connected) {
        value = true;

      } else if (settings.cascadeControl.output.onEnabledHeating && settings.heating.enabled && vars.cascadeControl.input) {
        value = true;
      }

      if (value != vars.cascadeControl.output) {
        if (value != pendingOutputState) {
          pendingOutputState = value;
          outputChangedTime = millis();

        } else if (millis() - outputChangedTime >= settings.cascadeControl.output.thresholdTime * 1000u) {
          vars.cascadeControl.output = value;

          ESP_LOGI(
            L_CASCADE_OUTPUT,
            "State changed to %s",
            value ? "TRUE" : "FALSE"
          );
        }

      } else if (value != pendingOutputState) {
        pendingOutputState = value;
      }
    }
  }

  if (!settings.cascadeControl.output.enabled || configuredOutputGpio == GPIO_IS_NOT_CONFIGURED) {
    if (vars.cascadeControl.output) {
      vars.cascadeControl.output = false;

      ESP_LOGI(
        L_CASCADE_OUTPUT,
        "Disabled, state changed to %s",
        vars.cascadeControl.output ? "TRUE" : "FALSE"
      );
    }
  }

  if (configuredOutputGpio != GPIO_IS_NOT_CONFIGURED) {
    digitalWrite(
      configuredOutputGpio,
      (vars.cascadeControl.output ^ settings.cascadeControl.output.invertState) ? HIGH : LOW
    );
  }
});

static Task externalPumpTask("External pump", [](void*) {
  static uint8_t configuredGpio = GPIO_IS_NOT_CONFIGURED;

  if (settings.externalPump.gpio != configuredGpio) {
    if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
      digitalWrite(configuredGpio, LOW);
      pinMode(configuredGpio, DISABLED);
    }

    if (GPIO_IS_VALID_OUTPUT(settings.externalPump.gpio)) {
      configuredGpio = settings.externalPump.gpio;
      pinMode(configuredGpio, OUTPUT);

    } else if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
      configuredGpio = GPIO_IS_NOT_CONFIGURED;
    }
  }

  if (configuredGpio == GPIO_IS_NOT_CONFIGURED) {
    if (vars.externalPump.state) {
      vars.externalPump.state = false;
      vars.externalPump.lastEnabledTime = millis();

      ESP_LOGI(L_EXTPUMP, "Disabled: use = off");
    }

    return;
  }

  if (!vars.master.heating.enabled && heatingEnabled) {
    heatingEnabled = false;
    heatingDisabledTime = millis();

  } else if (vars.master.heating.enabled && !heatingEnabled) {
    heatingEnabled = true;
  }

  if (!settings.externalPump.use) {
    if (vars.externalPump.state) {
      vars.externalPump.state = false;
      vars.externalPump.lastEnabledTime = millis();

      ESP_LOGI(L_EXTPUMP, "Disabled: use = off");
    }

  } else if (vars.externalPump.state && !heatingEnabled) {
    if (extPumpStartReason == PumpStartReason::HEATING && millis() - heatingDisabledTime > (settings.externalPump.postCirculationTime * 1000u)) {
      vars.externalPump.state = false;
      vars.externalPump.lastEnabledTime = millis();

      ESP_LOGI(L_EXTPUMP, "Disabled: expired post circulation time");

    } else if (extPumpStartReason == PumpStartReason::ANTISTUCK && millis() - externalPumpStartTime >= (settings.externalPump.antiStuckTime * 1000u)) {
      vars.externalPump.state = false;
      vars.externalPump.lastEnabledTime = millis();

      ESP_LOGI(L_EXTPUMP, "Disabled: expired anti stuck time");
    }

  } else if (vars.externalPump.state && heatingEnabled && extPumpStartReason == PumpStartReason::ANTISTUCK) {
    extPumpStartReason = PumpStartReason::HEATING;

  } else if (!vars.externalPump.state && heatingEnabled) {
    vars.externalPump.state = true;
    externalPumpStartTime = millis();
    extPumpStartReason = PumpStartReason::HEATING;

    ESP_LOGI(L_EXTPUMP, "Enabled: heating on");

  } else if (!vars.externalPump.state && (vars.externalPump.lastEnabledTime == 0 || millis() - vars.externalPump.lastEnabledTime >= (settings.externalPump.antiStuckInterval * 1000lu))) {
    vars.externalPump.state = true;
    externalPumpStartTime = millis();
    extPumpStartReason = PumpStartReason::ANTISTUCK;

    ESP_LOGI(L_EXTPUMP, "Enabled: anti stuck");
  }

  digitalWrite(
    configuredGpio,
    (vars.externalPump.state ^ settings.externalPump.invertState) ? HIGH : LOW
  );
});

void initControl() {
  freezeProtectionTask.setInterval(1000);
  coreTaskManager.addTask(freezeProtectionTask);

  emergencyTask.setInterval(1000);
  coreTaskManager.addTask(emergencyTask);

  // The OpenTherm loop previously skipped overheat control while disconnected.
  overheatProtectionTask.setEnabledWhen([] {
    return vars.slave.connected;
  });
  overheatProtectionTask.setInterval(1000);
  coreTaskManager.addTask(overheatProtectionTask);

  cascadeTask.setInterval(1000);
  coreTaskManager.addTask(cascadeTask);

  externalPumpTask.setInterval(1000);
  coreTaskManager.addTask(externalPumpTask);
}
