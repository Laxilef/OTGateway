#pragma once
#include <WiFiManager.h>

class ShortParameter : public WiFiManagerParameter {
public:
  ShortParameter(const char* id, const char* label, short value, const uint8_t length = 10) : WiFiManagerParameter("") {
    init(id, label, String(value).c_str(), length, "", WFM_LABEL_DEFAULT);
  }

  short getValue() {
    return atoi(WiFiManagerParameter::getValue());
  }

  void setValue(short value, int length) {
    WiFiManagerParameter::setValue(String(value).c_str(), length);
  }

  void setValue(short value) {
    setValue(value, getValueLength());
  }
};