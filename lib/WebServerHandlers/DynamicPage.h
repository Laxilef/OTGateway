#include <FS.h>


class DynamicPage : public RequestHandler {
public:
  typedef std::function<bool(HTTPMethod, const String&)> CanHandleCallback;
  typedef std::function<bool()> BeforeSendCallback;
  typedef std::function<String(const char*)> TemplateCallback;
  
  DynamicPage(const char* uri, FS* fs, const char* path, const char* cacheHeader = nullptr) {
    this->uri = uri;
    this->fs = fs;
    this->path = path;
    this->cacheHeader = cacheHeader;
  }

  DynamicPage* setCanHandleCallback(CanHandleCallback callback = nullptr) {
    this->canHandleCallback = callback;

    return this;
  }

  DynamicPage* setBeforeSendCallback(BeforeSendCallback callback = nullptr) {
    this->beforeSendCallback = callback;

    return this;
  }

  DynamicPage* setTemplateCallback(TemplateCallback callback = nullptr) {
    this->templateCallback = callback;

    return this;
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool canHandle(WebServer &server, HTTPMethod method, const String &uri) override {
    return this->canHandle(method, uri);
  }
  #endif

  bool canHandle(HTTPMethod method, const String& uri) override {
    return uri.equals(this->uri) && (!this->canHandleCallback || this->canHandleCallback(method, uri));
  }

  bool handle(WebServer& server, HTTPMethod method, const String& uri) override {
    if (!this->canHandle(method, uri)) {
      return false;
    }

    if (this->beforeSendCallback && !this->beforeSendCallback()) {
      return true;
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

    #ifdef ARDUINO_ARCH_ESP8266
    if (!server.chunkedResponseModeStart(200, F("text/html"))) {
      server.send(505, F("text/html"), F("HTTP1.1 required"));
      return true;
    }
    #else
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", emptyString);
    #endif

    uint8_t* argStartPos = nullptr;
    uint8_t* argEndPos = nullptr;
    uint8_t argName[16];
    size_t sizeArgName = 0;
    bool argNameProcess = false;
    while (file.available()) {
      uint8_t buf[64];
      size_t length = file.read(buf, sizeof(buf));
      size_t offset = 0;

      if (argNameProcess) {
        argEndPos = (uint8_t*) memchr(buf, '}', length);
        
        if (argEndPos != nullptr) {
          size_t fullSizeArgName = sizeArgName + (argEndPos - buf);
          if (fullSizeArgName < sizeof(argName)) {
            // copy full arg name
            if (argEndPos - buf > 0) {
              memcpy(argName + sizeArgName, buf, argEndPos - buf);
            }
            argName[fullSizeArgName] = '\0';

            // send arg value
            String argValue = this->templateCallback((const char*) argName);
            if (argValue.length()) {
              server.sendContent(argValue.c_str());

            } else if (fullSizeArgName > 0) {
              server.sendContent("{");
              server.sendContent((const char*) argName);
              server.sendContent("}");
            }

            offset = size_t(argEndPos - buf + 1);
            sizeArgName = 0;
            argNameProcess = false;
          }
        }

        if (argNameProcess) {
          server.sendContent("{");

          if (sizeArgName > 0) {
            argName[sizeArgName] = '\0';
            server.sendContent((const char*) argName);
          }

          argNameProcess = false;
        }
      }

      do {
        uint8_t* currentBuf = buf + offset;
        size_t currentLength = length - offset;

        argStartPos = (uint8_t*) memchr(currentBuf, '{', currentLength);

        // send all content
        if (argStartPos == nullptr) {
          if (currentLength > 0) {
            server.sendContent((const char*) currentBuf, currentLength);
          }

          break;
        }
        
        argEndPos = (uint8_t*) memchr(argStartPos, '}', length - (argStartPos - buf));
        if (argEndPos != nullptr) {
          sizeArgName = argEndPos - argStartPos - 1;

          // send all content if arg len > space
          if (sizeArgName >= sizeof(argName)) {
            if (currentLength > 0) {
              server.sendContent((const char*) currentBuf, currentLength);
            }

            break;
          }
          
          // arg name
          memcpy(argName, argStartPos + 1, sizeArgName);
          argName[sizeArgName] = '\0';

          // send arg value
          String argValue = this->templateCallback((const char*) argName);
          if (argValue.length()) {
            // send content before var
            if (argStartPos - buf > 0) {
              server.sendContent((const char*) currentBuf, argStartPos - buf);
            }

            server.sendContent(argValue.c_str());
            
          } else {
            server.sendContent((const char*) currentBuf, argEndPos - currentBuf + 1);
          }

          offset = size_t(argEndPos - currentBuf + 1);

        } else {
          sizeArgName = length - size_t(argStartPos - currentBuf) - 1;

          // send all content if arg len > space
          if (sizeArgName >= sizeof(argName)) {
            if (currentLength) {
              server.sendContent((const char*) currentBuf, currentLength);
            }

            break;
          }

          // send content before var
          if (argStartPos - buf > 0) {
            server.sendContent((const char*) currentBuf, argStartPos - buf);
          }

          // copy arg name chunk
          if (sizeArgName > 0) {
            memcpy(argName, argStartPos + 1, sizeArgName);
          }

          argNameProcess = true;

          break;
        }
      } while(true);
    }
    
    file.close();

    #ifdef ARDUINO_ARCH_ESP8266
    server.chunkedResponseFinalize();
    #else
    server.sendContent(emptyString);
    #endif

    return true;
  }

protected:
  FS* fs = nullptr;
  CanHandleCallback canHandleCallback;
  BeforeSendCallback beforeSendCallback;
  TemplateCallback templateCallback;
  String eTag;
  const char* uri = nullptr;
  const char* path = nullptr;
  const char* cacheHeader = nullptr;
};