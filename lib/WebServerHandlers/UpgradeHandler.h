#include <Arduino.h>

class UpgradeHandler : public RequestHandler {
public:
  enum class UpgradeType {
    FIRMWARE = 0,
    FILESYSTEM = 1
  };

  enum class UpgradeStatus {
    NONE,
    NO_FILE,
    SUCCESS,
    PROHIBITED,
    ABORTED,
    ERROR_ON_START,
    ERROR_ON_WRITE,
    ERROR_ON_FINISH
  };

  typedef struct {
    UpgradeType type;
    UpgradeStatus status;
    String error;
  } UpgradeResult;

  typedef std::function<bool(HTTPMethod, const String&)> CanHandleCallback;
  typedef std::function<bool(const String&)> CanUploadCallback;
  typedef std::function<bool(UpgradeType)> BeforeUpgradeCallback;
  typedef std::function<void(const UpgradeResult&, const UpgradeResult&)> AfterUpgradeCallback;
  
  UpgradeHandler(const char* uri) {
    this->uri = uri;
  }

  UpgradeHandler* setCanHandleCallback(CanHandleCallback callback = nullptr) {
    this->canHandleCallback = callback;

    return this;
  }

  UpgradeHandler* setCanUploadCallback(CanUploadCallback callback = nullptr) {
    this->canUploadCallback = callback;

    return this;
  }

  UpgradeHandler* setBeforeUpgradeCallback(BeforeUpgradeCallback callback = nullptr) {
    this->beforeUpgradeCallback = callback;

    return this;
  }

  UpgradeHandler* setAfterUpgradeCallback(AfterUpgradeCallback callback = nullptr) {
    this->afterUpgradeCallback = callback;

    return this;
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool canHandle(WebServer &server, HTTPMethod method, const String &uri) override {
    return this->canHandle(method, uri);
  }
  #endif

  bool canHandle(HTTPMethod method, const String& uri) override {
    return method == HTTP_POST && uri.equals(this->uri) && (!this->canHandleCallback || this->canHandleCallback(method, uri));
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool canUpload(WebServer &server, const String &uri) override {
    return this->canUpload(uri);
  }
  #endif

  bool canUpload(const String& uri) override {
    return uri.equals(this->uri) && (!this->canUploadCallback || this->canUploadCallback(uri));
  }

  bool handle(WebServer& server, HTTPMethod method, const String& uri) override {
    if (this->afterUpgradeCallback) {
      this->afterUpgradeCallback(this->firmwareResult, this->filesystemResult);
    }

    this->firmwareResult.status = UpgradeStatus::NONE;
    this->firmwareResult.error.clear();

    this->filesystemResult.status = UpgradeStatus::NONE;
    this->filesystemResult.error.clear();

    return true;
  }

  void upload(WebServer& server, const String& uri, HTTPUpload& upload) override {
    UpgradeResult* result;
    if (upload.name.equals("firmware")) {
      result = &this->firmwareResult;
    } else if (upload.name.equals("filesystem")) {
      result = &this->filesystemResult;
    } else {
      return;
    }

    if (result->status != UpgradeStatus::NONE) {
      return;
    }

    if (this->beforeUpgradeCallback && !this->beforeUpgradeCallback(result->type)) {
      result->status = UpgradeStatus::PROHIBITED;
      return;
    }

    if (!upload.filename.length()) {
      result->status = UpgradeStatus::NO_FILE;
      return;
    }

    if (upload.status == UPLOAD_FILE_START) {
      // reset
      if (Update.isRunning()) {
        Update.end(false);
        Update.clearError();
      }

      bool begin = false;
      #ifdef ARDUINO_ARCH_ESP8266
      Update.runAsync(true);
      
      if (result->type == UpgradeType::FIRMWARE) {
        begin = Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000, U_FLASH);
        
      } else if (result->type == UpgradeType::FILESYSTEM) {
        close_all_fs();
        begin = Update.begin((size_t)FS_end - (size_t)FS_start, U_FS);
      }
      #elif defined(ARDUINO_ARCH_ESP32)
      if (result->type == UpgradeType::FIRMWARE) {
        begin = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        
      } else if (result->type == UpgradeType::FILESYSTEM) {
        begin = Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
      }
      #endif

      if (!begin || Update.hasError()) {
        result->status = UpgradeStatus::ERROR_ON_START;
        #ifdef ARDUINO_ARCH_ESP8266
        result->error = Update.getErrorString();
        #else
        result->error = Update.errorString();
        #endif

        Log.serrorln(FPSTR(L_PORTAL_OTA), F("File '%s', on start: %s"), upload.filename.c_str(), result->error.c_str());
        return;
      }
      
      Log.sinfoln(FPSTR(L_PORTAL_OTA), F("File '%s', started"), upload.filename.c_str());

    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.end(false);

        result->status = UpgradeStatus::ERROR_ON_WRITE;
        #ifdef ARDUINO_ARCH_ESP8266
        result->error = Update.getErrorString();
        #else
        result->error = Update.errorString();
        #endif

        Log.serrorln(
          FPSTR(L_PORTAL_OTA),
          F("File '%s', on writing %d bytes: %s"),
          upload.filename.c_str(), upload.totalSize, result->error.c_str()
        );

      } else {
        Log.sinfoln(FPSTR(L_PORTAL_OTA), F("File '%s', writed %d bytes"), upload.filename.c_str(), upload.totalSize);
      }

    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        result->status = UpgradeStatus::SUCCESS;

        Log.sinfoln(FPSTR(L_PORTAL_OTA), F("File '%s': finish"), upload.filename.c_str());

      } else {
        result->status = UpgradeStatus::ERROR_ON_FINISH;
        #ifdef ARDUINO_ARCH_ESP8266
        result->error = Update.getErrorString();
        #else
        result->error = Update.errorString();
        #endif

        Log.serrorln(FPSTR(L_PORTAL_OTA), F("File '%s', on finish: %s"), upload.filename.c_str(), result->error);
      }

    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.end(false);
      result->status = UpgradeStatus::ABORTED;

      Log.serrorln(FPSTR(L_PORTAL_OTA), F("File '%s': aborted"), upload.filename.c_str());
    }
  }

protected:
  CanHandleCallback canHandleCallback;
  CanUploadCallback canUploadCallback;
  BeforeUpgradeCallback beforeUpgradeCallback;
  AfterUpgradeCallback afterUpgradeCallback;
  const char* uri = nullptr;

  UpgradeResult firmwareResult{UpgradeType::FIRMWARE, UpgradeStatus::NONE};
  UpgradeResult filesystemResult{UpgradeType::FILESYSTEM, UpgradeStatus::NONE};
};