#include "DisplayPort.h"

namespace {
constexpr int16_t CHROME_INSET = 24;
constexpr int16_t HEADER_HEIGHT = 44;
constexpr int16_t FOOTER_HEIGHT = 38;
constexpr uint16_t HEADER_BG = 0x0186;
constexpr uint16_t FOOTER_RULE = 0x39E7;
}

void ArduinoGfxBridge::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (_driver) _driver->drawPixel(x, y, color);
}

void ArduinoGfxBridge::startWrite() {
  if (_driver) _driver->startWrite();
}

void ArduinoGfxBridge::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (_driver) _driver->writePixel(x, y, color);
}

void ArduinoGfxBridge::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                     uint16_t color) {
  if (_driver) _driver->writeFillRect(x, y, w, h, color);
}

void ArduinoGfxBridge::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (_driver) _driver->writeFastVLine(x, y, h, color);
}

void ArduinoGfxBridge::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (_driver) _driver->writeFastHLine(x, y, w, color);
}

void ArduinoGfxBridge::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                 uint16_t color) {
  if (_driver) _driver->writeLine(x0, y0, x1, y1, color);
}

void ArduinoGfxBridge::endWrite() {
  if (_driver) _driver->endWrite();
}

void ArduinoGfxBridge::setRotation(uint8_t rotation) {
  Adafruit_GFX::setRotation(rotation);
  if (_driver) _driver->setRotation(rotation);
}

void ArduinoGfxBridge::invertDisplay(bool inverted) {
  if (_driver) _driver->invertDisplay(inverted);
}

void ArduinoGfxBridge::fillScreen(uint16_t color) {
  if (_driver) _driver->fillScreen(color);
}

void ArduinoGfxBridge::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                uint16_t color) {
  if (_driver) _driver->fillRect(x, y, w, h, color);
}

void ArduinoGfxBridge::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (_driver) _driver->drawFastVLine(x, y, h, color);
}

void ArduinoGfxBridge::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (_driver) _driver->drawFastHLine(x, y, w, color);
}

bool DisplayPort::begin() {
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  if (!_expander.begin(AMOLED_EXPANDER_I2C_ADDR, &Wire)) {
    Serial.println("[display] XCA9554 not found; trying AMOLED init anyway");
  } else {
    for (uint8_t pin = 0; pin < 3; pin++) {
      _expander.pinMode(pin, OUTPUT);
      _expander.digitalWrite(pin, LOW);
    }
    _expander.pinMode(7, OUTPUT);
    _expander.digitalWrite(7, HIGH);
    delay(20);
    for (uint8_t pin = 0; pin < 3; pin++) {
      _expander.digitalWrite(pin, HIGH);
    }
  }

  _bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_SDIO0,
                               PIN_LCD_SDIO1, PIN_LCD_SDIO2, PIN_LCD_SDIO3);
  _amoled = new Arduino_SH8601(_bus, PIN_LCD_RST, LCD_ROTATION, LCD_WIDTH, LCD_HEIGHT);
  if (!_amoled->begin()) {
    Serial.println("[display] SH8601 init failed");
    return false;
  }

  _amoled->setBrightness(AMOLED_BRIGHTNESS);
  _amoled->fillScreen(COLOR_BG);
  _gfx.attach(_amoled);
  _gfx.setRotation(LCD_ROTATION);
  _gfx.setTextWrap(false);
  _gfx.setTextSize(DISPLAY_TEXT_SIZE);
  _gfx.setTextColor(COLOR_TEXT, COLOR_BG);
  _ready = true;
  return true;
}

Adafruit_GFX& DisplayPort::gfx() {
  return _gfx;
}

void DisplayPort::clear(uint16_t color) {
  if (_ready) _gfx.fillScreen(color);
}

void DisplayPort::header(const char* title, const char* right) {
  if (!_ready) return;
  _gfx.fillRect(0, 0, width(), HEADER_HEIGHT, HEADER_BG);
  _gfx.setTextSize(2);
  _gfx.setTextColor(COLOR_TEXT, HEADER_BG);
  _gfx.setCursor(CHROME_INSET, 14);
  _gfx.print(title);
  if (right && right[0]) {
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t tw = 0;
    uint16_t th = 0;
    _gfx.getTextBounds(right, 0, 0, &x1, &y1, &tw, &th);
    _gfx.setCursor(max(static_cast<int>(CHROME_INSET),
                       static_cast<int>(width()) - static_cast<int>(tw) - CHROME_INSET),
                   14);
    _gfx.print(right);
  }
}

void DisplayPort::footer(const char* left, const char* right) {
  if (!_ready) return;
  const int16_t top = height() - FOOTER_HEIGHT;
  _gfx.fillRect(0, top, width(), FOOTER_HEIGHT, COLOR_PANEL);
  _gfx.drawFastHLine(0, top - 1, width(), FOOTER_RULE);
  _gfx.setTextSize(2);
  _gfx.setTextColor(COLOR_DIM, COLOR_PANEL);
  _gfx.setCursor(CHROME_INSET, height() - 28);
  _gfx.print(left);
  if (right && right[0]) {
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t tw = 0;
    uint16_t th = 0;
    _gfx.getTextBounds(right, 0, 0, &x1, &y1, &tw, &th);
    _gfx.setCursor(max(static_cast<int>(CHROME_INSET),
                       static_cast<int>(width()) - static_cast<int>(tw) - CHROME_INSET),
                   height() - 28);
    _gfx.print(right);
  }
}

void DisplayPort::message(const char* title, const char* body, uint16_t color) {
  clear();
  header(title);
  _gfx.setTextSize(2);
  _gfx.setTextColor(color, COLOR_BG);
  int y = 72;
  const char* p = body;
  while (*p && y < static_cast<int>(height()) - 52) {
    char line[34];
    uint8_t len = 0;
    while (*p && *p != '\n' && len < sizeof(line) - 1) {
      line[len++] = *p++;
    }
    if (*p == '\n') p++;
    line[len] = '\0';
    _gfx.setCursor(18, y);
    _gfx.print(line);
    y += 28;
  }
  footer("Tap or BOOT: back");
}

void DisplayPort::setBrightness(uint8_t brightness) {
  if (_amoled) _amoled->setBrightness(brightness);
}
