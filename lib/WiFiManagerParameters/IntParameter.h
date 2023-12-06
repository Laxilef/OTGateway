#pragma once
#include <WiFiManager.h>

class IntParameter : public WiFiManagerParameter {
public:
  IntParameter(const char* id, const char* label, int value, const uint8_t length = 10) : WiFiManagerParameter("") {
    init(id, label, String(value).c_str(), length, "", WFM_LABEL_DEFAULT);
  }

  int getValue() {
    return atoi(WiFiManagerParameter::getValue());
  }

  void setValue(int value, int length) {
    WiFiManagerParameter::setValue(String(value).c_str(), length);
  }

  void setValue(int value) {
    setValue(value, getValueLength());
  }
};