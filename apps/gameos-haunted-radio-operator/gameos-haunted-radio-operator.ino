#include <SD_MMC.h>
#include <WaveshareAmoledAppKit.h>
#include <WaveshareAmoledAudio.h>

#include <string.h>

using namespace WaveshareAmoled;

namespace {

constexpr uint16_t kBandMin = 875;   // 87.5 MHz, represented in tenths.
constexpr uint16_t kBandMax = 1080;  // 108.0 MHz, represented in tenths.
constexpr uint16_t kTrackX = 20;
constexpr uint16_t kTrackY = 141;
constexpr uint16_t kTrackW = 328;
constexpr uint16_t kTrackH = 30;
constexpr const char* kStateDir = "/waveshare-os/games/haunted-radio";
constexpr const char* kStatePath = "/waveshare-os/games/haunted-radio/state.txt";
constexpr const char* kStaticClip = "/waveshare-os/audio/haunted-radio/static.wav";
constexpr const char* kBackgroundClip = "/waveshare-os/audio/haunted-radio/background.wav";

struct Station {
  uint16_t frequency;
  const char* call;
  const char* message;
  const char* clip;
};

constexpr Station kStations[] = {
    {883, "WCH-88.3", "A child counts backward beneath the flood siren.",
     "/waveshare-os/audio/haunted-radio/signal-01.wav"},
    {946, "KNOX-94.6", "The road crew asks who moved the mile marker.",
     "/waveshare-os/audio/haunted-radio/signal-02.wav"},
    {1017, "AMBER-101.7", "A late dispatcher repeats your name, then apologizes.",
     "/waveshare-os/audio/haunted-radio/signal-03.wav"},
    {1073, "LAST-107.3", "The final operator signs off before you arrive.",
     "/waveshare-os/audio/haunted-radio/signal-04.wav"},
};

enum Screen : uint8_t { RadioScreen, ArchiveScreen };

Screen screen = RadioScreen;
uint16_t frequency = 900;
uint8_t tape = 0;
uint8_t capturedMask = 0;
uint8_t stability = 45;
uint8_t captureProgress = 0;
uint32_t lastTick = 0;
uint32_t lastDraw = 0;
char statusLine[92] = "Sweep the band. Find a signal worth keeping.";

int nearestStation() {
  int nearest = 0;
  uint16_t closest = 0xffff;
  for (uint8_t i = 0; i < sizeof(kStations) / sizeof(kStations[0]); ++i) {
    const uint16_t delta = frequency > kStations[i].frequency
        ? frequency - kStations[i].frequency
        : kStations[i].frequency - frequency;
    if (delta < closest) {
      closest = delta;
      nearest = i;
    }
  }
  return nearest;
}

uint8_t signalQuality() {
  const uint16_t delta = frequency > kStations[nearestStation()].frequency
      ? frequency - kStations[nearestStation()].frequency
      : kStations[nearestStation()].frequency - frequency;
  if (delta >= 22) return 0;
  return static_cast<uint8_t>((22 - delta) * 100 / 22);
}

void ensureStateDir() {
  ensureDir(kRootPath);
  ensureDir("/waveshare-os/games");
  ensureDir(kStateDir);
}

void saveState() {
  if (!sdReady() && !mountSd()) return;
  ensureStateDir();
  const char* tempPath = "/waveshare-os/games/haunted-radio/state.tmp";
  SD_MMC.remove(tempPath);
  File file = SD_MMC.open(tempPath, FILE_WRITE);
  if (!file) return;
  file.printf("v=1 freq=%u tape=%u mask=%u\n", frequency, tape, capturedMask);
  file.close();
  SD_MMC.remove(kStatePath);
  SD_MMC.rename(tempPath, kStatePath);
}

int readField(const char* text, const char* name, int fallback) {
  const char* found = strstr(text, name);
  return found ? atoi(found + strlen(name)) : fallback;
}

void loadState() {
  if (!sdReady() && !mountSd()) return;
  File file = SD_MMC.open(kStatePath, FILE_READ);
  if (!file) return;
  char line[96] = "";
  const size_t length = file.readBytesUntil('\n', line, sizeof(line) - 1);
  file.close();
  line[length] = '\0';
  if (readField(line, "v=", 0) != 1) return;
  const int loadedFrequency = readField(line, "freq=", frequency);
  frequency = constrain(loadedFrequency, kBandMin, kBandMax);
  tape = constrain(readField(line, "tape=", tape), 0, 4);
  capturedMask = static_cast<uint8_t>(readField(line, "mask=", capturedMask) & 0x0f);
  snprintf(statusLine, sizeof(statusLine), "Night log restored. %u signal%s archived.",
           tape, tape == 1 ? "" : "s");
}

void playClip(const char* path) {
  if (path && sdReady() && SD_MMC.exists(path)) {
    WaveshareAmoledAudio::requestPlayWav(path);
  }
}

void startBackground() {
  if (sdReady() && SD_MMC.exists(kBackgroundClip)) {
    WaveshareAmoledAudio::loopWav(kBackgroundClip);
  }
}

void formatFrequency(char* out, size_t outSize) {
  snprintf(out, outSize, "%u.%u", frequency / 10, frequency % 10);
}

void drawMeter(int16_t x, int16_t y, int16_t width, const char* label,
               uint8_t value, uint16_t color) {
  Adafruit_GFX& display = WaveshareAmoled::display();
  display.setTextSize(1);
  display.setTextColor(kColorDim, kColorBg);
  display.setCursor(x, y);
  display.print(label);
  display.drawRoundRect(x, y + 13, width, 12, 5, kColorPanel2);
  const int16_t fill = static_cast<int16_t>((width - 4) * value / 100);
  if (fill > 0) display.fillRoundRect(x + 2, y + 15, fill, 8, 4, color);
}

void drawSpectrum(uint8_t quality) {
  Adafruit_GFX& display = WaveshareAmoled::display();
  const uint32_t phase = millis() / 70;
  for (uint8_t i = 0; i < 28; ++i) {
    const uint8_t noise = static_cast<uint8_t>((phase * 13 + i * 29 + quality * 3) % 31);
    const uint8_t lift = quality > 55 && i > 11 && i < 17 ? quality / 5 : 0;
    const int16_t height = 5 + noise + lift;
    const int16_t x = 20 + i * 12;
    display.drawFastVLine(x, 121 - height, height * 2, quality > 55 ? kColorAccent : kColorDim);
  }
}

void drawRadio() {
  const uint8_t quality = signalQuality();
  char right[18];
  snprintf(right, sizeof(right), "TAPE %u/4", tape);
  clear();
  header("Haunted Radio", right);
  char tuned[16];
  formatFrequency(tuned, sizeof(tuned));
  Adafruit_GFX& display = WaveshareAmoled::display();
  display.setTextSize(5);
  display.setTextColor(kColorText, kColorBg);
  display.setCursor(87, 78);
  display.print(tuned);
  display.setTextSize(2);
  display.setTextColor(kColorAccent, kColorBg);
  display.setCursor(244, 102);
  display.print("MHz");
  drawSpectrum(quality);

  display.drawRoundRect(kTrackX, kTrackY, kTrackW, kTrackH, 8, kColorPanel2);
  const int16_t marker = kTrackX + static_cast<int32_t>(frequency - kBandMin) * (kTrackW - 10) /
      (kBandMax - kBandMin);
  display.fillRoundRect(marker, kTrackY + 4, 10, kTrackH - 8, 5, kColorAccent);
  drawMeter(20, 184, 150, "SIGNAL", quality, quality > 55 ? kColorGood : kColorDim);
  drawMeter(198, 184, 150, "STABILITY", stability, kColorWarn);
  button(20, 231, 157, 58, "STABILIZE", false, kColorAccent);
  button(191, 231, 157, 58, "SCAN", false, kColorWarn);
  drawMeter(20, 312, 328, "CAPTURE", captureProgress, kColorGood);
  wrapped(statusLine, 20, 352, 30, 2, kColorText);
  footer("Tap dial / swipe tune", "Swipe left archive");
  lastDraw = millis();
}

void drawArchive() {
  clear();
  header("Signal Archive", "LOGBOOK");
  wrapped("Captured transmissions persist on the SD card. Each clean lock unlocks a new station record.",
          20, 70, 30, 3, kColorDim);
  for (uint8_t i = 0; i < sizeof(kStations) / sizeof(kStations[0]); ++i) {
    const bool captured = (capturedMask & (1 << i)) != 0;
    const int16_t y = 166 + i * 54;
    listItem(20, y, 328, 44, captured ? kStations[i].call : "UNRESOLVED",
             captured ? kStations[i].message : "Tune, stabilize, and capture this signal.",
             captured, captured ? kColorAccent : kColorPanel2);
  }
  footer("Swipe right to radio", "Hold BOOT home");
  lastDraw = millis();
}

void draw() {
  if (screen == RadioScreen) drawRadio();
  else drawArchive();
}

void tuneBy(int16_t change) {
  int32_t next = static_cast<int32_t>(frequency) + change;
  if (next < kBandMin) next = kBandMax;
  if (next > kBandMax) next = kBandMin;
  frequency = static_cast<uint16_t>(next);
  captureProgress = 0;
  const uint8_t quality = signalQuality();
  if (quality > 55) {
    snprintf(statusLine, sizeof(statusLine), "%s is almost clear. Stabilize the carrier.",
             kStations[nearestStation()].call);
  } else {
    snprintf(statusLine, sizeof(statusLine), "Static, weather, then someone breathing too close.");
  }
  playClip(kStaticClip);
}

void scan() {
  const int current = nearestStation();
  const int next = (current + 1) % (sizeof(kStations) / sizeof(kStations[0]));
  frequency = kStations[next].frequency;
  captureProgress = 0;
  stability = 42;
  snprintf(statusLine, sizeof(statusLine), "Scanning stopped at %s. Hold the frequency together.",
           kStations[next].call);
  playClip(kStaticClip);
}

void stabilize() {
  const uint8_t quality = signalQuality();
  if (quality < 55) {
    stability = stability > 6 ? stability - 6 : 0;
    captureProgress = 0;
    snprintf(statusLine, sizeof(statusLine), "Nothing holds. Bring the dial closer to a voice.");
    playClip(kStaticClip);
    return;
  }
  stability = min(100, stability + 13);
  const uint8_t gain = quality > 82 ? 18 : 11;
  captureProgress = min(100, captureProgress + gain);
  const int station = nearestStation();
  if (captureProgress < 100) {
    snprintf(statusLine, sizeof(statusLine), "%s: keep tapping before the carrier falls apart.",
             kStations[station].call);
    return;
  }
  captureProgress = 0;
  if ((capturedMask & (1 << station)) == 0) {
    capturedMask |= 1 << station;
    tape = min(4, tape + 1);
    snprintf(statusLine, sizeof(statusLine), "ARCHIVED: %s", kStations[station].message);
    playClip(kStations[station].clip);
    saveState();
  } else {
    snprintf(statusLine, sizeof(statusLine), "The tape is already warm with this transmission.");
    playClip(kStations[station].clip);
  }
}

void resetGame() {
  frequency = 900;
  tape = 0;
  capturedMask = 0;
  stability = 45;
  captureProgress = 0;
  snprintf(statusLine, sizeof(statusLine), "The old tapes are erased. The night starts again.");
  saveState();
}

void updateRadio() {
  const uint32_t now = millis();
  if (lastTick == 0) lastTick = now;
  if (now - lastTick < 1000) return;
  lastTick = now;
  if (stability > 0) stability--;
  if (captureProgress > 0) captureProgress = captureProgress > 3 ? captureProgress - 3 : 0;
}

void handleSerial(const char* line) {
  if (strcmp(line, "home") == 0) {
    returnToLauncher();
  } else if (strcmp(line, "scan") == 0) {
    scan();
  } else if (strcmp(line, "stabilize") == 0) {
    stabilize();
  } else if (strcmp(line, "archive") == 0) {
    screen = ArchiveScreen;
  } else if (strcmp(line, "radio") == 0) {
    screen = RadioScreen;
  } else if (strcmp(line, "reset") == 0) {
    resetGame();
  } else if (strncmp(line, "tune ", 5) == 0) {
    const int requested = atoi(line + 5);
    if (requested >= kBandMin && requested <= kBandMax) {
      frequency = static_cast<uint16_t>(requested);
      captureProgress = 0;
      snprintf(statusLine, sizeof(statusLine), "Dial set to %u.%u MHz.", frequency / 10, frequency % 10);
    }
  } else if (strcmp(line, "status") == 0 || strcmp(line, "help") == 0) {
    Serial.printf("haunted-radio freq=%u.%u quality=%u stability=%u capture=%u tape=%u mask=%u\n",
                  frequency / 10, frequency % 10, signalQuality(), stability,
                  captureProgress, tape, capturedMask);
    if (strcmp(line, "help") == 0) {
      serialHelp("help, status, tune 875-1080, scan, stabilize, archive, radio, reset, home");
    }
  }
}

}  // namespace

