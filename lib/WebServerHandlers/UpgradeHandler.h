#include <Arduino.h>
#include <esp_log.h>
#include <strings_shared.h>

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
    size_t progress = 0;
    size_t size = 0;
  } UpgradeResult;

  typedef std::function<bool(AsyncWebServerRequest *request, UpgradeType)> BeforeUpgradeCallback;
  // A null request reports an upload disconnected after preparation was accepted.
  typedef std::function<void(AsyncWebServerRequest *request, const UpgradeResult&, const UpgradeResult&)> AfterUpgradeCallback;

  explicit UpgradeHandler(AsyncURIMatcher uri) : uri(uri) {}

  bool canHandle(AsyncWebServerRequest *request) const final {
    if (!request->isHTTP() || request->method() != HTTP_POST) {
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

  void handleRequest(AsyncWebServerRequest *request) final {
    if (request->getResponse()) {
      return;
    }

    if (this->activeRequest != request) {
      request->send(this->activeRequest ? 409 : 400);
      return;
    }

    finishRequest(request);
  }

  void handleUpload(AsyncWebServerRequest *request, const String &fileName, size_t index, uint8_t *data, size_t dataLength, bool isFinal) final {
    if (request->getResponse()) {
      return;
    }
    if (this->activeRequest && this->activeRequest != request) {
      // Keep the rejection on this request even if the active upload finishes first.
      request->send(409);
      return;
    }

    UpgradeResult* result = nullptr;

    if (!request->hasParam(asyncsrv::T_name, true, true)) {
      // Missing content-disposition 'name' parameter
      return;
    }

    const auto& pName = request->getParam(asyncsrv::T_name, true, true)->value();
    if (pName.equals("fw")) {
      result = &this->firmwareResult;

      if (!index) {
        result->progress = 0;
        result->size = request->hasParam("fw_size", true)
          ? request->getParam("fw_size", true)->value().toInt()
          : 0;
      }

    } else if (pName.equals("fs")) {
      result = &this->filesystemResult;

      if (!index) {
        result->progress = 0;
        result->size = request->hasParam("fs_size", true)
          ? request->getParam("fs_size", true)->value().toInt()
          : 0;
      }

    } else {
      // Unknown parameter name
      return;
    }

    if (!this->activeRequest) {
      this->activeRequest = request;
      request->onDisconnect([this, request] {
        if (this->activeRequest == request) {
          finishRequest(nullptr);
        }
      });
    }

    // check result status
    if (result->status != UpgradeStatus::NONE) {
      return;
    }

    if (this->beforeUpgradeCallback && !this->beforeUpgradeCallback(request, result->type)) {
      result->status = UpgradeStatus::PROHIBITED;
      return;
    }
    this->preparationAccepted = true;

    if (!fileName.length()) {
      result->status = UpgradeStatus::NO_FILE;
      return;
    }

    if (!index) {
      // reset
      if (Update.isRunning()) {
        Update.end(false);
        Update.clearError();
      }

      // try begin
      bool begin = false;
      if (result->type == UpgradeType::FIRMWARE) {
        begin = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);

      } else if (result->type == UpgradeType::FILESYSTEM) {
        begin = Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
      }

      if (!begin || Update.hasError()) {
        result->status = UpgradeStatus::ERROR_ON_START;
        result->error = Update.errorString();

        ESP_LOGE(L_PORTAL_OTA, "File '%s', on start: %s", fileName.c_str(), result->error.c_str());
        return;
      }

      ESP_LOGI(L_PORTAL_OTA, "File '%s', started", fileName.c_str());
    }

    if (dataLength) {
      if (Update.write(data, dataLength) != dataLength) {
        Update.end(false);
        result->status = UpgradeStatus::ERROR_ON_WRITE;
        result->error = Update.errorString();

        ESP_LOGE(
          L_PORTAL_OTA, "File '%s', on write %d bytes, %d of %d bytes",
          fileName.c_str(),
          dataLength,
          result->progress + dataLength,
          result->size
        );
        return;
      }

      result->progress += dataLength;
      ESP_LOGI(
        L_PORTAL_OTA, "File '%s', write %d bytes, %d of %d bytes",
        fileName.c_str(),
        dataLength,
        result->progress,
        result->size
      );
    }

    if (result->size > 0) {
      if (result->progress > result->size || (isFinal && result->progress < result->size)) {
        Update.end(false);
        result->status = UpgradeStatus::SIZE_MISMATCH;

        ESP_LOGE(
          L_PORTAL_OTA, "File '%s', size mismatch: %d of %d bytes",
          fileName.c_str(),
          result->progress,
          result->size
        );
        return;
      }
    }

    if (isFinal) {
      if (!Update.end(true)) {
        result->status = UpgradeStatus::ERROR_ON_FINISH;
        result->error = Update.errorString();

        ESP_LOGE(L_PORTAL_OTA, "File '%s', on finish: %s", fileName.c_str(), result->error.c_str());
        return;
      }

      result->status = UpgradeStatus::SUCCESS;
      ESP_LOGI(L_PORTAL_OTA, "File '%s': finish", fileName.c_str());
    }
  }

  bool isRequestHandlerTrivial() const final {
    return false;
  }

protected:
  void finishRequest(AsyncWebServerRequest *request) {
    if (request) {
      request->onDisconnect(nullptr);
    }
    if (Update.isRunning()) {
      // Abort only the unfinished file; an earlier successful firmware still needs a restart.
      Update.abort();
    }
    if (this->afterUpgradeCallback && (request || this->preparationAccepted)) {
      this->afterUpgradeCallback(request, this->firmwareResult, this->filesystemResult);
    }

    this->activeRequest = nullptr;
    this->preparationAccepted = false;
    this->firmwareResult = {UpgradeType::FIRMWARE, UpgradeStatus::NONE};
    this->filesystemResult = {UpgradeType::FILESYSTEM, UpgradeStatus::NONE};
  }

  BeforeUpgradeCallback beforeUpgradeCallback;
  AfterUpgradeCallback afterUpgradeCallback;
  AsyncURIMatcher uri;

  // Upload, completion and disconnect callbacks share the AsyncTCP worker.
  AsyncWebServerRequest* activeRequest = nullptr;
  bool preparationAccepted = false;
  UpgradeResult firmwareResult{UpgradeType::FIRMWARE, UpgradeStatus::NONE};
  UpgradeResult filesystemResult{UpgradeType::FILESYSTEM, UpgradeStatus::NONE};
};
