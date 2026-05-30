#include <Adafruit_GFX.h>
#include <Adafruit_XCA9554.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <memory>

#include <WaveshareAmoledIntro.h>
#include <WaveshareAmoledReturn.h>

constexpr uint16_t LCD_WIDTH = 368;
constexpr uint16_t LCD_HEIGHT = 448;
constexpr int PIN_LCD_SDIO0 = 4;
constexpr int PIN_LCD_SDIO1 = 5;
constexpr int PIN_LCD_SDIO2 = 6;
constexpr int PIN_LCD_SDIO3 = 7;
constexpr int PIN_LCD_SCLK = 11;
constexpr int PIN_LCD_CS = 12;
constexpr int PIN_TOUCH_SDA = 15;
constexpr int PIN_TOUCH_SCL = 14;
constexpr int PIN_TOUCH_INT = 21;
constexpr int PIN_BOOT_BUTTON = 0;
constexpr uint8_t AMOLED_EXPANDER_I2C_ADDR = 0x20;

Arduino_DataBus* bus = nullptr;
Arduino_SH8601* gfx = nullptr;
Adafruit_XCA9554 expander;
std::shared_ptr<Arduino_IIC_DriveBus> touchBus;
std::unique_ptr<Arduino_FT3x68> touchDevice;

void drawStatic() {
  gfx->fillScreen(0x0000);
  gfx->fillRect(0, 0, LCD_WIDTH, 42, 0x0186);
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF, 0x0186);
  gfx->setCursor(14, 13);
  gfx->print("Touch Diagnostics");
  gfx->setTextColor(0xBDF7, 0x0000);
  gfx->setCursor(20, 70);
  gfx->print("Tap or drag on the screen.");
  gfx->setCursor(20, 100);
  gfx->print("BOOT returns to launcher.");
  gfx->drawRect(20, 150, LCD_WIDTH - 40, 210, 0x39E7);
}

void setupDisplay() {
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  if (expander.begin(AMOLED_EXPANDER_I2C_ADDR, &Wire)) {
    for (uint8_t pin = 0; pin < 3; pin++) {
      expander.pinMode(pin, OUTPUT);
      expander.digitalWrite(pin, LOW);
    }
    delay(20);
    for (uint8_t pin = 0; pin < 3; pin++) expander.digitalWrite(pin, HIGH);
  }
  bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_SDIO0, PIN_LCD_SDIO1,
                              PIN_LCD_SDIO2, PIN_LCD_SDIO3);
  gfx = new Arduino_SH8601(bus, -1, 0, LCD_WIDTH, LCD_HEIGHT);
  gfx->begin();
  gfx->setBrightness(220);
  WaveshareAmoledIntro::draw(*gfx, LCD_WIDTH, LCD_HEIGHT, "Touch Diagnostics");
  drawStatic();
}

void setupTouch() {
  touchBus = std::make_shared<Arduino_HWIIC>(PIN_TOUCH_SDA, PIN_TOUCH_SCL, &Wire);
  touchDevice.reset(new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS,
                                       DRIVEBUS_DEFAULT_VALUE, PIN_TOUCH_INT));
  touchDevice->begin();
  touchDevice->IIC_Write_Device_State(touchDevice->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
                                      touchDevice->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
}

bool readTouch(uint16_t& x, uint16_t& y) {
  if (!touchDevice) return false;
  int32_t fingers = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
  if (fingers <= 0) return false;
  int32_t rawX = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
  int32_t rawY = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
  int32_t mx = rawX;
  int32_t my = rawY;
  x = constrain(mx, 0, LCD_WIDTH - 1);
  y = constrain(my, 0, LCD_HEIGHT - 1);
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  setupDisplay();
  setupTouch();
  Serial.println("touch-diagnostics ready");
}

void loop() {
  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
    delay(500);
    if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
      WaveshareAmoledOs::returnToLauncher();
    }
  }

  uint16_t x = 0;
  uint16_t y = 0;
  if (readTouch(x, y)) {
    gfx->fillCircle(x, y, 5, 0x07FF);
    gfx->fillRect(20, 382, 310, 32, 0x0000);
    gfx->setTextSize(2);
    gfx->setTextColor(0xFFFF, 0x0000);
    gfx->setCursor(20, 390);
    gfx->printf("x=%u y=%u", x, y);
  }
  delay(20);
}
