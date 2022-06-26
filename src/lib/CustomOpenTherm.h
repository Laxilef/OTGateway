#include <OpenTherm.h>

extern SchedulerClass Scheduler;

class CustomOpenTherm : public OpenTherm {
private:
  unsigned long send_ts = millis();
  void(*handleSendRequestCallback)(unsigned long, unsigned long, OpenThermResponseStatus status, byte attempt);

public:
  CustomOpenTherm(int inPin = 4, int outPin = 5, bool isSlave = false) : OpenTherm(inPin, outPin, isSlave) {}
  void setHandleSendRequestCallback(void(*handleSendRequestCallback)(unsigned long, unsigned long, OpenThermResponseStatus status, byte attempt)) {
    this->handleSendRequestCallback = handleSendRequestCallback;
  }

  unsigned long sendRequest(unsigned long request, byte attempts = 5, byte _attempt = 0) {
    _attempt++;
    while (send_ts > 0 && millis() - send_ts < 200) {
      Scheduler.yield();
    }

    //unsigned long response = OpenTherm::sendRequest(request);
    unsigned long _response;
    if (!sendRequestAync(request)) {
      _response = 0;
    } else {
      while (!isReady()) {
        Scheduler.yield();
        process();
      }

      _response = getLastResponse();
    }

    if (handleSendRequestCallback != NULL) {
      handleSendRequestCallback(request, _response, getLastResponseStatus(), _attempt);
    }

    send_ts = millis();
    if (getLastResponseStatus() == OpenThermResponseStatus::SUCCESS || _attempt >= attempts) {
      return _response;
    } else {
      return sendRequest(request, attempts, _attempt);
    }
  }
};
