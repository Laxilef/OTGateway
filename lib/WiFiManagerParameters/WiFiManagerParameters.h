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

class SeparatorParameter : public WiFiManagerParameter {
public:
  SeparatorParameter() : WiFiManagerParameter("<hr>") {}
};