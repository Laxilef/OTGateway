#include <FS.h>

class StaticPage : public RequestHandler {
public:
  typedef std::function<bool(HTTPMethod, const String&)> canHandleFunction;
  typedef std::function<bool()> beforeSendFunction;
  
  StaticPage(const char* uri, FS* fs, const char* path, const char* cacheHeader = nullptr) {
    this->uri = uri;
    this->fs = fs;
    this->path = path;
    this->cacheHeader = cacheHeader;
  }

  StaticPage* setCanHandleFunction(canHandleFunction val = nullptr) {
    this->canHandleFn = val;

    return this;
  }

  StaticPage* setBeforeSendFunction(beforeSendFunction val = nullptr) {
    this->beforeSendFn = val;

    return this;
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool canHandle(HTTPMethod method, const String uri) override {
  #else
  bool canHandle(HTTPMethod method, const String& uri) override {
  #endif
    return method == HTTP_GET && uri.equals(this->uri) && (!this->canHandleFn || this->canHandleFn(method, uri));
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool handle(WebServer& server, HTTPMethod method, const String uri) override {
  #else
  bool handle(WebServer& server, HTTPMethod method, const String& uri) override {
  #endif
    if (!this->canHandle(method, uri)) {
      return false;
    }

    if (this->beforeSendFn && !this->beforeSendFn()) {
      return true;
    }

    #if defined(ARDUINO_ARCH_ESP8266)
    if (server._eTagEnabled) {
      if (server._eTagFunction) {
        this->eTag = (server._eTagFunction)(*this->fs, this->path);

      } else  if (this->eTag.isEmpty()) {
        this->eTag = esp8266webserver::calcETag(*this->fs, this->path);
      }

      if (server.header("If-None-Match").equals(this->eTag.c_str())) {
        server.send(304);
        return true;
      }
    }
    #endif

    File file = this->fs->open(this->path, "r");
    if (!file) {
      return false;

    } else if (file.isDirectory()) {
      file.close();
      return false;
    }

    if (this->cacheHeader != nullptr) {
      server.sendHeader("Cache-Control", this->cacheHeader);
    }

    #if defined(ARDUINO_ARCH_ESP8266)
    if (server._eTagEnabled && this->eTag.length() > 0) {
      server.sendHeader("ETag", this->eTag);
    }
    #endif

    server.streamFile(file, F("text/html"), method);

    return true;
  }

protected:
  FS* fs = nullptr;
  canHandleFunction canHandleFn;
  beforeSendFunction beforeSendFn;
  String eTag;
  const char* uri = nullptr;
  const char* path = nullptr;
  const char* cacheHeader = nullptr;
};