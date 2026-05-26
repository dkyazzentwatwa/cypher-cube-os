#pragma once

#include <Arduino.h>

#include "BoardConfig.h"

struct TouchPoint {
  uint16_t x = 0;
  uint16_t y = 0;
};

class TouchInput {
public:
  bool begin();
  bool read(TouchPoint* points, uint8_t maxPoints, uint8_t& pointCount);
  bool available() const { return _ready; }

private:
  bool _ready = false;
};

