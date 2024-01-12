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

  typedef std::function<bool(HTTPMethod, const String&)> CanHandleFunction;
  typedef std::function<bool(const String&)> CanUploadFunction;
  typedef std::function<bool(UpgradeType)> BeforeUpgradeFunction;
  typedef std::function<void(const UpgradeResult&, const UpgradeResult&)> AfterUpgradeFunction;
  
  UpgradeHandler(const char* uri) {
    this->uri = uri;
  }

  UpgradeHandler* setCanHandleFunction(CanHandleFunction val = nullptr) {
    this->canHandleFn = val;

    return this;
  }

  UpgradeHandler* setCanUploadFunction(CanUploadFunction val = nullptr) {
    this->canUploadFn = val;

    return this;
  }

  UpgradeHandler* setBeforeUpgradeFunction(BeforeUpgradeFunction val = nullptr) {
    this->beforeUpgradeFn = val;

    return this;
  }

  UpgradeHandler* setAfterUpgradeFunction(AfterUpgradeFunction val = nullptr) {
    this->afterUpgradeFn = val;

    return this;
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool canHandle(HTTPMethod method, const String uri) override {
  #else
  bool canHandle(HTTPMethod method, const String& uri) override {
  #endif
    return method == HTTP_POST && uri.equals(this->uri) && (!this->canHandleFn || this->canHandleFn(method, uri));
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool canUpload(const String uri) override {
  #else
  bool canUpload(const String& uri) override {
  #endif
    return uri.equals(this->uri) && (!this->canUploadFn || this->canUploadFn(uri));
  }

  #if defined(ARDUINO_ARCH_ESP32)
  bool handle(WebServer& server, HTTPMethod method, const String uri) override {
  #else
  bool handle(WebServer& server, HTTPMethod method, const String& uri) override {
  #endif
    if (this->afterUpgradeFn) {
      this->afterUpgradeFn(this->firmwareResult, this->filesystemResult);
    }

    this->firmwareResult.status = UpgradeStatus::NONE;
    this->firmwareResult.error.clear();

    this->filesystemResult.status = UpgradeStatus::NONE;
    this->filesystemResult.error.clear();

    return true;
  }

  #if defined(ARDUINO_ARCH_ESP32)
  void upload(WebServer& server, const String uri, HTTPUpload& upload) override {
  #else
  void upload(WebServer& server, const String& uri, HTTPUpload& upload) override {
  #endif
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

    if (this->beforeUpgradeFn && !this->beforeUpgradeFn(result->type)) {
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

        Log.serrorln("PORTAL.OTA", F("File '%s', on start: %s"), upload.filename.c_str(), result->error.c_str());
        return;
      }
      
      Log.sinfoln("PORTAL.OTA", F("File '%s', started"), upload.filename.c_str());

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
          "PORTAL.OTA",
          F("File '%s', on writing %d bytes: %s"),
          upload.filename.c_str(), upload.totalSize, result->error
        );

      } else {
        Log.sinfoln("PORTAL.OTA", F("File '%s', writed %d bytes"), upload.filename.c_str(), upload.totalSize);
      }

    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        result->status = UpgradeStatus::SUCCESS;

        Log.sinfoln("PORTAL.OTA", F("File '%s': finish"), upload.filename.c_str());

      } else {
        result->status = UpgradeStatus::ERROR_ON_FINISH;
        #ifdef ARDUINO_ARCH_ESP8266
        result->error = Update.getErrorString();
        #else
        result->error = Update.errorString();
        #endif

        Log.serrorln("PORTAL.OTA", F("File '%s', on finish: %s"), upload.filename.c_str(), result->error);
      }

    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.end(false);
      result->status = UpgradeStatus::ABORTED;

      Log.serrorln("PORTAL.OTA", F("File '%s': aborted"), upload.filename.c_str());
    }
  }

protected:
  CanHandleFunction canHandleFn;
  CanUploadFunction canUploadFn;
  BeforeUpgradeFunction beforeUpgradeFn;
  AfterUpgradeFunction afterUpgradeFn;
  const char* uri = nullptr;

  UpgradeResult firmwareResult{UpgradeType::FIRMWARE, UpgradeStatus::NONE};
  UpgradeResult filesystemResult{UpgradeType::FILESYSTEM, UpgradeStatus::NONE};
};