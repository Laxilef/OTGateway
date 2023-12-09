#pragma once
#include "ESPTelnetStream.h"

class BufferedTelnetStream : public ESPTelnetStream {
public:
  size_t write(const uint8_t* data, size_t size) override {
    if (client && isConnected()) {
      return client.write(data, size);
    } else {
      return 0;
    }
  }
};