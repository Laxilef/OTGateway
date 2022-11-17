#include <microDS18B20.h>

MicroDS18B20<DS18B20_PIN> outdoorSensor;

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  float filteredOutdoorTemp = 0;
  bool emptyOutdoorTemp = true;
  void setup() {}

  void loop() {
    // DS18B20 sensor
    if (outdoorSensor.online()) {
      if (outdoorSensor.readTemp()) {
        float rawTemp = outdoorSensor.getTemp();
        INFO_F("[SENSORS][DS18B20] Raw temp: %f \n", rawTemp);

        if ( emptyOutdoorTemp ) {
          filteredOutdoorTemp = rawTemp;
          emptyOutdoorTemp = false;

        } else {
          filteredOutdoorTemp += (rawTemp - filteredOutdoorTemp) * OUTDOOR_SENSOR_FILTER_K;
        }

        filteredOutdoorTemp = floor(filteredOutdoorTemp * 100) / 100;
        
        if ( fabs(vars.temperatures.outdoor - filteredOutdoorTemp) > 0.099 ) {
          vars.temperatures.outdoor = filteredOutdoorTemp;
          INFO_F("[SENSORS][DS18B20] New temp: %f \n", filteredOutdoorTemp);
        }

      } else {
        ERROR("[SENSORS][DS18B20] Invalid data from sensor");
      }

      outdoorSensor.requestTemp();
    } else {
      ERROR("[SENSORS][DS18B20] Failed to connect to sensor");
    }
  }
};