class BufferedWebServer {
public:
  BufferedWebServer(WebServer* webServer, size_t bufferSize = 64) {
    this->webServer = webServer;
    this->bufferSize = bufferSize;
    this->buffer = (uint8_t*)malloc(bufferSize * sizeof(*this->buffer));
  }

  ~BufferedWebServer() {
    free(this->buffer);
  }

  void send(int code, const char* contentType, JsonDocument& content, bool pretty = false) {
    #ifdef ARDUINO_ARCH_ESP8266
    if (!this->webServer->chunkedResponseModeStart(code, contentType)) {
      this->webServer->send(505, F("text/html"), F("HTTP1.1 required"));
      return;
    }

    this->webServer->setContentLength(measureJson(content));
    #else
    this->webServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
    this->webServer->sendHeader(F("Content-Length"), String(measureJson(content)));
    this->webServer->send(code, contentType, emptyString);
    #endif

    if (pretty) {
      serializeJsonPretty(content, *this);
      
    } else {
      serializeJson(content, *this);
    }

    this->flush();

    #ifdef ARDUINO_ARCH_ESP8266
    this->webServer->chunkedResponseFinalize();
    #else
    this->webServer->sendContent(emptyString);
    #endif
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

  void flush() {
    if (this->bufferPos == 0) {
      return;
    }

    this->webServer->sendContent((const char*)this->buffer, this->bufferPos);
    this->bufferPos = 0;
    #ifdef ARDUINO_ARCH_ESP8266
    ::delay(0);
    #endif
  }

protected:
  WebServer* webServer = nullptr;
  uint8_t* buffer;
  size_t bufferSize = 64;
  size_t bufferPos = 0;
};