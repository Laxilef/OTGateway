#include <Arduino.h>
#include <OpenTherm.h>

class CustomOpenTherm : public OpenTherm {
public:
  typedef std::function<void(unsigned int)> DelayCallback;
  typedef std::function<void(unsigned long, uint8_t)> BeforeSendRequestCallback;
  typedef std::function<void(unsigned long, unsigned long, OpenThermResponseStatus, uint8_t)> AfterSendRequestCallback;

  CustomOpenTherm(int inPin = 4, int outPin = 5, bool isSlave = false, bool alwaysReceive = false) : OpenTherm(inPin, outPin, isSlave, alwaysReceive) {}
  ~CustomOpenTherm() {}

  CustomOpenTherm* setDelayCallback(DelayCallback callback = nullptr) {
    this->delayCallback = callback;

    return this;
  }

  CustomOpenTherm* setBeforeSendRequestCallback(BeforeSendRequestCallback callback = nullptr) {
    this->beforeSendRequestCallback = callback;

    return this;
  }

  CustomOpenTherm* setAfterSendRequestCallback(AfterSendRequestCallback callback = nullptr) {
    this->afterSendRequestCallback = callback;

    return this;
  }

  unsigned long sendRequest(unsigned long request) override {
    this->sendRequestAttempt++;

    while (!this->isReady()) {
      if (this->delayCallback) {
        this->delayCallback(150);
      }

      this->process();
    }

    if (this->beforeSendRequestCallback) {
      this->beforeSendRequestCallback(request, this->sendRequestAttempt);
    }

    if (this->sendRequestAsync(request)) {
      do {
        if (this->delayCallback) {
          this->delayCallback(150);
        }

        this->process();
      } while (this->status != OpenThermStatus::READY && this->status != OpenThermStatus::DELAY);
    }

    if (this->afterSendRequestCallback) {
      this->afterSendRequestCallback(request, this->response, this->responseStatus, this->sendRequestAttempt);
    }

    if (this->responseStatus == OpenThermResponseStatus::SUCCESS || this->responseStatus == OpenThermResponseStatus::INVALID) {
      this->sendRequestAttempt = 0;
      return this->response;

    } else if (this->sendRequestAttempt >= this->sendRequestMaxAttempts) {
      this->sendRequestAttempt = 0;
      return this->response;

    } else {
      return this->sendRequest(request);
    }
  }

  inline auto sendBoilerReset() {
    return this->sendRequestCode(1);
  }

  inline auto sendServiceReset() {
    return this->sendRequestCode(10);
  }

  inline auto sendWaterFilling() {
    return this->sendRequestCode(2);
  }

  bool sendRequestCode(const uint8_t requestCode) {
    unsigned long response = this->sendRequest(buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::RemoteRequest,
      static_cast<unsigned int>(requestCode) << 8
    ));

    if (!isValidResponse(response) || !isValidResponseId(response, OpenThermMessageID::RemoteRequest)) {
      return false;
    }

    const uint8_t responseRequestCode = (response & 0xFFFF) >> 8;
    const uint8_t responseCode = response & 0xFF;
    if (responseRequestCode != requestCode || responseCode < 128) {
      return false;
    }

    // reset
    this->sendRequest(buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::RemoteRequest,
      0u << 8
    ));

    return true;
  }

  bool getStr(OpenThermMessageID id, char* buffer, uint16_t length = 50) {
    if (buffer == nullptr || length == 0) {
      return false;
    }

    unsigned long response;
    uint8_t index = 0;
    uint8_t maxIndex = 255;

    while (index <= maxIndex && index < length) {
      response = this->sendRequest(buildRequest(
        OpenThermMessageType::READ_DATA,
        id,
        static_cast<unsigned int>(index) << 8
      ));

      if (!isValidResponse(response) || !isValidResponseId(response, id)) {
        break;
      }

      const uint8_t character = response & 0xFF;
      if (character == 0) {
        break;
      }

      if (index == 0) {
        maxIndex = (response & 0xFFFF) >> 8;
      }

      buffer[index++] = static_cast<char>(character);
    }

    buffer[index] = '\0';
    return index > 0;
  }

  static bool isCh2Active(unsigned long response) {
    return response & 0x20;
  }

  static bool isValidResponseId(unsigned long response, OpenThermMessageID id) {
    const uint8_t responseId = (response >> 16) & 0xFF;

    return static_cast<uint8_t>(id) == responseId;
  }

  static uint8_t getResponseMessageTypeId(unsigned long response) {
    return (response << 1) >> 29;
  }

  static const char* getResponseMessageTypeString(unsigned long response) {
    uint8_t msgType = getResponseMessageTypeId(response);

    switch (msgType) {
      case static_cast<uint8_t>(OpenThermMessageType::READ_ACK):
      case static_cast<uint8_t>(OpenThermMessageType::WRITE_ACK):
      case static_cast<uint8_t>(OpenThermMessageType::DATA_INVALID):
      case static_cast<uint8_t>(OpenThermMessageType::UNKNOWN_DATA_ID):
        return CustomOpenTherm::messageTypeToString(
          static_cast<OpenThermMessageType>(msgType)
        );

      default:
        return "UNKNOWN";
    }
  }

  // converters
  template <class T>
  static unsigned int toFloat(const T val) {
    return (unsigned int)(val * 256);
  }

  static short getInt(const unsigned long response) {
    return response & 0xffff;
  }

protected:
  const uint8_t sendRequestMaxAttempts = 5;
  uint8_t sendRequestAttempt = 0;
  DelayCallback delayCallback;
  BeforeSendRequestCallback beforeSendRequestCallback;
  AfterSendRequestCallback afterSendRequestCallback;
};
