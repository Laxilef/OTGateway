#pragma once
#include <WiFiManager.h>

class CheckboxParameter : public WiFiManagerParameter {
public:
  const char* checked = "type=\"checkbox\" checked";
  const char* noChecked = "type=\"checkbox\"";
  const char* trueVal = "T";

  CheckboxParameter(const char* id, const char* label, bool value) : WiFiManagerParameter("") {
    init(id, label, String(value ? trueVal : "0").c_str(), 1, "", WFM_LABEL_AFTER);
  }

  const char* getValue() const override {
    return trueVal;
  }

  void setValue(bool value) {
    WiFiManagerParameter::setValue(value ? trueVal : "0", 1);
  }

  const char* getCustomHTML() const override {
    return strcmp(WiFiManagerParameter::getValue(), trueVal) == 0 ? checked : noChecked;
  }

  bool getCheckboxValue() {
    return strcmp(WiFiManagerParameter::getValue(), trueVal) == 0 ? true : false;
  }
};