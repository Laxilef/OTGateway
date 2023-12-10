#pragma once
#include <WiFiManager.h>


class HeaderParameter : public WiFiManagerParameter {
public:
  HeaderParameter(const char* title) {
    WiFiManagerParameter("");
    byte size = strlen(title) + strlen(this->tpl) + 1;
    this->buffer = new char[size];
    this->title = title;
  }

  ~HeaderParameter() {
    delete[] this->buffer;
  }

  const char* getCustomHTML() const override {
    sprintf(this->buffer, this->tpl, title);
    return this->buffer;
  }

protected:
  const char* title;
  char* buffer;
  const char* tpl = "<div class=\"bheader\">%s</div>";
};