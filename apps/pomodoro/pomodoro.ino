// Pomodoro — focus timer with classic 25/5/15 intervals, a tap Start/Pause +
// Reset UI, time-of-day from the RTC, and a cyclable accent theme color.
//
// Controls:
//   tap Start/Pause / Reset buttons
//   short BOOT press  -> cycle accent theme color (persisted)
//   long  BOOT press  -> return to launcher

#include <Preferences.h>

#include <WaveshareAmoledAppKit.h>
#include <WaveshareAmoledSensors.h>

// Smooth GFX fonts, vendored locally so the sketch is self-contained (the
// library Fonts/ path is not reliably on the include path under arduino-cli).
#include "FreeMonoBold24pt7b.h"
#include "FreeSansBold18pt7b.h"

namespace WA = WaveshareAmoled;
namespace WS = WaveshareAmoled::Sensors;

namespace {
uint8_t focusMin = 25;             // adjustable focus length (5..60, step 5)
constexpr uint8_t kFocusMinMin = 5;
constexpr uint8_t kFocusMinMax = 60;
constexpr uint32_t kShortMs = 5UL * 60 * 1000;
constexpr uint32_t kLongMs = 15UL * 60 * 1000;
constexpr uint8_t kLongEvery = 4;  // long break after every 4th focus block

uint32_t focusMs() { return static_cast<uint32_t>(focusMin) * 60UL * 1000UL; }

enum Phase { FOCUS, SHORT_BREAK, LONG_BREAK };
enum Run { IDLE, RUNNING, PAUSED, DONE };

Phase phase = FOCUS;
Run run = IDLE;
uint32_t remainingMs = 25UL * 60 * 1000;
uint32_t lastTick = 0;
uint8_t focusInCycle = 0;  // completed focus blocks toward the next long break

const uint16_t kThemes[] = {0x07FF, 0x07E0, 0xFD20, 0xF81F, 0xF800, 0xFFFF};
constexpr uint8_t kThemeN = sizeof(kThemes) / sizeof(kThemes[0]);
uint8_t themeIdx = 0;
Preferences prefs;

int lastSecShown = -1;
bool needFull = true;

uint16_t accent() { return kThemes[themeIdx]; }
uint32_t phaseDuration(Phase p) {
  return p == FOCUS ? focusMs() : (p == SHORT_BREAK ? kShortMs : kLongMs);
}
const char* phaseName(Phase p) {
  return p == FOCUS ? "FOCUS" : (p == SHORT_BREAK ? "BREAK" : "LONG BREAK");
}
const char* startLabel() {
  switch (run) {
    case RUNNING: return "Pause";
    case PAUSED: return "Resume";
    case DONE: return phase == FOCUS ? "Start Focus" : "Start Break";
    default: return "Start";
  }
}

void nextPhase() {
  if (phase == FOCUS) {
    focusInCycle++;
    phase = (focusInCycle >= kLongEvery) ? LONG_BREAK : SHORT_BREAK;
  } else if (phase == LONG_BREAK) {
    focusInCycle = 0;
    phase = FOCUS;
  } else {
    phase = FOCUS;
  }
  remainingMs = phaseDuration(phase);
}

void drawButtons() {
  WA::button(24, 300, 210, 80, startLabel(), run == RUNNING, accent());
  WA::button(250, 300, 94, 80, "Reset");
}

void drawPrompt() {
  Adafruit_GFX& d = WA::display();
  d.fillRect(0, 386, WA::kDisplayWidth, 24, WA::kColorBg);
  const char* msg = run == DONE ? "Interval done - tap Start"
                                : (run == PAUSED ? "Paused" : "");
  if (msg[0]) {
    d.setTextSize(2);
    d.setTextColor(WA::kColorDim);
    int16_t w = static_cast<int16_t>(strlen(msg)) * 12;
    d.setCursor((WA::kDisplayWidth - w) / 2, 388);
    d.print(msg);
  }
}

// Draws a string centered horizontally with its top edge at topY, using a
// smooth GFX font, then restores the built-in font for everything else.
void drawCenteredFont(const char* s, const GFXfont* font, int16_t topY,
                      uint16_t color) {
  Adafruit_GFX& d = WA::display();
  d.setFont(font);
  d.setTextSize(1);
  d.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  d.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  d.setCursor((WA::kDisplayWidth - static_cast<int16_t>(w)) / 2 - x1, topY - y1);
  d.print(s);
  d.setFont();  // back to the built-in font for AppKit widgets
}

void drawCountdown() {
  Adafruit_GFX& d = WA::display();
  d.fillRect(10, 150, WA::kDisplayWidth - 20, 92, WA::kColorBg);
  const uint32_t mm = remainingMs / 60000;
  const uint32_t ss = (remainingMs % 60000) / 1000;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", static_cast<unsigned>(mm),
           static_cast<unsigned>(ss));
  drawCenteredFont(buf, &FreeMonoBold24pt7b, 166, accent());
}

