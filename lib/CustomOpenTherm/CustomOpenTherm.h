#include <Arduino.h>
#include <OpenTherm.h>

class CustomOpenTherm : public OpenTherm {
public:
  typedef std::function<void(unsigned int)> DelayCallback;
  typedef std::function<void(unsigned long, byte)> BeforeSendRequestCallback;
  typedef std::function<void(unsigned long, unsigned long, OpenThermResponseStatus, byte)> AfterSendRequestCallback;

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

  bool sendBoilerReset() {
    unsigned int data = 1;
    data <<= 8;
    unsigned long response = this->sendRequest(buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::RemoteRequest,
      data
    ));

    return isValidResponse(response) && isValidResponseId(response, OpenThermMessageID::RemoteRequest);
  }

  bool sendServiceReset() {
    unsigned int data = 10;
    data <<= 8;
    unsigned long response = this->sendRequest(buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::RemoteRequest,
      data
    ));

    return isValidResponse(response) && isValidResponseId(response, OpenThermMessageID::RemoteRequest);
  }

  bool sendWaterFilling() {
    unsigned int data = 2;
    data <<= 8;
    unsigned long response = this->sendRequest(buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::RemoteRequest,
      data
    ));

    return isValidResponse(response) && isValidResponseId(response, OpenThermMessageID::RemoteRequest);
  }

  static bool isCh2Active(unsigned long response)
  {
      return response & 0x20;
  }

  static bool isValidResponseId(unsigned long response, OpenThermMessageID id) {
    byte responseId = (response >> 16) & 0xFF;

    return (byte)id == responseId;
  }

  static uint8_t getResponseMessageTypeId(unsigned long response) {
    return (response << 1) >> 29;
  }

  static const char* getResponseMessageTypeString(unsigned long response) {
    uint8_t msgType = getResponseMessageTypeId(response);

    switch (msgType) {
      case (uint8_t) OpenThermMessageType::READ_ACK:
      case (uint8_t) OpenThermMessageType::WRITE_ACK:
      case (uint8_t) OpenThermMessageType::DATA_INVALID:
      case (uint8_t) OpenThermMessageType::UNKNOWN_DATA_ID:
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
