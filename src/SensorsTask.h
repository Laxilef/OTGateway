#include <OneWire.h>
#include <DallasTemperature.h>

const char S_SENSORS_OUTDOOR[]  PROGMEM = "SENSORS.OUTDOOR";
const char S_SENSORS_INDOOR[]   PROGMEM = "SENSORS.INDOOR";

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  OneWire* oneWireOutdoorSensor;
  OneWire* oneWireIndoorSensor;

  DallasTemperature* outdoorSensor;
  DallasTemperature* indoorSensor;

  bool initOutdoorSensor = false;
  unsigned long startOutdoorConversionTime = 0;
  float filteredOutdoorTemp = 0;
  bool emptyOutdoorTemp = true;

  bool initIndoorSensor = false;
  unsigned long startIndoorConversionTime = 0;
  float filteredIndoorTemp = 0;
  bool emptyIndoorTemp = true;


  const char* getTaskName() {
    return "Sensors";
  }

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
      startOutdoorConversionTime = millis();
      initOutdoorSensor = true;
    }

    unsigned long estimateConversionTime = millis() - startOutdoorConversionTime;
    if (estimateConversionTime < outdoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = outdoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      // fail, retry
      outdoorSensor->requestTemperatures();
      startOutdoorConversionTime = millis();

      Log.serrorln(FPSTR(S_SENSORS_OUTDOOR), F("Could not read temperature data (no response)"));
    }

    if (!completed) {
      return;
    }

    float rawTemp = outdoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      Log.serrorln(FPSTR(S_SENSORS_OUTDOOR), F("Could not read temperature data (not connected)"));

    } else {
      Log.straceln(FPSTR(S_SENSORS_OUTDOOR), F("Raw temp: %f"), rawTemp);

      if (emptyOutdoorTemp) {
        filteredOutdoorTemp = rawTemp;
        emptyOutdoorTemp = false;

      } else {
        filteredOutdoorTemp += (rawTemp - filteredOutdoorTemp) * EXT_SENSORS_FILTER_K;
      }

      filteredOutdoorTemp = floor(filteredOutdoorTemp * 100) / 100;

      if (fabs(vars.temperatures.outdoor - filteredOutdoorTemp) > 0.099) {
        vars.temperatures.outdoor = filteredOutdoorTemp + settings.sensors.outdoor.offset;
        Log.sinfoln(FPSTR(S_SENSORS_OUTDOOR), F("New temp: %f"), filteredOutdoorTemp);
      }
    }

    outdoorSensor->requestTemperatures();
    startOutdoorConversionTime = millis();
  }

  void indoorTemperatureSensor() {
    if (!initIndoorSensor) {
      oneWireIndoorSensor = new OneWire(settings.sensors.indoor.pin);
      indoorSensor = new DallasTemperature(oneWireIndoorSensor);
      indoorSensor->begin();
      indoorSensor->setResolution(12);
      indoorSensor->setWaitForConversion(false);
      indoorSensor->requestTemperatures();
      startIndoorConversionTime = millis();
      initIndoorSensor = true;
    }

    unsigned long estimateConversionTime = millis() - startIndoorConversionTime;
    if (estimateConversionTime < indoorSensor->millisToWaitForConversion()) {
      return;
    }

    bool completed = indoorSensor->isConversionComplete();
    if (!completed && estimateConversionTime >= 1000) {
      // fail, retry
      indoorSensor->requestTemperatures();
      startIndoorConversionTime = millis();
      
      Log.serrorln(FPSTR(S_SENSORS_INDOOR), F("Could not read temperature data (no response)"));
    }

    if (!completed) {
      return;
    }

    float rawTemp = indoorSensor->getTempCByIndex(0);
    if (rawTemp == DEVICE_DISCONNECTED_C) {
      Log.serrorln(FPSTR(S_SENSORS_INDOOR), F("Could not read temperature data (not connected)"));

    } else {
      Log.straceln(FPSTR(S_SENSORS_INDOOR), F("Raw temp: %f"), rawTemp);

      if (emptyIndoorTemp) {
        filteredIndoorTemp = rawTemp;
        emptyIndoorTemp = false;

      } else {
        filteredIndoorTemp += (rawTemp - filteredIndoorTemp) * EXT_SENSORS_FILTER_K;
      }

      filteredIndoorTemp = floor(filteredIndoorTemp * 100) / 100;

      if (fabs(vars.temperatures.indoor - filteredIndoorTemp) > 0.099) {
        vars.temperatures.indoor = filteredIndoorTemp + settings.sensors.indoor.offset;
        Log.sinfoln(FPSTR(S_SENSORS_INDOOR), F("New temp: %f"), filteredIndoorTemp);
      }
    }

    indoorSensor->requestTemperatures();
    startIndoorConversionTime = millis();
  }
};