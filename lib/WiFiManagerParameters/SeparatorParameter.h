#pragma once
#include <WiFiManager.h>


class SeparatorParameter : public WiFiManagerParameter {
public:
  SeparatorParameter() : WiFiManagerParameter("<hr>") {}
};