#include <FS.h>
#include <detail/mimetable.h>

using namespace mime;

class StaticPage : public RequestHandler {
public:
  typedef std::function<bool(HTTPMethod, const String&)> CanHandleCallback;
  typedef std::function<bool()> BeforeSendCallback;
  
  StaticPage(const char* uri, FS* fs, const char* path, const char* cacheHeader = nullptr) {
    this->uri = uri;
    this->fs = fs;
    this->path = path;
    this->cacheHeader = cacheHeader;
  }

  StaticPage* setCanHandleCallback(CanHandleCallback callback = nullptr) {
    this->canHandleCallback = callback;

    return this;
  }

  StaticPage* setBeforeSendCallback(BeforeSendCallback callback = nullptr) {
    this->beforeSendCallback = callback;

    return this;
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool canHandle(WebServer &server, HTTPMethod method, const String &uri) override {
    return this->canHandle(method, uri);
  }
  #endif

  bool canHandle(HTTPMethod method, const String& uri) override {
    return method == HTTP_GET && uri.equals(this->uri) && (!this->canHandleCallback || this->canHandleCallback(method, uri));
  }

  bool handle(WebServer& server, HTTPMethod method, const String& uri) override {
    if (!this->canHandle(method, uri)) {
      return false;
    }

    if (this->beforeSendCallback && !this->beforeSendCallback()) {
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

    if (!this->path.endsWith(FPSTR(mimeTable[gz].endsWith)) && !this->fs->exists(path))  {
      String pathWithGz = this->path + FPSTR(mimeTable[gz].endsWith);

      if (this->fs->exists(pathWithGz)) {
        this->path += FPSTR(mimeTable[gz].endsWith);
      }
    }

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
    
    server.streamFile(file, F("text/html"), method);
    #else
    server.streamFile(file, F("text/html"), 200);
    #endif

    return true;
  }

protected:
  FS* fs = nullptr;
  CanHandleCallback canHandleCallback;
  BeforeSendCallback beforeSendCallback;
  String eTag;
  const char* uri = nullptr;
  String path;
  const char* cacheHeader = nullptr;
};