#include "WaveshareAmoledAppKit.h"
#include "WaveshareAmoledIntro.h"

#include <Adafruit_XCA9554.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <SD_MMC.h>
#include <Wire.h>

#include <memory>

namespace WaveshareAmoled {
namespace {

class GfxBridge : public Adafruit_GFX {
 public:
  GfxBridge() : Adafruit_GFX(kDisplayWidth, kDisplayHeight) {}

  void attach(Arduino_DataBus* bus, Arduino_SH8601* driver) {
    _bus = bus;
    _driver = driver;
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (_driver) _driver->drawPixel(x, y, color);
  }

  void startWrite() override {
    if (_driver) _driver->startWrite();
  }

  void writePixel(int16_t x, int16_t y, uint16_t color) override {
    if (_driver) _driver->writePixel(x, y, color);
  }

  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                     uint16_t color) override {
    if (_driver) _driver->writeFillRect(x, y, w, h, color);
  }

  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    if (_driver) _driver->writeFastVLine(x, y, h, color);
  }

  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    if (_driver) _driver->writeFastHLine(x, y, w, color);
  }

  void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 uint16_t color) override {
    if (_driver) _driver->writeLine(x0, y0, x1, y1, color);
  }

  void endWrite() override {
    if (_driver) _driver->endWrite();
  }

  void setRotation(uint8_t rotation) override {
    Adafruit_GFX::setRotation(rotation);
    if (_driver) _driver->setRotation(rotation);
  }

  void invertDisplay(bool inverted) override {
    if (_driver) _driver->invertDisplay(inverted);
  }

  void fillScreen(uint16_t color) {
    if (_driver) _driver->fillScreen(color);
  }

 private:
  Arduino_DataBus* _bus = nullptr;
  Arduino_SH8601* _driver = nullptr;
};

Adafruit_XCA9554 expander;
Arduino_DataBus* bus = nullptr;
Arduino_SH8601* amoled = nullptr;
GfxBridge gfx;
std::shared_ptr<Arduino_IIC_DriveBus> touchBus;
std::unique_ptr<Arduino_FT3x68> touchDevice;
bool displayOk = false;
bool touchOk = false;
bool cardOk = false;
bool touchWasDown = false;
bool bootWasDown = false;
bool bootLongSent = false;
uint16_t touchStartX = 0;
uint16_t touchStartY = 0;
uint16_t touchLastX = 0;
uint16_t touchLastY = 0;
uint32_t touchStartAt = 0;
uint32_t bootStartAt = 0;
char serialBuf[128] = "";
uint8_t serialLen = 0;

void remapTouch(int32_t rawX, int32_t rawY, uint16_t& outX, uint16_t& outY) {
  int32_t x = rawX;
  int32_t y = rawY;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= kDisplayWidth) x = kDisplayWidth - 1;
  if (y >= kDisplayHeight) y = kDisplayHeight - 1;
  outX = static_cast<uint16_t>(x);
  outY = static_cast<uint16_t>(y);
}

