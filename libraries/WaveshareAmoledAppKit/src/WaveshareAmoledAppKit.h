#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <FS.h>

#include <WaveshareAmoledReturn.h>

namespace WaveshareAmoled {

constexpr uint32_t kSerialBaud = 115200;

constexpr uint16_t kDisplayWidth = 368;
constexpr uint16_t kDisplayHeight = 448;
constexpr uint8_t kDisplayRotation = 0;

constexpr int kPinLcdSdio0 = 4;
constexpr int kPinLcdSdio1 = 5;
constexpr int kPinLcdSdio2 = 6;
constexpr int kPinLcdSdio3 = 7;
constexpr int kPinLcdSclk = 11;
constexpr int kPinLcdCs = 12;
constexpr int kPinLcdRst = -1;

constexpr int kPinTouchSda = 15;
constexpr int kPinTouchScl = 14;
constexpr int kPinTouchRst = -1;
constexpr int kPinTouchInt = 21;

constexpr int kPinSdClk = 2;
constexpr int kPinSdCmd = 1;
constexpr int kPinSdD0 = 3;

constexpr int kPinBoot = 0;
constexpr bool kBootActiveLow = true;
constexpr uint8_t kExpanderAddress = 0x20;
constexpr uint8_t kDefaultBrightness = 220;

constexpr const char* kRootPath = "/waveshare-os";
constexpr const char* kAppsPath = "/waveshare-os/apps";
constexpr const char* kDeskPath = "/waveshare-os/desk";
constexpr const char* kGameOsPath = "/waveshare-os/cardputer-game-os";
constexpr const char* kMpcPath = "/waveshare-os/cardputer-mpc";

constexpr uint16_t kColorBg = 0x0000;
constexpr uint16_t kColorPanel = 0x1082;
constexpr uint16_t kColorPanel2 = 0x2104;
constexpr uint16_t kColorText = 0xFFFF;
constexpr uint16_t kColorDim = 0xBDF7;
constexpr uint16_t kColorAccent = 0x07FF;
constexpr uint16_t kColorGood = 0x07E0;
constexpr uint16_t kColorWarn = 0xFD20;
constexpr uint16_t kColorBad = 0xF800;

enum EventType : uint8_t {
  EventNone = 0,
  EventTap,
  EventLongPress,
  EventSwipeUp,
  EventSwipeDown,
  EventSwipeLeft,
  EventSwipeRight,
  EventBootShort,
  EventBootLong,
  EventSerialLine
};

struct Event {
  EventType type = EventNone;
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t startX = 0;
  uint16_t startY = 0;
  uint32_t durationMs = 0;
  char line[128] = "";
};

bool begin(const char* title = nullptr, bool mountCard = true);
bool mountSd();
bool sdReady();
bool touchReady();
bool bootDown();
void setBrightness(uint8_t brightness);
Adafruit_GFX& display();
Event poll();
const char* eventName(EventType type);

bool isBack(const Event& event);
bool isHome(const Event& event);
bool hit(const Event& event, int16_t x, int16_t y, int16_t w, int16_t h);
void returnToLauncher(uint32_t delayMs = 180);

void clear(uint16_t color = kColorBg);
void header(const char* title, const char* right = nullptr);
void footer(const char* left, const char* right = nullptr);
void button(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
            bool selected = false, uint16_t color = kColorText);
void listItem(int16_t x, int16_t y, int16_t w, int16_t h, const char* title,
              const char* subtitle, bool selected = false, uint16_t accent = kColorAccent);
void wrapped(const char* text, int16_t x, int16_t y, uint8_t maxChars,
             uint8_t maxLines, uint16_t color = kColorDim, uint16_t bg = kColorBg);
void message(const char* title, const char* body, uint16_t color = kColorText);
void serialHelp(const char* commands);

bool ensureDir(const char* path);
bool appendLine(const char* path, const char* line);

}  // namespace WaveshareAmoled
