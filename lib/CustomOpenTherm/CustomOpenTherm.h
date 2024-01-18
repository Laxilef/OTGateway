#include <Arduino.h>
#include <OpenTherm.h>

class CustomOpenTherm : public OpenTherm {
public:
  typedef std::function<void()> YieldCallback;
  typedef std::function<void(unsigned long, byte)> BeforeSendRequestCallback;
  typedef std::function<void(unsigned long, unsigned long, OpenThermResponseStatus, byte)> AfterSendRequestCallback;

  CustomOpenTherm(int inPin = 4, int outPin = 5, bool isSlave = false) : OpenTherm(inPin, outPin, isSlave) {}

  CustomOpenTherm* setYieldCallback(YieldCallback callback = nullptr) {
    this->yieldCallback = callback;

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

  unsigned long sendRequest(unsigned long request, byte attempts = 5, byte _attempt = 0) {
    _attempt++;
    bool antiFreeze = true;
    while (antiFreeze || (this->prevRequestTime > 0 && millis() - this->prevRequestTime <= 200)) {
      if (this->yieldCallback) {
        this->yieldCallback();
      } else {
        ::yield();
      }

      if (antiFreeze) {
        antiFreeze = false;
      }
    }

    if (this->beforeSendRequestCallback) {
      this->beforeSendRequestCallback(request, _attempt);
    }

    unsigned long _response;
    if (!sendRequestAync(request)) {
      _response = 0;
      
    } else {
      while (true) {
        process();

        if (isReady()) {
          break;
        }

        if (this->yieldCallback) {
          this->yieldCallback();
        } else {
          ::yield();
        }
      }

      _response = getLastResponse();
    }
    this->prevRequestTime = millis();

    OpenThermResponseStatus _responseStatus = getLastResponseStatus();
    if (this->afterSendRequestCallback) {
      this->afterSendRequestCallback(request, _response, _responseStatus, _attempt);
    }

    if (_responseStatus == OpenThermResponseStatus::SUCCESS || _responseStatus == OpenThermResponseStatus::INVALID || _attempt >= attempts) {
      return _response;

    } else {
      return sendRequest(request, attempts, _attempt);
    }
  }

  unsigned long setBoilerStatus(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2, bool summerWinterMode, bool dhwBlocking) {
    return sendRequest(buildSetBoilerStatusRequest(enableCentralHeating, enableHotWater, enableCooling, enableOutsideTemperatureCompensation, enableCentralHeating2, summerWinterMode, dhwBlocking));
  }

  unsigned long buildSetBoilerStatusRequest(bool enableCentralHeating, bool enableHotWater, bool enableCooling, bool enableOutsideTemperatureCompensation, bool enableCentralHeating2, bool summerWinterMode, bool dhwBlocking) {
    unsigned int data = enableCentralHeating | (enableHotWater << 1) | (enableCooling << 2) | (enableOutsideTemperatureCompensation << 3) | (enableCentralHeating2 << 4) | (summerWinterMode << 5) | (dhwBlocking << 6);
    data <<= 8;
    return buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Status, data);
  }

  bool setHeatingCh1Temp(float temperature) {
    unsigned int data = temperatureToData(temperature);
    unsigned long response = sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TSet, data));
    return isValidResponse(response);
  }

  bool setHeatingCh2Temp(float temperature) {
    unsigned int data = temperatureToData(temperature);
    unsigned long response = sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TsetCH2, data));
    return isValidResponse(response);
  }

  bool setDhwTemp(float temperature) {
    unsigned int data = temperatureToData(temperature);
    unsigned long response = sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data));
    return isValidResponse(response);
  }

  bool sendBoilerReset() {
    unsigned int data = 1;
    data <<= 8;
    unsigned long response = sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::Command, data));
    return isValidResponse(response);
  }

  bool sendServiceReset() {
    unsigned int data = 10;
    data <<= 8;
    unsigned long response = sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::Command, data));
    return isValidResponse(response);
  }

  bool sendWaterFilling() {
    unsigned int data = 2;
    data <<= 8;
    unsigned long response = sendRequest(buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::Command, data));
    return isValidResponse(response);
  }

  // converters
  float fromF88(unsigned long response) {
    const byte valueLB = response & 0xFF;
    const byte valueHB = (response >> 8) & 0xFF;

    float value = (int8_t) valueHB;
    return value + (float) valueLB / 256.0;
  }

  template <class T> unsigned int toF88(T val) {
    return (unsigned int) (val * 256);
  }

  int16_t fromS16(unsigned long response) {
    const byte valueLB = response & 0xFF;
    const byte valueHB = (response >> 8) & 0xFF;

    int16_t value = valueHB;
    return ((value << 8) + valueLB);
  }

protected:
  YieldCallback yieldCallback;
  BeforeSendRequestCallback beforeSendRequestCallback;
  AfterSendRequestCallback afterSendRequestCallback;
  unsigned long prevRequestTime = 0;
};