bool readTouch(uint16_t& x, uint16_t& y) {
  if (!touchOk || !touchDevice) return false;
  const int32_t fingers = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(
          touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
  if (fingers <= 0) return false;

  const int32_t rawX = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(
          touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
  const int32_t rawY = static_cast<int32_t>(
      touchDevice->IIC_Read_Device_Value(
          touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
  remapTouch(rawX, rawY, x, y);
  return true;
}

bool pollSerial(Event& event) {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      serialBuf[serialLen] = '\0';
      serialLen = 0;
      event.type = EventSerialLine;
      strncpy(event.line, serialBuf, sizeof(event.line) - 1);
      event.line[sizeof(event.line) - 1] = '\0';
      return true;
    }
    if (serialLen < sizeof(serialBuf) - 1) serialBuf[serialLen++] = c;
  }
  return false;
}

}  // namespace

bool begin(const char* title, bool mountCard) {
  if (!Serial) Serial.begin(kSerialBaud);
  delay(30);
  pinMode(kPinBoot, INPUT_PULLUP);
  Wire.begin(kPinTouchSda, kPinTouchScl);

  if (!expander.begin(kExpanderAddress, &Wire)) {
    Serial.println("[appkit] XCA9554 not found; continuing");
  } else {
    for (uint8_t pin = 0; pin < 3; pin++) {
      expander.pinMode(pin, OUTPUT);
      expander.digitalWrite(pin, LOW);
    }
    expander.pinMode(7, OUTPUT);
    expander.digitalWrite(7, HIGH);
    delay(20);
    for (uint8_t pin = 0; pin < 3; pin++) expander.digitalWrite(pin, HIGH);
  }

  if (!bus) {
    bus = new Arduino_ESP32QSPI(kPinLcdCs, kPinLcdSclk, kPinLcdSdio0,
                                kPinLcdSdio1, kPinLcdSdio2, kPinLcdSdio3);
  }
  if (!amoled) {
    amoled = new Arduino_SH8601(bus, kPinLcdRst, kDisplayRotation,
                                kDisplayWidth, kDisplayHeight);
  }
  displayOk = amoled && amoled->begin();
  if (displayOk) {
    amoled->setBrightness(kDefaultBrightness);
    gfx.attach(bus, amoled);
    gfx.setRotation(kDisplayRotation);
    gfx.setTextWrap(false);
    gfx.setTextSize(2);
    gfx.setTextColor(kColorText, kColorBg);
    clear();
    WaveshareAmoledIntro::draw(gfx, kDisplayWidth, kDisplayHeight,
                               title ? title : "Cypher Cube");
    if (title) header(title);
  } else {
    Serial.println("[appkit] SH8601 init failed");
  }

  touchBus = std::make_shared<Arduino_HWIIC>(kPinTouchSda, kPinTouchScl, &Wire);
  touchDevice.reset(new Arduino_FT3x68(touchBus, 0x38, DRIVEBUS_DEFAULT_VALUE,
                                       kPinTouchInt));
  touchOk = false;
  for (uint8_t tries = 0; tries < 5; tries++) {
    if (touchDevice->begin()) {
      touchDevice->IIC_Write_Device_State(
          touchDevice->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
          touchDevice->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
      touchOk = true;
      break;
    }
    delay(220);
  }
  Serial.printf("[appkit] display=%d touch=%d\n", displayOk, touchOk);

  if (mountCard) mountSd();
  return displayOk;
}

bool mountSd() {
  SD_MMC.end();
  pinMode(kPinSdClk, INPUT_PULLUP);
  pinMode(kPinSdCmd, INPUT_PULLUP);
  pinMode(kPinSdD0, INPUT_PULLUP);
  SD_MMC.setPins(kPinSdClk, kPinSdCmd, kPinSdD0);
  const uint32_t freqs[] = {25000, 20000, 10000, 4000};
  cardOk = false;
  for (uint8_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
    cardOk = SD_MMC.begin("/sdcard", true, false, freqs[i]);
    if (cardOk) break;
    SD_MMC.end();
    delay(80);
  }
  Serial.printf("[appkit] sd=%d\n", cardOk);
  return cardOk;
}

bool sdReady() {
  return cardOk;
}

bool touchReady() {
  return touchOk;
}

bool bootDown() {
  const int raw = digitalRead(kPinBoot);
  return kBootActiveLow ? raw == LOW : raw == HIGH;
}

void setBrightness(uint8_t brightness) {
  if (amoled) amoled->setBrightness(brightness);
}

Adafruit_GFX& display() {
  return gfx;
}

Event poll() {
  Event event;
  if (pollSerial(event)) return event;

  const bool boot = bootDown();
  if (boot && !bootWasDown) {
    bootWasDown = true;
    bootLongSent = false;
    bootStartAt = millis();
  } else if (boot && bootWasDown && !bootLongSent &&
             millis() - bootStartAt >= 900) {
    bootLongSent = true;
    event.type = EventBootLong;
    event.durationMs = millis() - bootStartAt;
    return event;
  } else if (!boot && bootWasDown) {
    bootWasDown = false;
    if (!bootLongSent) {
      event.type = EventBootShort;
      event.durationMs = millis() - bootStartAt;
      return event;
    }
  }

  uint16_t x = 0;
  uint16_t y = 0;
  const bool down = readTouch(x, y);
  if (down && !touchWasDown) {
    touchWasDown = true;
    touchStartX = x;
    touchStartY = y;
    touchLastX = x;
    touchLastY = y;
    touchStartAt = millis();
  } else if (down && touchWasDown) {
    touchLastX = x;
    touchLastY = y;
  } else if (!down && touchWasDown) {
    touchWasDown = false;
    const int16_t dx = static_cast<int16_t>(touchLastX) -
                       static_cast<int16_t>(touchStartX);
    const int16_t dy = static_cast<int16_t>(touchLastY) -
                       static_cast<int16_t>(touchStartY);
    const uint32_t duration = millis() - touchStartAt;
    event.x = touchLastX;
    event.y = touchLastY;
    event.startX = touchStartX;
    event.startY = touchStartY;
    event.durationMs = duration;

    if (abs(dx) > 52 || abs(dy) > 52) {
      if (abs(dx) > abs(dy)) {
        event.type = dx > 0 ? EventSwipeRight : EventSwipeLeft;
      } else {
        event.type = dy > 0 ? EventSwipeDown : EventSwipeUp;
      }
    } else if (duration >= 650) {
      event.type = EventLongPress;
    } else {
      event.type = EventTap;
    }
    return event;
  }

  return event;
}

const char* eventName(EventType type) {
  switch (type) {
    case EventTap: return "tap";
    case EventLongPress: return "long";
    case EventSwipeUp: return "swipe-up";
    case EventSwipeDown: return "swipe-down";
    case EventSwipeLeft: return "swipe-left";
    case EventSwipeRight: return "swipe-right";
    case EventBootShort: return "boot-short";
    case EventBootLong: return "boot-long";
    case EventSerialLine: return "serial";
    default: return "none";
  }
}

bool isBack(const Event& event) {
  return event.type == EventBootShort || event.type == EventSwipeRight;
}

bool isHome(const Event& event) {
  return event.type == EventBootLong;
}

bool hit(const Event& event, int16_t x, int16_t y, int16_t w, int16_t h) {
  if (event.type != EventTap && event.type != EventLongPress) return false;
  return event.x >= x && event.x < x + w && event.y >= y && event.y < y + h;
}

void returnToLauncher(uint32_t delayMs) {
  WaveshareAmoledOs::returnToLauncher(delayMs);
}

void clear(uint16_t color) {
  gfx.fillScreen(color);
}

void header(const char* title, const char* right) {
  gfx.fillRect(0, 0, kDisplayWidth, 42, 0x0186);
  gfx.setTextSize(2);
  gfx.setTextColor(kColorText, 0x0186);
  gfx.setCursor(14, 13);
  gfx.print(title ? title : "");
  if (right && right[0]) {
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t tw = 0;
    uint16_t th = 0;
    gfx.getTextBounds(right, 0, 0, &x1, &y1, &tw, &th);
    gfx.setCursor(max(14, static_cast<int>(kDisplayWidth) - static_cast<int>(tw) - 14), 13);
    gfx.print(right);
  }
}

void footer(const char* left, const char* right) {
  gfx.fillRect(0, kDisplayHeight - 34, kDisplayWidth, 34, kColorPanel);
  gfx.drawFastHLine(0, kDisplayHeight - 35, kDisplayWidth, 0x39E7);
  gfx.setTextSize(2);
  gfx.setTextColor(kColorDim, kColorPanel);
  gfx.setCursor(14, kDisplayHeight - 24);
  gfx.print(left ? left : "");
  if (right && right[0]) {
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t tw = 0;
    uint16_t th = 0;
    gfx.getTextBounds(right, 0, 0, &x1, &y1, &tw, &th);
    gfx.setCursor(max(14, static_cast<int>(kDisplayWidth) - static_cast<int>(tw) - 14),
                  kDisplayHeight - 24);
    gfx.print(right);
  }
}

void button(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
            bool selected, uint16_t color) {
  const uint16_t bg = selected ? kColorAccent : kColorPanel;
  gfx.fillRoundRect(x, y, w, h, 8, bg);
  gfx.drawRoundRect(x, y, w, h, 8, selected ? kColorText : kColorPanel2);
  gfx.setTextSize(2);
  gfx.setTextColor(selected ? kColorBg : color, bg);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t tw = 0;
  uint16_t th = 0;
  gfx.getTextBounds(label ? label : "", 0, 0, &x1, &y1, &tw, &th);
  gfx.setCursor(x + max(8, (w - static_cast<int>(tw)) / 2), y + (h - 14) / 2);
  gfx.print(label ? label : "");
}

void listItem(int16_t x, int16_t y, int16_t w, int16_t h, const char* title,
              const char* subtitle, bool selected, uint16_t accent) {
  const uint16_t bg = selected ? kColorPanel2 : kColorPanel;
  gfx.fillRoundRect(x, y, w, h, 8, bg);
  gfx.drawRoundRect(x, y, w, h, 8, selected ? accent : kColorPanel2);
  gfx.fillRect(x + 8, y + 10, 4, h - 20, selected ? accent : kColorDim);
  gfx.setTextSize(2);
  gfx.setTextColor(kColorText, bg);
  gfx.setCursor(x + 20, y + 12);
  gfx.print(title ? title : "");
  gfx.setTextColor(kColorDim, bg);
  gfx.setCursor(x + 20, y + 39);
  gfx.print(subtitle ? subtitle : "");
}

void wrapped(const char* text, int16_t x, int16_t y, uint8_t maxChars,
             uint8_t maxLines, uint16_t color, uint16_t bg) {
  gfx.setTextSize(2);
  gfx.setTextColor(color, bg);
  const char* p = text ? text : "";
  for (uint8_t line = 0; line < maxLines && *p; line++) {
    char buf[42];
    uint8_t len = 0;
    const char* start = p;
    const char* lastSpace = nullptr;
    while (*p && *p != '\n' && len < maxChars) {
      if (*p == ' ') lastSpace = p;
      p++;
      len++;
    }
    if (*p && *p != '\n' && lastSpace && lastSpace > start) {
      len = static_cast<uint8_t>(lastSpace - start);
      p = lastSpace + 1;
    } else if (*p == '\n') {
      p++;
    }
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    gfx.setCursor(x, y + line * 26);
    gfx.print(buf);
  }
}

void message(const char* title, const char* body, uint16_t color) {
  clear();
  header(title);
  wrapped(body, 18, 72, 29, 11, color);
  footer("BOOT back", "Hold BOOT home");
}

void serialHelp(const char* commands) {
  Serial.println();
  Serial.println("[appkit] serial fallback commands:");
  Serial.println(commands ? commands : "help, home");
}

bool ensureDir(const char* path) {
  if (!cardOk && !mountSd()) return false;
  if (SD_MMC.exists(path)) return true;
  return SD_MMC.mkdir(path);
}

bool appendLine(const char* path, const char* line) {
  if (!cardOk && !mountSd()) return false;
  File file = SD_MMC.open(path, FILE_APPEND);
  if (!file) return false;
  file.println(line ? line : "");
  file.close();
  return true;
}

}  // namespace WaveshareAmoled
