#include <Arduino.h>

class UpgradeHandler : public AsyncWebHandler {
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
    SIZE_MISMATCH,
    ERROR_ON_START,
    ERROR_ON_WRITE,
    ERROR_ON_FINISH
  };

  typedef struct {
    UpgradeType type;
    UpgradeStatus status;
    String error;
    unsigned int written = 0;
  } UpgradeResult;

  typedef std::function<bool(AsyncWebServerRequest *request, UpgradeType)> BeforeUpgradeCallback;
  typedef std::function<void(AsyncWebServerRequest *request, const UpgradeResult&, const UpgradeResult&)> AfterUpgradeCallback;
  
  UpgradeHandler(AsyncURIMatcher uri) : uri(uri) {}

  bool canHandle(AsyncWebServerRequest *request) const override final {
    if (!request->isHTTP()) {
      return false;
    }
    
    return this->uri.matches(request);
  }

  UpgradeHandler* setBeforeUpgradeCallback(BeforeUpgradeCallback callback = nullptr) {
    this->beforeUpgradeCallback = callback;

    return this;
  }

  UpgradeHandler* setAfterUpgradeCallback(AfterUpgradeCallback callback = nullptr) {
    this->afterUpgradeCallback = callback;

    return this;
  }

  void handleRequest(AsyncWebServerRequest *request) override final {
    if (this->afterUpgradeCallback) {
      this->afterUpgradeCallback(request, this->firmwareResult, this->filesystemResult);
    }

    this->firmwareResult.status = UpgradeStatus::NONE;
    this->firmwareResult.error.clear();
    this->firmwareResult.written = 0;

    this->filesystemResult.status = UpgradeStatus::NONE;
    this->filesystemResult.error.clear();
    this->filesystemResult.written = 0;
  }

  void handleUpload(AsyncWebServerRequest *request, const String &fileName, size_t index, uint8_t *data, size_t dataLength, bool isFinal) override final {
    UpgradeResult* result = nullptr;
    unsigned int fileSize = 0;

    const auto& fwName = request->hasParam("fw[name]", true) ? request->getParam("fw[name]", true)->value() : String();
    const auto& fsName = request->hasParam("fs[name]", true) ? request->getParam("fs[name]", true)->value() : String();

    if (fileName.equals(fwName)) {
      result = &this->firmwareResult;
      if (request->hasParam("fw[size]", true)) {
        fileSize = request->getParam("fw[size]", true)->value().toInt();
      }

    } else if (fileName.equals(fsName)) {
      result = &this->filesystemResult;
      if (request->hasParam("fs[size]", true)) {
        fileSize = request->getParam("fs[size]", true)->value().toInt();
      } 
    }

    if (result == nullptr || result->status != UpgradeStatus::NONE) {
      return;
    }

    if (this->beforeUpgradeCallback && !this->beforeUpgradeCallback(request, result->type)) {
      result->status = UpgradeStatus::PROHIBITED;
      return;
    }

    if (!fileName.length() || !fileSize) {
      result->status = UpgradeStatus::NO_FILE;
      return;
    }

    if (index == 0) {
      // reset
      if (Update.isRunning()) {
        Update.end(false);
        Update.clearError();
      }

      bool begin = false;
      if (result->type == UpgradeType::FIRMWARE) {
        begin = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        
      } else if (result->type == UpgradeType::FILESYSTEM) {
        begin = Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
      }

      if (!begin || Update.hasError()) {
        result->status = UpgradeStatus::ERROR_ON_START;
        result->error = Update.errorString();

        Log.serrorln(FPSTR(L_PORTAL_OTA), F("File '%s', on start: %s"), fileName.c_str(), result->error.c_str());
        return;
      }
      
      Log.sinfoln(FPSTR(L_PORTAL_OTA), F("File '%s', started"), fileName.c_str());
    }
    
    if (dataLength) {
      result->written += dataLength;

      if (Update.write(data, dataLength) != dataLength) {
        Update.end(false);
        result->status = UpgradeStatus::ERROR_ON_WRITE;
        result->error = Update.errorString();

        Log.serrorln(
          FPSTR(L_PORTAL_OTA),
          F("File '%s', on write %d bytes, %d of %d bytes"),
          fileName.c_str(),
          dataLength,
          result->written,
          fileSize
        );
        return;
      }

      Log.sinfoln(
        FPSTR(L_PORTAL_OTA),
        F("File '%s', write %d bytes, %d of %d bytes"),
        fileName.c_str(),
        dataLength,
        result->written,
        fileSize
      );
    }

    if (result->written > fileSize || (isFinal && result->written < fileSize)) {
      Update.end(false);
      result->status = UpgradeStatus::SIZE_MISMATCH;

      Log.serrorln(
        FPSTR(L_PORTAL_OTA),
        F("File '%s', size mismatch: %d of %d bytes"),
        fileName.c_str(),
        result->written,
        fileSize
      );
      return;
    }
    
    if (isFinal) {
      if (!Update.end(true)) {
        result->status = UpgradeStatus::ERROR_ON_FINISH;
        result->error = Update.errorString();

        Log.serrorln(FPSTR(L_PORTAL_OTA), F("File '%s', on finish: %s"), fileName.c_str(), result->error);
        return;
      }

      result->status = UpgradeStatus::SUCCESS;
      Log.sinfoln(FPSTR(L_PORTAL_OTA), F("File '%s': finish"), fileName.c_str());
    }
  }

  bool isRequestHandlerTrivial() const override final {
    return false;
  }

protected:
  BeforeUpgradeCallback beforeUpgradeCallback;
  AfterUpgradeCallback afterUpgradeCallback;
  AsyncURIMatcher uri;

  UpgradeResult firmwareResult{UpgradeType::FIRMWARE, UpgradeStatus::NONE};
  UpgradeResult filesystemResult{UpgradeType::FILESYSTEM, UpgradeStatus::NONE};
};