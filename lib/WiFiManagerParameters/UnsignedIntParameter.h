#pragma once
#include <WiFiManager.h>

class UnsignedIntParameter : public WiFiManagerParameter {
public:
  UnsignedIntParameter(const char* id, const char* label, unsigned int value, const uint8_t length = 10) : WiFiManagerParameter("") {
    init(id, label, String(value).c_str(), length, "", WFM_LABEL_DEFAULT);
  }

  unsigned int getValue() {
    return (unsigned int) atoi(WiFiManagerParameter::getValue());
  }

  void setValue(unsigned int value, int length) {
    WiFiManagerParameter::setValue(String(value).c_str(), length);
  }

  void setValue(unsigned int value) {
    setValue(value, getValueLength());
  }
};