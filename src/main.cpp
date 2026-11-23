#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>
#include <metrics.h>
#include <esp_system.h>

#include <cstdlib>

#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
  #include <USB.h>
#endif

#ifndef ARDUINO_ARCH_ESP32
  #error Only ESP32 boards are supported
#endif

#include <LittleFS.h>

// Vars
Sensors::Result sensorsResults[SENSORS_AMOUNT];

TaskManager coreTaskManager("Core");
TaskManager systemTaskManager("System");
TaskManager openthermTaskManager("OpenTherm");
TaskManager sensorsTaskManager("Sensors");


extern "C" void app_main() {
  // https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/main.cpp
  #if defined(F_XTAL_MHZ) && !CONFIG_IDF_TARGET_ESP32S2
    rtc_clk_xtal_freq_update((rtc_xtal_freq_t)F_XTAL_MHZ);
    rtc_clk_cpu_freq_set_xtal();
  #endif

  #ifdef F_CPU
    setCpuFrequencyMhz(F_CPU / 1000000);
  #endif

  #if ARDUINO_USB_DFU_ON_BOOT && !ARDUINO_USB_MODE
    USB.enableDFU();
  #endif

  #if ARDUINO_USB_ON_BOOT && !ARDUINO_USB_MODE
    USB.begin();
  #endif

  initArduino();

  initCoreDumpMetrics();
  Sensors::setMaxSensors(SENSORS_AMOUNT);
  Sensors::settings = sensorsSettings;
  Sensors::results = sensorsResults;
  LittleFS.begin();

  if (!initLogging()) {
    std::abort();
  }

  initConfig();
  initNetwork();
  initSensors();
  initControl();
  initRegulator();
  initLeds();
  initMqtt();
  initPortal();
  initSystem();
  initOpenTherm();

  // RMT has its own worker; this manager schedules synchronous boiler operations.
  if (!openthermTaskManager.asyncStart(4096, 5, 0, 10, false)) {
    esp_system_abort("Failed to start OpenTherm task manager");
  }
  ESP_LOGI(L_MAIN, "Started %s task manager", openthermTaskManager.name());

  // Core runs control and MQTT tasks; poll their intervals every 100 ms.
  if (!coreTaskManager.asyncStart(4608, 4, tskNO_AFFINITY, 100, false)) {
    esp_system_abort("Failed to start Core task manager");
  }
  ESP_LOGI(L_MAIN, "Started %s task manager", coreTaskManager.name());

  // System keeps maintenance, networking and restart handling active during Core pauses.
  if (!systemTaskManager.asyncStart(5120, 3, tskNO_AFFINITY, 100, false)) {
    esp_system_abort("Failed to start System task manager");
  }
  ESP_LOGI(L_MAIN, "Started %s task manager", systemTaskManager.name());

  BaseType_t sensorsCore = tskNO_AFFINITY;
  #if USE_BLE && defined(CONFIG_BT_NIMBLE_PINNED_TO_CORE)
    sensorsCore = CONFIG_BT_NIMBLE_PINNED_TO_CORE;
  #endif

  // Keep physical sensor I/O and result processing outside the Core worker.
  if (!sensorsTaskManager.asyncStart(4096, 4, sensorsCore, 1000, false)) {
    esp_system_abort("Failed to start Sensors task manager");
  }
  ESP_LOGI(L_MAIN, "Started %s task manager", sensorsTaskManager.name());
}
