#pragma once

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <stdint.h>

#include <mutex>

struct HeapMetrics : multi_heap_info_t {
  size_t total;
  size_t minLargestBlock = SIZE_MAX;
  uint8_t fragmentation;
};

struct MemoryMetrics {
  bool captured;
  uint64_t uptime;
  HeapMetrics heap;
  HeapMetrics psram;
};

struct CrashMetrics {
  bool captured;
  uint32_t core;
  uint64_t uptime;
};

struct CoreDumpBuildInfo {
  char version[sizeof(BUILD_VERSION)] = BUILD_VERSION;
  char date[sizeof("Mmm dd yyyy hh:mm:ss")];
  char commit[sizeof(BUILD_COMMIT)] = BUILD_COMMIT;
  char env[sizeof(BUILD_ENV)] = BUILD_ENV;
  char core[sizeof(ESP_ARDUINO_VERSION_STR)];
  char sdk[64];
  char chipModel[16];
  uint16_t chipRevision;
  uint8_t chipCores;
  uint32_t cpuFreqMHz;
  uint32_t flashChipSize;
};

// Keep one shared instance of each record in the native core dump.
inline COREDUMP_DRAM_ATTR MemoryMetrics memoryMetrics __attribute__((used)) = {};
inline COREDUMP_DRAM_ATTR CrashMetrics crashMetrics __attribute__((used)) = {};
inline COREDUMP_DRAM_ATTR CoreDumpBuildInfo coreDumpBuildInfo __attribute__((used)) = {};
inline std::mutex memoryMetricsMutex;

// Refresh and copy under the same lock: System and HTTP run on different tasks.
inline MemoryMetrics getMemoryMetrics() {
  std::lock_guard<std::mutex> lock(memoryMetricsMutex);

  // A panic during the update must not report partially written statistics.
  __atomic_store_n(&memoryMetrics.captured, false, __ATOMIC_SEQ_CST);
  heap_caps_get_info(&memoryMetrics.heap, MALLOC_CAP_INTERNAL);

  if (memoryMetrics.psram.total) {
    heap_caps_get_info(&memoryMetrics.psram, MALLOC_CAP_SPIRAM);
  }

  if (memoryMetrics.heap.largest_free_block < memoryMetrics.heap.minLargestBlock) {
    memoryMetrics.heap.minLargestBlock = memoryMetrics.heap.largest_free_block;
  }

  if (memoryMetrics.psram.largest_free_block < memoryMetrics.psram.minLargestBlock) {
    memoryMetrics.psram.minLargestBlock = memoryMetrics.psram.largest_free_block;
  }

  memoryMetrics.heap.fragmentation = memoryMetrics.heap.total_free_bytes
    ? 100 - memoryMetrics.heap.largest_free_block * 100.0 / memoryMetrics.heap.total_free_bytes
    : 0;
  memoryMetrics.psram.fragmentation = memoryMetrics.psram.total_free_bytes
    ? 100 - memoryMetrics.psram.largest_free_block * 100.0 / memoryMetrics.psram.total_free_bytes
    : 0;
  memoryMetrics.uptime = esp_timer_get_time() / 1000;
  __atomic_store_n(&memoryMetrics.captured, true, __ATOMIC_RELEASE);
  return memoryMetrics;
}

static void IRAM_ATTR captureCoreDumpMetrics(arduino_panic_info_t* info, void*) {
  crashMetrics.core = info->core;
  crashMetrics.uptime = esp_timer_get_time() / 1000;
  __atomic_store_n(&crashMetrics.captured, true, __ATOMIC_RELEASE);
}

static inline void initCoreDumpMetrics() {
  // Keep the timestamp local to the calling translation unit, outside shared types.
  strlcpy(coreDumpBuildInfo.date, __DATE__ " " __TIME__, sizeof(coreDumpBuildInfo.date));
  strlcpy(coreDumpBuildInfo.core, ESP.getCoreVersion(), sizeof(coreDumpBuildInfo.core));
  strlcpy(coreDumpBuildInfo.sdk, ESP.getSdkVersion(), sizeof(coreDumpBuildInfo.sdk));
  strlcpy(coreDumpBuildInfo.chipModel, ESP.getChipModel(), sizeof(coreDumpBuildInfo.chipModel));

  coreDumpBuildInfo.chipRevision = ESP.getChipRevision();
  coreDumpBuildInfo.chipCores = ESP.getChipCores();
  coreDumpBuildInfo.cpuFreqMHz = ESP.getCpuFreqMHz();
  coreDumpBuildInfo.flashChipSize = ESP.getFlashChipSize();

  // Arduino has registered the memory regions before this one-time initialization.
  memoryMetrics.heap.total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  memoryMetrics.psram.total = psramFound() ? heap_caps_get_total_size(MALLOC_CAP_SPIRAM) : 0;

  getMemoryMetrics();
  set_arduino_panic_handler(captureCoreDumpMetrics, nullptr);
}
