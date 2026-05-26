#include "TouchInput.h"

#include <Arduino_DriveBus_Library.h>
#include <Wire.h>
#include <memory>

namespace {
std::shared_ptr<Arduino_IIC_DriveBus> touchBus;
std::unique_ptr<Arduino_FT3x68> touchDevice;

void remapTouch(int32_t rawX, int32_t rawY, uint16_t& outX, uint16_t& outY) {
  int32_t x = rawX;
  int32_t y = rawY;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= LCD_WIDTH) x = LCD_WIDTH - 1;
  if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;
  outX = static_cast<uint16_t>(x);
  outY = static_cast<uint16_t>(y);
}
}

bool TouchInput::begin() {
  touchBus = std::make_shared<Arduino_HWIIC>(PIN_TOUCH_SDA, PIN_TOUCH_SCL, &Wire);
  touchDevice.reset(new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS,
                                       DRIVEBUS_DEFAULT_VALUE, PIN_TOUCH_INT));
  for (uint8_t tries = 0; tries < 5; tries++) {
    if (touchDevice->begin()) {
      touchDevice->IIC_Write_Device_State(touchDevice->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
                                          touchDevice->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
      Serial.printf("[touch] FT3168 init ok id=0x%X\n",
                    static_cast<unsigned>(touchDevice->IIC_Read_Device_ID()));
      _ready = true;
      return true;
    }
    Serial.println("[touch] FT3168 init retry");
    delay(250);
  }
  Serial.println("[touch] FT3168 init failed");
  _ready = false;
  return false;
}

bool TouchInput::read(TouchPoint* points, uint8_t maxPoints, uint8_t& pointCount) {
  pointCount = 0;
  if (!_ready || !touchDevice || !points || maxPoints == 0) return false;

  const int32_t fingers = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
  if (fingers <= 0) return false;

  const int32_t rawX = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
  const int32_t rawY = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));

  remapTouch(rawX, rawY, points[0].x, points[0].y);
  pointCount = 1;
  return true;
}
