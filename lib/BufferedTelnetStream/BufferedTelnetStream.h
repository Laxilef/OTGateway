#pragma once
#include "ESPTelnetStream.h"
#include <StreamUtils.h>

class BufferedTelnetStream : public ESPTelnetStream {
public:
  size_t write(const uint8_t* buffer, size_t size) {
    WriteBufferingStream bufferedWifiClient{ client, 32 };
    size_t _size = bufferedWifiClient.write((const char*) buffer);
    bufferedWifiClient.flush();
    return _size;
  }
};