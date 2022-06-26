class CustomTask : public Task {
public:
  CustomTask(bool enabled = false, unsigned long interval = 0) {
    _enabled = enabled;
    _interval = interval;
  }

  bool isEnabled() {
    return _enabled;
  }

  void enable() {
    _enabled = true;
  }

  void disable() {
    _enabled = false;
  }

  void setInterval(unsigned long val) {
    _interval = val;
  }

  unsigned long getInterval() {
    return _interval;
  }

protected:
  bool _enabled = true;
  unsigned long _lastRun = 0;
  unsigned long _interval = 0;

  bool shouldRun() {
    if (!_enabled || !Task::shouldRun()) {
      return false;
    }

    if (_interval > 0 && millis() - _lastRun < _interval) {
      return false;
    }

    _lastRun = millis();
    return true;
  }
};