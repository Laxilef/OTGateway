#include <OneWire.h>
#include <DallasTemperature.h>

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  OneWire* oneWireOutdoorSensor;
  OneWire* oneWireIndoorSensor;

  DallasTemperature* outdoorSensor;
  DallasTemperature* indoorSensor;

  bool initOutdoorSensor = false;
  unsigned long startConversionTime = 0;
  float filteredOutdoorTemp = 0;
  bool emptyOutdoorTemp = true;

  bool initIndoorSensor = false;
  float filteredIndoorTemp = 0;
  bool emptyIndoorTemp = true;


  void setup() {}
  void loop() {
    if (settings.sensors.outdoor.type == 2) {
      outdoorTemperatureSensor();
    }

    if (settings.sensors.indoor.type == 2) {
      indoorTemperatureSensor();
    }
  }

  void outdoorTemperatureSensor() {
    if (!initOutdoorSensor) {
      oneWireOutdoorSensor = new OneWire(settings.sensors.outdoor.pin);
      outdoorSensor = new DallasTemperature(oneWireOutdoorSensor);
      outdoorSensor->begin();
      outdoorSensor->setResolution(12);
      outdoorSensor->setWaitForConversion(false);
      outdoorSensor->requestTemperatures();
      startConversionTime = millis();
      initOutdoorSensor = true;
    }

    unsigned long estimateConversionTime = millis() - startConversionTime;
    if (estimateConversionTime < outdoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = outdoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      // fail, retry
      outdoorSensor->requestTemperatures();
      startConversionTime = millis();

      ERROR("[SENSORS][OUTDOOR] Could not read temperature data (no response)");
    }

    if (!completed) {
      return;
    }

    float rawTemp = outdoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      ERROR("[SENSORS][OUTDOOR] Could not read temperature data (not connected)");

    } else {
      DEBUG_F("[SENSORS][OUTDOOR] Raw temp: %f \n", rawTemp);

      if (emptyOutdoorTemp) {
        filteredOutdoorTemp = rawTemp;
        emptyOutdoorTemp = false;

      } else {
        filteredOutdoorTemp += (rawTemp - filteredOutdoorTemp) * EXT_SENSORS_FILTER_K;
      }

      filteredOutdoorTemp = floor(filteredOutdoorTemp * 100) / 100;

      if (fabs(vars.temperatures.outdoor - filteredOutdoorTemp) > 0.099) {
        vars.temperatures.outdoor = filteredOutdoorTemp + settings.sensors.outdoor.offset;
        INFO_F("[SENSORS][OUTDOOR] New temp: %f \n", filteredOutdoorTemp);
      }
    }

    outdoorSensor->requestTemperatures();
    startConversionTime = millis();
  }

  void indoorTemperatureSensor() {
    if (!initIndoorSensor) {
      oneWireIndoorSensor = new OneWire(settings.sensors.indoor.pin);
      indoorSensor = new DallasTemperature(oneWireIndoorSensor);
      indoorSensor->begin();
      indoorSensor->setResolution(12);
      indoorSensor->setWaitForConversion(false);
      indoorSensor->requestTemperatures();
      startConversionTime = millis();
      initIndoorSensor = true;
    }

    unsigned long estimateConversionTime = millis() - startConversionTime;
    if (estimateConversionTime < indoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = indoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      // fail, retry
      indoorSensor->requestTemperatures();
      startConversionTime = millis();

      ERROR("[SENSORS][INDOOR] Could not read temperature data (no response)");
    }

    if (!completed) {
      return;
    }

    float rawTemp = indoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      ERROR("[SENSORS][INDOOR] Could not read temperature data (not connected)");

    } else {
      DEBUG_F("[SENSORS][INDOOR] Raw temp: %f \n", rawTemp);

      if (emptyIndoorTemp) {
        filteredIndoorTemp = rawTemp;
        emptyIndoorTemp = false;

      } else {
        filteredIndoorTemp += (rawTemp - filteredIndoorTemp) * EXT_SENSORS_FILTER_K;
      }

      filteredIndoorTemp = floor(filteredIndoorTemp * 100) / 100;

      if (fabs(vars.temperatures.indoor - filteredIndoorTemp) > 0.099) {
        vars.temperatures.indoor = filteredIndoorTemp + settings.sensors.indoor.offset;
        INFO_F("[SENSORS][INDOOR] New temp: %f \n", filteredIndoorTemp);
      }
    }

    indoorSensor->requestTemperatures();
    startConversionTime = millis();
  }
};