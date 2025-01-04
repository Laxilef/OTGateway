#include <Arduino.h>
#include <OpenTherm.h>

class CustomOpenTherm : public OpenTherm {
public:
  typedef std::function<void(unsigned int)> DelayCallback;
  typedef std::function<void(unsigned long, byte)> BeforeSendRequestCallback;
  typedef std::function<void(unsigned long, unsigned long, OpenThermResponseStatus, byte)> AfterSendRequestCallback;

  CustomOpenTherm(int inPin = 4, int outPin = 5, bool isSlave = false) : OpenTherm(inPin, outPin, isSlave) {
    this->_outPin = outPin;
  }
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

  void reset() {
    if (this->status == OpenThermStatus::NOT_INITIALIZED) {
      return;
    }

    this->end();
    this->status = OpenThermStatus::NOT_INITIALIZED;

    digitalWrite(this->_outPin, LOW);
    if (this->delayCallback) {
      this->delayCallback(1000);
    }
    
    this->begin();
  }

  unsigned long sendRequest(unsigned long request, byte attempts = 5, byte _attempt = 0) {
    _attempt++;

    while (!this->isReady()) {
      if (this->delayCallback) {
        this->delayCallback(150);
      }

      this->process();
    }

    if (this->beforeSendRequestCallback) {
      this->beforeSendRequestCallback(request, _attempt);
    }

    unsigned long _response;
    OpenThermResponseStatus _responseStatus = OpenThermResponseStatus::NONE;
    if (!this->sendRequestAsync(request)) {
      _response = 0;

    } else {
      do {
        if (this->delayCallback) {
          this->delayCallback(150);
        }

        this->process();
      } while (this->status != OpenThermStatus::READY && this->status != OpenThermStatus::DELAY);

      _response = this->getLastResponse();
      _responseStatus = this->getLastResponseStatus();
    }

    if (this->afterSendRequestCallback) {
      this->afterSendRequestCallback(request, _response, _responseStatus, _attempt);
    }

    if (_responseStatus == OpenThermResponseStatus::SUCCESS || _responseStatus == OpenThermResponseStatus::INVALID || _attempt >= attempts) {
      return _response;

    } else {
      return this->sendRequest(request, attempts, _attempt);
    }
  }

  unsigned long setBoilerStatus(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2, bool summerWinterMode, bool dhwBlocking, uint8_t lb = 0) {
    unsigned int data = enableCentralHeating
      | (enableHotWater << 1)
      | (enableCooling << 2)
      | (enableOutsideTemperatureCompensation << 3)
      | (enableCentralHeating2 << 4)
      | (summerWinterMode << 5)
      | (dhwBlocking << 6);
    
    data <<= 8;
    data |= lb;

    return this->sendRequest(buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::Status,
      data
    ));
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

  static bool isValidResponseId(unsigned long response, OpenThermMessageID id) {
    byte responseId = (response >> 16) & 0xFF;
    
    return (byte)id == responseId;
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
  DelayCallback delayCallback;
  BeforeSendRequestCallback beforeSendRequestCallback;
  AfterSendRequestCallback afterSendRequestCallback;
  int _outPin;
};