void setup() {
  begin("Haunted Radio", true);
  WaveshareAmoledAudio::begin();
  loadState();
  startBackground();
  serialHelp("help, status, tune 875-1080, scan, stabilize, archive, radio, reset, home");
  draw();
}

void loop() {
  WaveshareAmoledAudio::update();
  updateRadio();
  const Event event = poll();
  if (isHome(event)) returnToLauncher();

  if (event.type == EventSwipeLeft) {
    screen = ArchiveScreen;
    draw();
  } else if (event.type == EventSwipeRight) {
    screen = RadioScreen;
    draw();
  } else if (screen == RadioScreen && event.type == EventSwipeUp) {
    tuneBy(5);
    draw();
  } else if (screen == RadioScreen && event.type == EventSwipeDown) {
    tuneBy(-5);
    draw();
  } else if (screen == RadioScreen && event.type == EventTap) {
    if (hit(event, kTrackX, kTrackY - 12, kTrackW, kTrackH + 24)) {
      frequency = kBandMin + static_cast<uint32_t>(event.x - kTrackX) *
          (kBandMax - kBandMin) / kTrackW;
      captureProgress = 0;
      snprintf(statusLine, sizeof(statusLine), "Fine-tune with up/down swipes. Then stabilize.");
      playClip(kStaticClip);
    } else if (hit(event, 20, 231, 157, 58)) {
      stabilize();
    } else if (hit(event, 191, 231, 157, 58)) {
      scan();
    }
    draw();
  } else if (event.type == EventSerialLine) {
    handleSerial(event.line);
    draw();
  }

  if (millis() - lastDraw > 120) draw();
  delay(16);
}
