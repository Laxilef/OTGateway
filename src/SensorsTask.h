#include <microDS18B20.h>

MicroDS18B20<DS18B20_PIN> outdoorSensor;

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  void setup() {}

  void loop() {
    // DS18B20 sensor
    if (outdoorSensor.online()) {
      if (outdoorSensor.readTemp()) {
        vars.temperatures.outdoor = outdoorSensor.getTemp();

      } else {
        DEBUG("Invalid data from outdoor sensor (DS18B20)");
      }

      outdoorSensor.requestTemp();
    } else {
      WARN("Failed to connect to outdoor sensor (DS18B20)");
    }
  }
};