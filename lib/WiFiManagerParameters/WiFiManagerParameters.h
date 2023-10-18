class IntParameter : public WiFiManagerParameter {
public:
  IntParameter(const char* id, const char* label, int value, const uint8_t length = 10) : WiFiManagerParameter("") {
    init(id, label, String(value).c_str(), length, "", WFM_LABEL_DEFAULT);
  }

  int getValue() {
    return atoi(WiFiManagerParameter::getValue());
  }
};

class CheckboxParameter : public WiFiManagerParameter {
public:
  const char* checked = "type=\"checkbox\" checked";
  const char* noChecked = "type=\"checkbox\"";
  const char* trueVal = "T";

  CheckboxParameter(const char* id, const char* label, bool value) : WiFiManagerParameter("") {
    init(id, label, value ? trueVal : "0", 1, "", WFM_LABEL_AFTER);
  }

  const char* getValue() const override {
    return trueVal;
  }

  const char* getCustomHTML() const override {
    return strcmp(WiFiManagerParameter::getValue(), trueVal) == 0 ? checked : noChecked;
  }

  bool getCheckboxValue() {
    return strcmp(WiFiManagerParameter::getValue(), trueVal) == 0 ? true : false;
  }
};

class SeparatorParameter : public WiFiManagerParameter {
public:
  SeparatorParameter() : WiFiManagerParameter("<hr>") {}
};