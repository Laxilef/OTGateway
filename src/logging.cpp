#include <app.h>

#include <MycilaWebSerial.h>
#include <esp_log_color.h>
#include <esp_private/log_format.h>
#include <esp_private/log_timestamp.h>
#include <esp_rom_sys.h>
#include <freertos/ringbuf.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static_assert(CONFIG_LOG_VERSION == 2, "OTGateway requires an SDK built with Log v2");
static_assert(CONFIG_LOG_DYNAMIC_LEVEL_CONTROL, "Runtime log levels require dynamic level control");

static std::atomic<RingbufHandle_t> loggingQueue{nullptr};
static std::atomic<bool> activeSerial{false};
static std::atomic<bool> activeWebSerial{false};
static thread_local bool writingOutputs = false;

// Return the full byte count, like snprintf, so only long records need a second pass.
static size_t formatLogRecord(esp_log_msg_t& message, char* buffer, size_t capacity) {
  const auto& options = message.config.opts;
  size_t prefixLength = 0;
  const char* suffix = "";

  if (options.require_formatting) {
    const auto level = options.log_level;
    if (level >= ESP_LOG_MAX) {
      return 0;
    }

    const char* color = "";
    #if ESP_LOG_SUPPORT_COLOR && !ESP_LOG_COLOR_DISABLED
    static const char* const colors[ESP_LOG_MAX] = {
      "", LOG_COLOR_E, LOG_COLOR_W, LOG_COLOR_I, LOG_COLOR(LOG_COLOR_CYAN), LOG_COLOR(LOG_COLOR_MAGENTA)
    };
    if (!options.dis_color) {
      color = colors[level];
    }
    #endif

    char timestamp[32] = {};
    if (!options.dis_timestamp) {
      esp_log_timestamp_str(false, message.timestamp, timestamp);
    }

    int written = snprintf(
      buffer, capacity, "%s%s%s%s[%c]%s%s%s ",
      options.dis_timestamp ? "" : "[",
      timestamp,
      options.dis_timestamp ? "" : "]",
      color,
      "\0EWIDV"[level],
      message.tag ? "[" : "",
      message.tag ? message.tag : "",
      message.tag ? "]" : ""
    );
    if (written < 0) {
      return 0;
    }

    prefixLength = written;
    suffix = *color ? LOG_RESET_COLOR "\n" : "\n";
  }

  size_t offset = std::min(prefixLength, capacity - 1);
  va_list args;
  va_copy(args, message.args);
  int bodyLength = vsnprintf(buffer + offset, capacity - offset, message.format, args);
  va_end(args);
  if (bodyLength < 0) {
    return 0;
  }

  size_t suffixOffset = prefixLength + bodyLength;
  size_t length = suffixOffset + strlen(suffix);
  if (length < capacity) {
    memcpy(buffer + suffixOffset, suffix, length - suffixOffset + 1);
  }
  return length;
}

static void __attribute__((noinline)) enqueueLogRecord(RingbufHandle_t queue, esp_log_msg_t& message) {
  char local[128];
  size_t length = formatLogRecord(message, local, sizeof(local));
  if (length == 0 || length >= xRingbufferGetMaxItemSize(queue)) {
    return;
  }

  if (length < sizeof(local)) {
    xRingbufferSend(queue, local, length + 1, 0);
    return;
  }

  char* buffer = static_cast<char*>(malloc(length + 1));
  if (buffer != nullptr) {
    if (formatLogRecord(message, buffer, length + 1) == length) {
      xRingbufferSend(queue, buffer, length + 1, 0);
    }

    free(buffer);
  }
}

extern "C" {
  void __real_esp_log_format(esp_log_msg_t* message);

  void IRAM_ATTR __wrap_esp_log_format(esp_log_msg_t* message) {
    RingbufHandle_t queue = loggingQueue.load(std::memory_order_acquire);
    if (message->config.opts.constrained_env || writingOutputs || queue == nullptr) {
      __real_esp_log_format(message);
      return;
    }

    const bool hasOutput = activeSerial.load(std::memory_order_relaxed) ||
                           activeWebSerial.load(std::memory_order_relaxed);

    if (hasOutput) {
      enqueueLogRecord(queue, *message);
    }
  }
}

