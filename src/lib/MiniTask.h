class MiniTask {
public:
  MiniTask(bool enabled = false, unsigned long interval = 0) {
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

  void loopWrapper() {
    if (!shouldRun()) {
      return;
    }

    if (!_setupDone) {
      setup();
      _setupDone = true;
    }

    loop();
    yield();
  }

protected:
  virtual void setup() {}
  virtual void loop() {}

  virtual bool shouldRun() {
    if (!_enabled) {
      return false;
    }

    if (_interval > 0 && millis() - _lastRun < _interval) {
      return false;
    }

    _lastRun = millis();
    return true;
  }

private:
  bool _enabled = false;
  unsigned long _interval = 0;
  unsigned long _lastRun = 0;
  bool _setupDone = false;
};
