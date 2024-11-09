#pragma once
#include <Arduino.h>
#include <MqttClient.h>
#ifdef ARDUINO_ARCH_ESP32
#include <mutex>
#endif


class MqttWriter {
public:
  typedef std::function<void()> YieldCallback;
  typedef std::function<void(const char*, size_t, size_t, bool)> PublishEventCallback;
  typedef std::function<void(size_t, size_t)> FlushEventCallback;

  MqttWriter(MqttClient* client, size_t bufferSize = 64) {
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

  MqttWriter* setYieldCallback(YieldCallback callback = nullptr) {
    this->yieldCallback = callback;

    return this;
  }

  MqttWriter* setPublishEventCallback(PublishEventCallback callback) {
    this->publishEventCallback = callback;

    return this;
  }

  MqttWriter* setFlushEventCallback(FlushEventCallback callback) {
    this->flushEventCallback = callback;

    return this;
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

  bool publish(const char* topic, const JsonVariantConst doc, bool retained = false) {
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
    if (this->client->beginMessage(topic, docSize, retained)) {
      serializeJson(doc, *this);
      this->flush();
      this->client->endMessage();
      
      written = this->writeAfterLock;
    }
    this->unlock();

    if (this->publishEventCallback) {
      this->publishEventCallback(topic, written, docSize, written == docSize);
    }

    return written == docSize;
  }

  bool publish(const char* topic, const char* buffer, bool retained = false) {
    return this->publish(topic, (const uint8_t*) buffer, strlen(buffer), retained);
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
    if (!length || buffer == nullptr) {
      result = this->client->beginMessage(topic, retained) && this->client->endMessage();

    } else if (this->client->beginMessage(topic, length, retained)) {
      this->write(buffer, length);
      this->flush();
      this->client->endMessage();
      
      written = this->writeAfterLock;
      result = written == length;
    }
    this->unlock();

    if (this->publishEventCallback) {
      this->publishEventCallback(topic, written, length, result);
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

    if (this->flushEventCallback) {
      this->flushEventCallback(written, length);
    }

    return written == length;
  }

protected:
  YieldCallback yieldCallback;
  PublishEventCallback publishEventCallback;
  FlushEventCallback flushEventCallback;
  MqttClient* client;
  uint8_t* buffer;
  size_t bufferSize = 64;
  size_t bufferPos = 0;
  bool locked = false;
#ifdef ARDUINO_ARCH_ESP32
  mutable std::mutex* mutex;
#endif
  unsigned long lockedTime = 0;
  size_t writeAfterLock = 0;
};
