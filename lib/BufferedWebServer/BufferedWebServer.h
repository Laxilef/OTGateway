class BufferedWebServer {
public:
  explicit BufferedWebServer(WebServer* webServer, size_t bufferSize = 64) {
    this->webServer = webServer;
    this->bufferSize = bufferSize;
    this->buffer = static_cast<uint8_t*>(malloc(bufferSize * sizeof(*this->buffer)));
  }

  ~BufferedWebServer() {
    free(this->buffer);
  }

  template <class T>
  void send(int code, T contentType, const JsonVariantConst content, bool pretty = false) {
    auto contentLength = pretty ? measureJsonPretty(content) : measureJson(content);

    this->webServer->setContentLength(contentLength);
    this->webServer->send(code, contentType, emptyString);

    if (pretty) {
      serializeJsonPretty(content, *this);

    } else {
      serializeJson(content, *this);
    }

    this->flush();

    this->webServer->sendContent(emptyString);
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

    auto& client = this->webServer->client();
    if (client.connected()) {
      this->webServer->sendContent((const char*)this->buffer, this->bufferPos);
    }

    this->bufferPos = 0;

  }

protected:
  WebServer* webServer = nullptr;
  uint8_t* buffer;
  size_t bufferSize = 64;
  size_t bufferPos = 0;
};