static void writeOutput(Print& output, const uint8_t* buffer, size_t size) {
  while (size > 0) {
    size_t written = output.write(buffer, size);
    if (written == 0 || written > size) {
      return;
    }

    buffer += written;
    size -= written;
  }
}

static void setLoggingLevel(esp_log_level_t level) {
  esp_log_level_set("*", level);

  // prevents loop with WebSerial in case we reach some error logs in esp_async_ws like WS message queue overflow
  esp_log_level_set("async_tcp", ESP_LOG_NONE);
  esp_log_level_set("async_ws", ESP_LOG_NONE);
}

void drainLoggingSerial() {
  if (activeSerial.load(std::memory_order_relaxed)) {
    // Drain only the current RX backlog; continuous input must not starve tasks.
    int available = Serial.available();
    while (available-- > 0) {
      Serial.read();
    }
  }
}

static Task sendLogsTask("Send logs", [](void*) {
  // Bound each pass so continuous logging cannot starve System tasks.
  size_t remaining = LOGGING_QUEUE_SIZE;
  const unsigned long started = millis();
  RingbufHandle_t queue = loggingQueue.load(std::memory_order_acquire);

  while (remaining > 0 && millis() - started < 10) {
    size_t size = 0;
    auto* buffer = static_cast<uint8_t*>(xRingbufferReceive(queue, &size, 0));

    if (buffer == nullptr) {
      break;
    }

    // A transport may itself log during a write on this task.
    writingOutputs = true;
    if (activeSerial.load(std::memory_order_relaxed)) {
      writeOutput(Serial, buffer, size - 1);
    }

    if (activeWebSerial.load(std::memory_order_relaxed) && webSerial != nullptr) {
      if (webSerial->getConnectionCount() > 0) {
        writeOutput(*webSerial, buffer, size - 1);
      }
    }
    writingOutputs = false;

    vRingbufferReturnItem(queue, buffer);
    remaining = size >= remaining ? 0 : remaining - size;
  }
});

Task configureLoggingTask("Configure logging", Task::Type::ONCE, [](void*) {
  static unsigned int currentSerialBaudrate = 0;
  unsigned int newSerialBaudrate = settings.system.serial.enabled ? settings.system.serial.baudrate : 0;
  if (currentSerialBaudrate != newSerialBaudrate) {
    if (newSerialBaudrate == 0) {
      Serial.end();
    }

    if (newSerialBaudrate != 0) {
      #if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
      Serial.setTxBufferSize(512);
      Serial.setTxTimeoutMs(0);
      #endif

      Serial.begin(newSerialBaudrate);
    }

    currentSerialBaudrate = newSerialBaudrate;
  }

  activeSerial.store(currentSerialBaudrate != 0, std::memory_order_relaxed);
  activeWebSerial.store(settings.system.webSerial.enabled && webSerial != nullptr, std::memory_order_relaxed);

  setLoggingLevel(settings.system.logLevel);
});

bool initLogging() {
  if (loggingQueue.load(std::memory_order_relaxed) != nullptr) {
    return false;
  }

  RingbufHandle_t queue = xRingbufferCreate(LOGGING_QUEUE_SIZE, RINGBUF_TYPE_NOSPLIT);
  if (queue == nullptr) {
    return false;
  }

  #if !CONFIG_LOG_API_CONSTRAINED_ENV_SAFE
  // The native formatter uses the vprintf hook even in an ISR on these SDK builds.
  esp_log_set_vprintf(esp_rom_vprintf);
  #endif

  configureLoggingTask.forceRun();
  loggingQueue.store(queue, std::memory_order_release);
  systemTaskManager.addTask(configureLoggingTask);

  sendLogsTask.setInterval(10);
  systemTaskManager.addTask(sendLogsTask);

  return true;
}
