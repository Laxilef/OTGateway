#pragma once
#include <WiFiManager.h>

class DoubleParameter : public WiFiManagerParameter {
public:
  DoubleParameter(const char* id, const char* label, double value, const uint8_t length = 10) : WiFiManagerParameter("") {
    init(id, label, String(value, length - 1).c_str(), length, "", WFM_LABEL_DEFAULT);
  }

  double getValue() {
    return atof(WiFiManagerParameter::getValue());
  }

  void setValue(double value, int length) {
    WiFiManagerParameter::setValue(String(value, length - 1).c_str(), length);
  }

  void setValue(double value) {
    setValue(value, getValueLength());
  }
};