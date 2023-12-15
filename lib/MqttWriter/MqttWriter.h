#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#ifdef ARDUINO_ARCH_ESP32
#include <mutex>
#endif


class MqttWriter {
public:
  MqttWriter(PubSubClient* client, size_t bufferSize = 64) {
    this->client = client;
    this->bufferSize = bufferSize;
    this->buffer = (uint8_t*) malloc(bufferSize * sizeof(*this->buffer));

#ifdef ARDUINO_ARCH_ESP32
    this->mutex = new std::mutex();
#endif
  }

  ~MqttWriter() {
    free(this->buffer);

#ifdef ARDUINO_ARCH_ESP32
    delete this->mutex;
#endif
  }

  void setYieldCallback(std::function<void()> callback) {
    this->yieldCallback = callback;
  }

  void setYieldCallback() {
    this->yieldCallback = nullptr;
  }

  void setEventPublishCallback(std::function<void(const char*, size_t, size_t, bool)> callback) {
    this->eventPublishCallback = callback;
  }

  void setEventPublishCallback() {
    this->eventPublishCallback = nullptr;
  }

  void setEventFlushCallback(std::function<void(size_t, size_t)> callback) {
    this->eventFlushCallback = callback;
  }

  void setEventFlushCallback() {
    this->eventFlushCallback = nullptr;
  }

  bool lock() {
#ifdef ARDUINO_ARCH_ESP32
    if (!this->mutex->try_lock()) {
      return false;
    }
#else
    if (this->isLocked()) {
      return false;
    }
#endif

    this->locked = true;
    this->writeAfterLock = 0;
    this->lockedTime = millis();

    return true;
  }

  bool isLocked() {
    return this->locked;
  }

  void unlock() {
    this->locked = false;
#if defined(ARDUINO_ARCH_ESP32)
    this->mutex->unlock();
#endif
  }

  bool publish(const char* topic, JsonDocument& doc, bool retained = false) {
    if (!this->client->connected()) {
      this->bufferPos = 0;
      return false;
    }

    while (!this->lock()) {
      if (this->yieldCallback) {
        this->yieldCallback();
      }
    }

    this->bufferPos = 0;
    size_t docSize = measureJson(doc);
    size_t written = 0;
    if (this->client->beginPublish(topic, docSize, retained)) {
      serializeJson(doc, *this);
      doc.clear();
      this->flush();
      
      written = this->writeAfterLock;
    }
    this->unlock();

    if (this->eventPublishCallback) {
      this->eventPublishCallback(topic, written, docSize, written == docSize);
    }

    return written == docSize;
  }

  bool publish(const char* topic, const char* buffer, bool retained = false) {
    return this->publish(topic, (uint8_t*) buffer, strlen(buffer), retained);
  }

  bool publish(const char* topic, const uint8_t* buffer, size_t length, bool retained = false) {
    if (!this->client->connected()) {
      this->bufferPos = 0;
      return false;
    }

    while (!this->lock()) {
      if (this->yieldCallback) {
        this->yieldCallback();
      }
    }

    this->bufferPos = 0;
    size_t written = 0;
    bool result = false;
    if (length == 0) {
      result = this->client->publish(topic, nullptr, 0, retained);

    } else if (this->client->beginPublish(topic, length, retained)) {
      this->write(buffer, length);
      this->flush();
      
      written = this->writeAfterLock;
      result = written == length;
    }
    this->unlock();

    if (this->eventPublishCallback) {
      this->eventPublishCallback(topic, written, length, result);
    }

    return result;
  }

  size_t write(uint8_t c) {
    this->buffer[this->bufferPos++] = c;

    if (this->bufferPos >= this->bufferSize) {
      this->flush();
    }

    return 1;
  }

  size_t write(const uint8_t* buffer, size_t length) {
    size_t written = 0;
    while (written < length) {
      size_t copySize = this->bufferSize - this->bufferPos;
      if (written + copySize > length) {
        copySize = length - written;
      }

      memcpy(this->buffer + this->bufferPos, buffer + written, copySize);
      this->bufferPos += copySize;

      if (this->bufferPos >= this->bufferSize) {
        this->flush();
      }

      written += copySize;
    }

    return written;
  }

  bool flush() {
    if (this->bufferPos == 0) {
      return false;
    }

    if (!this->client->connected()) {
      this->bufferPos = 0;
    }

    size_t length = this->bufferPos;
    size_t written = this->client->write(this->buffer, length);
    this->client->flush();
    this->bufferPos = 0;

    if (this->isLocked()) {
      this->writeAfterLock += written;
    }

    if (this->eventFlushCallback) {
      this->eventFlushCallback(written, length);
    }

    return written == length;
  }

protected:
  PubSubClient* client;
  uint8_t* buffer;
  size_t bufferSize = 64;
  size_t bufferPos = 0;
  bool locked = false;
#ifdef ARDUINO_ARCH_ESP32
  mutable std::mutex* mutex;
#endif
  unsigned long lockedTime = 0;
  size_t writeAfterLock = 0;
  std::function<void()> yieldCallback = nullptr;
  std::function<void(const char*, size_t, size_t, bool)> eventPublishCallback = nullptr;
  std::function<void(size_t, size_t)> eventFlushCallback = nullptr;
};