void drawStatic() {
  Adafruit_GFX& d = WA::display();
  WA::clear();
  WA::header("Pomodoro");
  WS::drawStatusChrome(d, WA::kDisplayWidth);  // battery + time-of-day

  drawCenteredFont(phaseName(phase), &FreeSansBold18pt7b, 58, accent());

  const int16_t startX = (WA::kDisplayWidth - (3 * 30)) / 2;
  for (int i = 0; i < kLongEvery; i++) {
    const int16_t cx = startX + i * 30;
    if (i < focusInCycle) {
      d.fillCircle(cx, 122, 8, accent());
    } else {
      d.drawCircle(cx, 122, 8, WA::kColorDim);
    }
  }

  // Focus-length editor, only while idle (can't change a running interval).
  if (run == IDLE) {
    WA::button(40, 248, 60, 44, "-");
    WA::button(268, 248, 60, 44, "+");
    char fb[20];
    snprintf(fb, sizeof(fb), "Focus %um", focusMin);
    d.setTextSize(2);
    d.setTextColor(WA::kColorDim);
    d.setCursor((WA::kDisplayWidth - static_cast<int16_t>(strlen(fb)) * 12) / 2, 260);
    d.print(fb);
  }

  drawButtons();
  drawPrompt();
  WA::footer("BOOT: color", "hold: home");
  lastSecShown = -1;  // force countdown repaint
}

void alert() {
  for (int i = 0; i < 6; i++) {
    WA::clear((i % 2) ? accent() : WA::kColorBg);
    delay(120);
  }
  WS::Audio::beep();  // attempt; silent until the codec path is verified
}

void handleTap(const WA::Event& e) {
  if (WA::hit(e, 24, 300, 210, 80)) {
    if (run == RUNNING) {
      run = PAUSED;
    } else {
      run = RUNNING;  // covers IDLE, PAUSED, DONE (next interval already loaded)
      lastTick = millis();
    }
    needFull = true;
  } else if (WA::hit(e, 250, 300, 94, 80)) {
    phase = FOCUS;
    focusInCycle = 0;
    remainingMs = focusMs();
    run = IDLE;
    needFull = true;
  } else if (run == IDLE && WA::hit(e, 40, 248, 60, 44)) {
    if (focusMin > kFocusMinMin) focusMin -= 5;
    remainingMs = focusMs();
    prefs.putUChar("focusMin", focusMin);
    needFull = true;
  } else if (run == IDLE && WA::hit(e, 268, 248, 60, 44)) {
    if (focusMin < kFocusMinMax) focusMin += 5;
    remainingMs = focusMs();
    prefs.putUChar("focusMin", focusMin);
    needFull = true;
  }
}
}  // namespace

void setup() {
  WA::begin("Pomodoro");
  WS::begin();
  prefs.begin("pomodoro", false);
  themeIdx = prefs.getUChar("theme", 0);
  if (themeIdx >= kThemeN) themeIdx = 0;
  focusMin = prefs.getUChar("focusMin", 25);
  if (focusMin < kFocusMinMin || focusMin > kFocusMinMax) focusMin = 25;
  remainingMs = focusMs();
  lastTick = millis();
  needFull = true;
}

void loop() {
  WA::Event e = WA::poll();
  if (WA::isHome(e)) WA::returnToLauncher();
  if (e.type == WA::EventBootShort) {
    themeIdx = (themeIdx + 1) % kThemeN;
    prefs.putUChar("theme", themeIdx);
    needFull = true;
  }
  if (e.type == WA::EventTap) handleTap(e);

  if (run == RUNNING) {
    const uint32_t now = millis();
    const uint32_t dt = now - lastTick;
    lastTick = now;
    if (dt >= remainingMs) {
      remainingMs = 0;
    } else {
      remainingMs -= dt;
    }
    if (remainingMs == 0) {
      nextPhase();
      run = DONE;
      alert();
      needFull = true;
    }
  }

  if (needFull) {
    drawStatic();
    needFull = false;
  }
  const int secs = static_cast<int>(remainingMs / 1000);
  if (secs != lastSecShown) {
    drawCountdown();
    lastSecShown = secs;
  }
  delay(100);
}
