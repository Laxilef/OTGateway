#include <microDS18B20.h>

MicroDS18B20<DS18B20_PIN> outdoorSensor;

class SensorsTask : public MiniTask {
public:
  SensorsTask(bool enabled = false, unsigned long interval = 0) : MiniTask(enabled, interval) {}

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