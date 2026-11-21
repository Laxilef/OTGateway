#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <app.h>
#include <metrics.h>
#include <esp_system.h>


std::string getResetReason() {
  std::string value;

  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      value = "Reset due to power-on event";
      break;

    case ESP_RST_EXT:
      value = "Reset by external pin";
      break;

    case ESP_RST_SW:
      value = "Software reset via esp_restart";
      break;

    case ESP_RST_PANIC:
      value = "Software reset due to exception/panic";
      break;

    case ESP_RST_INT_WDT:
      value = "Reset (software or hardware) due to interrupt watchdog";
      break;

    case ESP_RST_TASK_WDT:
      value = "Reset due to task watchdog";
      break;

    case ESP_RST_WDT:
      value = "Reset due to other watchdogs";
      break;

    case ESP_RST_DEEPSLEEP:
      value = "Reset after exiting deep sleep mode";
      break;

    case ESP_RST_BROWNOUT:
      value = "Brownout reset (software or hardware)";
      break;

    case ESP_RST_SDIO:
      value = "Reset over SDIO";
      break;

    case ESP_RST_UNKNOWN:
    default:
      value = "unknown";
      break;

  }

  return value;
}

static unsigned long lastHeapInfo = 0;
static unsigned int minFreeHeap = 0;
static unsigned int minLargestFreeBlock = 0;

Task restartTask("Restart", Task::Type::ONCE, [](void*) {
  ESP_LOGI(L_MAIN, "Restart preparation started");
  coreTaskManager.pause();
  sensorsTaskManager.pause();
  openthermTaskManager.pause();

  // Finish application callbacks and onDone before saving, including with critical heap.
  coreTaskManager.waitForPaused();
  sensorsTaskManager.waitForPaused();
  openthermTaskManager.waitForPaused();
  saveSettingsNow();

  // Yield to FreeRTOS/IDF tasks before resetting the device.
  ::delay(250);
  ESP.restart();
});

// Maintenance
static Task maintenanceTask("System maintenance", [](void*) {
  getMemoryMetrics();

  drainLoggingSerial();
});

// Heap monitoring remains active while the core manager is paused.
static Task heapTask("Heap monitoring", [](void*) {
  const auto memory = getMemoryMetrics();
  unsigned int freeHeap = memory.heap.total_free_bytes;
  unsigned int maxFreeBlockHeap = memory.heap.largest_free_block;

  /*if (freeHeap < 2048 || maxFreeBlockHeap < 2048) {
    if (!vars.states.upgrading) {
      esp_system_abort("OTGateway: low internal heap");
    }
  }*/

  if (esp_log_level_get(L_MAIN) < ESP_LOG_VERBOSE) {
    return;
  }

  size_t currentMinFreeHeap = memory.heap.minimum_free_bytes;
  size_t minFreeHeapDiff = 0;
  if (currentMinFreeHeap < minFreeHeap || minFreeHeap == 0) {
    minFreeHeapDiff = minFreeHeap ? minFreeHeap - currentMinFreeHeap : 0;
    minFreeHeap = currentMinFreeHeap;
  }

  size_t currentMinLargestFreeBlock = memory.heap.minLargestBlock;
  size_t minLargestFreeBlockDiff = 0;
  if (currentMinLargestFreeBlock < minLargestFreeBlock || minLargestFreeBlock == 0) {
    minLargestFreeBlockDiff = minLargestFreeBlock ? minLargestFreeBlock - currentMinLargestFreeBlock : 0;
    minLargestFreeBlock = currentMinLargestFreeBlock;
  }

  if (millis() - lastHeapInfo > 20000 || minFreeHeapDiff > 0 || minLargestFreeBlockDiff > 0) {
    ESP_LOGV(
      L_MAIN,
      "Free heap size: %u of %u bytes (min: %u, diff: %u), max free block: %u (min: %u, diff: %u, frag: %hhu%%)",
      freeHeap, memory.heap.total, minFreeHeap, minFreeHeapDiff,
      maxFreeBlockHeap, minLargestFreeBlock, minLargestFreeBlockDiff, memory.heap.fragmentation
    );
    lastHeapInfo = millis();
  }
});

// Initialization
void initSystem() {
  systemTaskManager.addTask(restartTask);

  maintenanceTask.setInterval(100);
  systemTaskManager.addTask(maintenanceTask);

  heapTask.setInterval(100);
  systemTaskManager.addTask(heapTask);
}
