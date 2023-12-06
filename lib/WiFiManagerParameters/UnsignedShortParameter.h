#pragma once
#include <WiFiManager.h>

class UnsignedShortParameter : public WiFiManagerParameter {
public:
  UnsignedShortParameter(const char* id, const char* label, unsigned short value, const uint8_t length = 10) : WiFiManagerParameter("") {
    init(id, label, String(value).c_str(), length, "", WFM_LABEL_DEFAULT);
  }

  unsigned short getValue() {
    return (unsigned short) atoi(WiFiManagerParameter::getValue());
  }

  void setValue(unsigned short value, int length) {
    WiFiManagerParameter::setValue(String(value).c_str(), length);
  }

  void setValue(unsigned short value) {
    setValue(value, getValueLength());
  }
};