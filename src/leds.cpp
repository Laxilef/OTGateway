#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>
#include <Blinker.h>

static Blinker blinker;

static Task ledStatusTask("LED status", [](void*) {
  uint8_t errors[4];
  uint8_t errCount = 0;
  static uint8_t errPos = 0;
  static unsigned long endBlinkTime = 0;
  static bool ledOn = false;
  static uint8_t configuredGpio = GPIO_IS_NOT_CONFIGURED;

  if (settings.system.statusLedGpio != configuredGpio) {
    if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
      digitalWrite(configuredGpio, LOW);
      pinMode(configuredGpio, DISABLED);
    }

    if (GPIO_IS_VALID_OUTPUT(settings.system.statusLedGpio)) {
      configuredGpio = settings.system.statusLedGpio;
      pinMode(configuredGpio, OUTPUT);
      digitalWrite(configuredGpio, LOW);
      blinker.init(configuredGpio);

    } else if (configuredGpio != GPIO_IS_NOT_CONFIGURED) {
      configuredGpio = GPIO_IS_NOT_CONFIGURED;
    }
  }

  if (configuredGpio == GPIO_IS_NOT_CONFIGURED) {
    return;
  }

  if (!isNetworkConnected()) {
    errors[errCount++] = 2;
  }

  if (!vars.slave.connected) {
    errors[errCount++] = 3;
  }

  if (vars.slave.fault.active) {
    errors[errCount++] = 4;
  }

  if (vars.emergency.state) {
    errors[errCount++] = 5;
  }


  if (blinker.ready()) {
    endBlinkTime = millis();
  }

  if (!blinker.running() && millis() - endBlinkTime >= 5000) {
    if (errCount == 0) {
      if (!ledOn) {
        digitalWrite(configuredGpio, HIGH);
        ledOn = true;
      }

      return;

    } else if (ledOn) {
      digitalWrite(configuredGpio, LOW);
      ledOn = false;
      endBlinkTime = millis();
      return;
    }

    if (errPos >= errCount) {
      errPos = 0;

      // end of error list
      blinker.blink(10, 50, 50);

    } else {
      blinker.blink(errors[errPos++], 300, 300);
    }
  }

  blinker.tick();
});

void initLeds() {
  ledStatusTask.setInterval(100);
  coreTaskManager.addTask(ledStatusTask);
}
