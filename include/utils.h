#pragma once

#include <Arduino.h>
#include <cmath>
#include <stdio.h>
#include <string>
#include <string.h>
#include <time.h>

#include "types.h"

inline std::string getChipId(const char* prefix = nullptr, const char* suffix = nullptr) {
  std::string chipId;
  chipId.reserve(
    6
    + (prefix != nullptr ? strlen(prefix) : 0)
    + (suffix != nullptr ? strlen(suffix) : 0)
  );

  if (prefix != nullptr) {
    chipId += prefix;
  }

  uint32_t cid = 0;
  // https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/ChipID/GetChipID/GetChipID.ino
  for (uint8_t i = 0; i < 17; i = i + 8) {
    cid |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  char hexId[7];
  snprintf(hexId, sizeof(hexId), "%lx", static_cast<unsigned long>(cid));
  chipId += hexId;

  if (suffix != nullptr) {
    chipId += suffix;
  }

  chipId.erase(chipId.find_last_not_of(" \t\n\r\f\v") + 1);
  chipId.erase(0, chipId.find_first_not_of(" \t\n\r\f\v"));
  return chipId;
}

inline bool isLeapYear(short year) {
  if (year % 4 != 0) {
    return false;
  }

  if (year % 100 != 0) {
    return true;
  }

  return (year % 400) == 0;
}

// convert UTC tm time to time_t epoch time
// source: https://github.com/cyberman54/ESP32-Paxcounter/blob/master/src/timekeeper.cpp
inline time_t mkgmtime(const struct tm *ptm) {
  const int SecondsPerMinute = 60;
  const int SecondsPerHour = 3600;
  const int SecondsPerDay = 86400;
  const int DaysOfMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  time_t secs = 0;
  // tm_year is years since 1900
  int year = ptm->tm_year + 1900;
  for (int y = 1970; y < year; ++y) {
    secs += (isLeapYear(y) ? 366 : 365) * SecondsPerDay;
  }
  // tm_mon is month from 0..11
  for (int m = 0; m < ptm->tm_mon; ++m) {
    secs += DaysOfMonth[m] * SecondsPerDay;
    if (m == 1 && isLeapYear(year)) {
      secs += SecondsPerDay;
    }
  }
  secs += (ptm->tm_mday - 1) * SecondsPerDay;
  secs += ptm->tm_hour * SecondsPerHour;
  secs += ptm->tm_min * SecondsPerMinute;
  secs += ptm->tm_sec;
  return secs;
}

inline float liter2gallon(float value) {
  return value / 3.785411784f;
}

inline float gallon2liter(float value) {
  return value * 3.785411784f;
}

inline float convertVolume(float value, const UnitSystem unitFrom, const UnitSystem unitTo) {
  if (unitFrom == UnitSystem::METRIC && unitTo == UnitSystem::IMPERIAL) {
    value = liter2gallon(value);

  } else if (unitFrom == UnitSystem::IMPERIAL && unitTo == UnitSystem::METRIC) {
    value = gallon2liter(value);
  }

  return value;
}

inline float bar2psi(float value) {
  return value * 14.5038f;
}

inline float psi2bar(float value) {
  return value / 14.5038f;
}

inline float convertPressure(float value, const UnitSystem unitFrom, const UnitSystem unitTo) {
  if (unitFrom == UnitSystem::METRIC && unitTo == UnitSystem::IMPERIAL) {
    value = bar2psi(value);

  } else if (unitFrom == UnitSystem::IMPERIAL && unitTo == UnitSystem::METRIC) {
    value = psi2bar(value);
  }

  return value;
}

inline float c2f(float value) {
  return (9.0f / 5.0f) * value + 32.0f;
}

inline float f2c(float value) {
  return (value - 32.0f) * 5.0f / 9.0f;
}

inline float convertTemp(float value, const UnitSystem unitFrom, const UnitSystem unitTo) {
  if (unitFrom == UnitSystem::METRIC && unitTo == UnitSystem::IMPERIAL) {
    value = c2f(value);

  } else if (unitFrom == UnitSystem::IMPERIAL && unitTo == UnitSystem::METRIC) {
    value = f2c(value);
  }

  return value;
}

inline bool isValidTemp(const float value, UnitSystem unit, const float min = 0.1f, const float max = 99.9f, const UnitSystem minMaxUnit = UnitSystem::METRIC) {
  return value >= convertTemp(min, minMaxUnit, unit) && value <= convertTemp(max, minMaxUnit, unit);
}

inline bool isFloatDifferent(float first, float second, float tolerance) {
  return fabsf(first - second) > tolerance;
}

inline float roundf(float value, uint8_t decimals = 2) {
  if (decimals == 0) {
    return std::round(value);

  } else if (abs(value) < 0.00000001f) {
    return 0.0f;
  }

  const float multiplier = pow10(decimals);
  const float scaled = value * multiplier;
  return std::isfinite(scaled) ? std::round(scaled) / multiplier : value;
}

template <class T>
void arr2str(String& str, T arr[], size_t length) {
  char buffer[12];
  for (size_t i = 0; i < length; i++) {
    auto addr = arr[i];
    if (!addr) {
      continue;
    }

    sprintf(buffer, "0x%08X ", addr);
    str.concat(buffer);
  }

  str.trim();
}

inline uint8_t rssiToSignalQuality(int16_t rssi) {
  return constrain(map(rssi, -100, -50, 0, 100), 0, 100);
}
