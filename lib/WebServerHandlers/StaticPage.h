#include <FS.h>
#include <detail/mimetable.h>

using namespace mime;

class StaticPage : public RequestHandler {
public:
  typedef std::function<bool(HTTPMethod, const String&)> CanHandleCallback;
  typedef std::function<bool()> BeforeSendCallback;

  template <class T>
  StaticPage(const char* uri, FS* fs, T path, const char* cacheHeader = nullptr) {
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

  bool canHandle(WebServer &server, HTTPMethod method, const String &uri) override {
    return this->canHandle(method, uri);
  }

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

    if (server._eTagEnabled) {
      if (this->eTag.isEmpty()) {
        if (server._eTagFunction) {
          this->eTag = (server._eTagFunction)(*this->fs, this->path);
        }
      }

      if (!this->eTag.isEmpty() && server.header("If-None-Match").equals(this->eTag.c_str())) {
        server.send(304);
        return true;
      }
    }

    if (!this->path.endsWith(mimeTable[gz].endsWith) && !this->fs->exists(path))  {
      String pathWithGz = this->path + mimeTable[gz].endsWith;

      if (this->fs->exists(pathWithGz)) {
        this->path += mimeTable[gz].endsWith;
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

    if (server._eTagEnabled && !this->eTag.isEmpty()) {
      server.sendHeader("ETag", this->eTag);
    }

    server.streamFile(file, "text/html", 200);

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
