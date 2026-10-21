#pragma once
#include <Arduino.h>

#include "esp_err.h"

namespace CrashRecorder {
  typedef struct {
    unsigned int data[32];
    uint8_t length;
    bool continues;
  } backtrace_t;

  typedef struct {
    unsigned int data[4];
    uint8_t length;
  } epc_t;

  typedef struct {
    uint8_t core;
    size_t heap;
    unsigned long uptime;
  } ext_t;


  __NOINIT_ATTR volatile static backtrace_t backtrace; // NOLINT
  __NOINIT_ATTR volatile static epc_t epc; // NOLINT
  __NOINIT_ATTR volatile static ext_t ext; // NOLINT

  uint8_t backtraceMaxLength = sizeof(backtrace.data) / sizeof(*backtrace.data);
  uint8_t epcMaxLength = sizeof(epc.data) / sizeof(*epc.data);

  void IRAM_ATTR panicHandler(arduino_panic_info_t *info, void *arg) {;
    ext.core = info->core;
    ext.heap = ESP.getFreeHeap();
    ext.uptime = millis() / 1000u;

    // Backtrace
    backtrace.length = info->backtrace_len < backtraceMaxLength ? info->backtrace_len : backtraceMaxLength;
    backtrace.continues = false;
    for (unsigned int i = 0; i < info->backtrace_len; i++) {
      if (i >= backtraceMaxLength) {
        backtrace.continues = true;
        break;
      }

      backtrace.data[i] = info->backtrace[i];
    }

    // EPC
    if (info->pc) {
      epc.data[0] = (unsigned int) info->pc;
      epc.length = 1;

    } else {
      epc.length = 0;
    }
  }

  void init() {
    if (backtrace.length > backtraceMaxLength) {
      backtrace.length = 0;
    }

    if (epc.length > epcMaxLength) {
      epc.length = 0;
    }

    set_arduino_panic_handler(panicHandler, nullptr);
  }
}
