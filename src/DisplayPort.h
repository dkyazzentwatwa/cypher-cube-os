#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_XCA9554.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#include "BoardConfig.h"

class ArduinoGfxBridge : public Adafruit_GFX {
public:
  ArduinoGfxBridge(uint16_t w, uint16_t h) : Adafruit_GFX(w, h) {}
  void attach(Arduino_GFX* driver) { _driver = driver; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void startWrite() override;
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
  void endWrite() override;
  void setRotation(uint8_t rotation) override;
  void invertDisplay(bool inverted) override;
  void fillScreen(uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;

private:
  Arduino_GFX* _driver = nullptr;
};

class DisplayPort {
public:
  bool begin();
  Adafruit_GFX& gfx();
  bool available() const { return _ready; }
  uint16_t width() const { return _gfx.width(); }
  uint16_t height() const { return _gfx.height(); }
  void clear(uint16_t color = COLOR_BG);
  void header(const char* title, const char* right = nullptr);
  void footer(const char* left, const char* right = nullptr);
  void message(const char* title, const char* body, uint16_t color = COLOR_TEXT);
  void setBrightness(uint8_t brightness);

private:
  Arduino_DataBus* _bus = nullptr;
  Arduino_SH8601* _amoled = nullptr;
  ArduinoGfxBridge _gfx{LCD_WIDTH, LCD_HEIGHT};
  Adafruit_XCA9554 _expander;
  bool _ready = false;
};

