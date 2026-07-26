#include "Launcher.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

#include <WaveshareAmoledSensors.h>

#include "BoardConfig.h"
#include "DisplayPort.h"
#include "SerialShell.h"
#include "TouchInput.h"

namespace WSensors = WaveshareAmoled::Sensors;

namespace {
struct AppEntry {
  char name[40];
  char slug[32];
  char binary[72];
  char version[24];
  char notes[128];
  char status[32];
  bool installable = true;
};

enum ScreenMode {
  SCREEN_HOME,
  SCREEN_APPS,
  SCREEN_SETTINGS,
  SCREEN_SOUND,
  SCREEN_SET_TIME,
  SCREEN_PICK_BOOT_APP,
  SCREEN_INFO,
  SCREEN_CONFIRM_INSTALL,
  SCREEN_MESSAGE
};

struct TapEvent {
  bool tapped = false;
  uint16_t x = 0;
  uint16_t y = 0;
};

DisplayPort display;
TouchInput touch;
Preferences prefs;
AppEntry apps[MAX_APPS];
uint8_t appCount = 0;
uint8_t selectedHome = 0;
uint8_t selectedApp = 0;
uint8_t appScroll = 0;
ScreenMode screenMode = SCREEN_HOME;
bool sdMounted = false;
bool appPartitionValid = false;
bool bootToApp = false;
bool touchWasDown = false;
uint8_t brightness = AMOLED_BRIGHTNESS;
uint8_t soundVolume = 70;
bool soundMuted = false;
char lastInstalled[40] = "";
char lastSlug[32] = "";
uint32_t lastSize = 0;
char statusLine[96] = "";
char messageTitle[40] = "";
char messageBody[180] = "";
char serialLine[160] = "";
uint8_t serialLineLen = 0;
bool bootWasDown = false;
uint32_t bootDownAt = 0;

// Screen timeout / sleep.
uint16_t screenTimeoutSec = 60;  // 0 = never
uint32_t lastActivityAt = 0;
bool screenAsleep = false;
bool swallowBootRelease = false;

// Default auto-boot app (slug). Empty = boot whatever is installed.
char defaultBootSlug[32] = "";

// Motion gestures (flip-to-sleep, shake-to-home).
bool imuGesturesOn = false;

// Working copy while editing the clock on SCREEN_SET_TIME.
WSensors::DateTime editTime;
uint8_t timeField = 0;  // 0=Y 1=M 2=D 3=H 4=Min

// Forward declarations for functions referenced before their definition.
void redraw();
bool installSelectedApp(bool force);
void noteActivity();
bool relaunchMatchesInstalled();
void goHome();

constexpr uint8_t INTRO_RAIN_COLS = 28;
constexpr uint16_t INTRO_BG = 0x0000;
constexpr uint16_t INTRO_PLATE = 0x0021;
constexpr uint16_t INTRO_PLATE_EDGE = 0x05FF;
constexpr uint16_t INTRO_MATRIX_DIM = 0x0320;
constexpr uint16_t INTRO_MATRIX_MID = 0x07E0;
constexpr uint16_t INTRO_MATRIX_HEAD = 0x87FF;
constexpr uint16_t INTRO_SCAN = 0xFFE0;

int16_t introRainY[INTRO_RAIN_COLS];
uint8_t introRainSpeed[INTRO_RAIN_COLS];
bool introRainReady = false;

void copyField(char* dest, size_t destSize, const char* source) {
  if (destSize == 0) return;
  if (!source) source = "";
  strncpy(dest, source, destSize - 1);
  dest[destSize - 1] = '\0';
}

void setStatus(const char* text) {
  copyField(statusLine, sizeof(statusLine), text);
  Serial.printf("[status] %s\n", statusLine);
}

void showMessage(const char* title, const char* body) {
  copyField(messageTitle, sizeof(messageTitle), title);
  copyField(messageBody, sizeof(messageBody), body);
  screenMode = SCREEN_MESSAGE;
}

const esp_partition_t* launcherPartition() {
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0");
  if (!part) {
    part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  }
  return part;
}

const esp_partition_t* appPartition() {
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "app1");
  if (!part) {
    part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  }
  return part;
}

bool setBootPartition(const esp_partition_t* part) {
  if (!part) return false;
  esp_err_t err = esp_ota_set_boot_partition(part);
  if (err != ESP_OK) {
    Serial.printf("[boot] set boot partition failed: %s\n", esp_err_to_name(err));
    return false;
  }
  Serial.printf("[boot] next boot: %s @ 0x%06X\n", part->label, part->address);
  return true;
}

bool readAppMagic() {
  const esp_partition_t* part = appPartition();
  if (!part) return false;
  uint8_t magic = 0;
  if (esp_partition_read(part, 0, &magic, 1) != ESP_OK) return false;
  return magic == ESP_APP_IMAGE_MAGIC;
}

void loadPrefs() {
  prefs.begin(PREF_NS, false);
  brightness = prefs.getUChar("bright", AMOLED_BRIGHTNESS);
  soundVolume = prefs.getUChar("soundVol", 70);
  if (soundVolume > 100) soundVolume = 70;
  soundMuted = prefs.getBool("soundMute", false);
  bootToApp = prefs.getBool("bootToApp", false);
  if (prefs.getBool("returnOnce", false)) {
    prefs.putBool("returnOnce", false);
    bootToApp = false;
    prefs.putBool("bootToApp", false);
  }
  String installed = prefs.getString("lastApp", "");
  copyField(lastInstalled, sizeof(lastInstalled), installed.c_str());
  copyField(lastSlug, sizeof(lastSlug), prefs.getString("lastSlug", "").c_str());
  lastSize = prefs.getULong("lastSize", 0);
  screenTimeoutSec = prefs.getUShort("scrnTo", 60);
  copyField(defaultBootSlug, sizeof(defaultBootSlug),
            prefs.getString("bootSlug", "").c_str());
  imuGesturesOn = prefs.getBool("imuGest", false);
}

void savePrefs() {
  prefs.putUChar("bright", brightness);
  prefs.putUChar("soundVol", soundVolume);
  prefs.putBool("soundMute", soundMuted);
  prefs.putBool("bootToApp", bootToApp);
  prefs.putString("lastApp", lastInstalled);
  prefs.putString("lastSlug", lastSlug);
  prefs.putULong("lastSize", lastSize);
  prefs.putUShort("scrnTo", screenTimeoutSec);
  prefs.putString("bootSlug", defaultBootSlug);
  prefs.putBool("imuGest", imuGesturesOn);
}

bool bootButtonDown() {
  const int raw = digitalRead(PIN_BOOT_BUTTON);
  return BOOT_BUTTON_ACTIVE_LOW ? raw == LOW : raw == HIGH;
}

bool forceLauncherAtBoot() {
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  delay(25);
  if (!bootButtonDown()) return false;
  const uint32_t start = millis();
  while (millis() - start < 650) {
    if (!bootButtonDown()) return false;
    delay(20);
  }
  return true;
}

bool mountSd() {
  SD_MMC.end();
  pinMode(PIN_SD_CLK, INPUT_PULLUP);
  pinMode(PIN_SD_CMD, INPUT_PULLUP);
  pinMode(PIN_SD_D0, INPUT_PULLUP);
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  const uint32_t freqs[] = {25000, 20000, 10000, 4000};
  sdMounted = false;
  for (uint8_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
    sdMounted = SD_MMC.begin("/sdcard", true, false, freqs[i]);
    if (sdMounted) break;
    SD_MMC.end();
    delay(80);
  }
  if (!sdMounted) {
    setStatus("SD card not mounted");
    return false;
  }
  Serial.printf("[sd] mounted type=%u size=%llu MB\n", SD_MMC.cardType(),
                SD_MMC.cardSize() / (1024ULL * 1024ULL));
  return true;
}

bool loadManifest() {
  appCount = 0;
  selectedApp = 0;
  appScroll = 0;
  if (!sdMounted && !mountSd()) return false;

  File file = SD_MMC.open(APP_MANIFEST, FILE_READ);
  if (!file) {
    setStatus("apps.json missing");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    setStatus("apps.json parse failed");
    Serial.printf("[apps] manifest error: %s\n", err.c_str());
    return false;
  }

  JsonArray list = doc["apps"].as<JsonArray>();
  for (JsonObject item : list) {
    if (appCount >= MAX_APPS) break;
    AppEntry& app = apps[appCount++];
    copyField(app.name, sizeof(app.name), item["name"] | "Unnamed");
    copyField(app.slug, sizeof(app.slug), item["slug"] | "");
    copyField(app.binary, sizeof(app.binary), item["binary"] | "");
    copyField(app.version, sizeof(app.version), item["version"] | "");
    copyField(app.notes, sizeof(app.notes), item["notes"] | "");
    copyField(app.status, sizeof(app.status), item["status"] | "ready");
    app.installable = item["installable"] | true;
  }

  char buf[64];
  snprintf(buf, sizeof(buf), "Loaded %u app(s)", appCount);
  setStatus(buf);
  return appCount > 0;
}

void resetIntroRain() {
  randomSeed(micros());
  for (uint8_t i = 0; i < INTRO_RAIN_COLS; i++) {
    introRainY[i] = random(-static_cast<int>(display.height()), 0);
    introRainSpeed[i] = random(7, 15);
  }
  introRainReady = true;
}

void drawIntroRain(Adafruit_GFX& d) {
  if (!introRainReady) resetIntroRain();
  static constexpr char glyphs[] = "01#*+<>/";
  const int16_t colW = max(8, static_cast<int>(display.width()) / INTRO_RAIN_COLS);
  d.setTextSize(1);
  for (uint8_t col = 0; col < INTRO_RAIN_COLS; col++) {
    const int16_t x = col * colW + (colW / 2);
    for (uint8_t tail = 0; tail < 8; tail++) {
      const int16_t y = introRainY[col] - tail * 14;
      if (y < -8 || y >= static_cast<int>(display.height())) continue;
      const uint16_t color = tail == 0 ? INTRO_MATRIX_HEAD :
                             tail < 3 ? INTRO_MATRIX_MID : INTRO_MATRIX_DIM;
      d.setTextColor(color);
      d.setCursor(x, y);
      d.write(glyphs[(col + tail + (millis() / 120)) % (sizeof(glyphs) - 1)]);
    }
    introRainY[col] += introRainSpeed[col];
    if (introRainY[col] - 110 > static_cast<int>(display.height())) {
      introRainY[col] = random(-80, 0);
      introRainSpeed[col] = random(7, 15);
    }
  }
}

int16_t centeredTextX(Adafruit_GFX& d, const char* text, uint8_t textSize) {
  d.setTextSize(textSize);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t tw = 0;
  uint16_t th = 0;
  d.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
  return max(0, (static_cast<int>(display.width()) - static_cast<int>(tw)) / 2);
}

void drawStartupIntro() {
  if (!display.available()) return;

  resetIntroRain();
  Adafruit_GFX& d = display.gfx();
  static constexpr const char* title = "CYPHER CUBE";
  static constexpr const char* credit = "by littlehakr";
  static constexpr uint32_t DURATION_MS = 4000;
  static constexpr uint32_t FRAME_MS = 70;
  static constexpr uint32_t REVEAL_MS = 1800;
  static constexpr uint32_t TYPE_START_MS = 2000;
  static constexpr uint32_t TYPE_MS = 1150;

  const int16_t plateX = 18;
  const int16_t plateY = 148;
  const int16_t plateW = display.width() - plateX * 2;
  const int16_t plateH = 150;
  const int16_t titleY = plateY + 44;
  const int16_t creditY = plateY + 104;

  d.setTextSize(4);
  int16_t bx = 0;
  int16_t by = 0;
  uint16_t titleW = 0;
  uint16_t titleH = 0;
  d.getTextBounds(title, 0, 0, &bx, &by, &titleW, &titleH);
  const int16_t titleX = centeredTextX(d, title, 4);
  const int16_t creditX = centeredTextX(d, credit, 2);
  const uint32_t startedAt = millis();

  d.fillScreen(INTRO_BG);
  drawIntroRain(d);

  while (millis() - startedAt < DURATION_MS) {
    const uint32_t frameStart = millis();
    const uint32_t elapsed = frameStart - startedAt;

    d.fillRoundRect(plateX, plateY, plateW, plateH, 8, INTRO_PLATE);
    d.drawRoundRect(plateX, plateY, plateW, plateH, 8, INTRO_PLATE_EDGE);
    d.drawFastHLine(plateX + 14, plateY + 22, plateW - 28, INTRO_MATRIX_DIM);
    d.drawFastHLine(plateX + 14, plateY + plateH - 22, plateW - 28, INTRO_MATRIX_DIM);

    const uint32_t revealElapsed = min(elapsed, REVEAL_MS);
    const int16_t revealW = (static_cast<uint32_t>(titleW + 12) * revealElapsed) / REVEAL_MS;
    const int16_t beamX = titleX + revealW;

    d.setTextSize(4);
    d.setTextColor(COLOR_TEXT);
    d.setCursor(titleX, titleY);
    d.print(title);
    if (revealW < static_cast<int>(titleW + 12)) {
      d.fillRect(beamX, titleY - 4,
                 display.width() - beamX - plateX, static_cast<int>(titleH) + 12,
                 INTRO_PLATE);
      d.drawFastVLine(beamX, titleY - 8, titleH + 18, INTRO_SCAN);
      if (beamX + 2 < static_cast<int>(display.width()) - plateX) {
        d.drawFastVLine(beamX + 2, titleY - 4, titleH + 10, INTRO_MATRIX_HEAD);
      }
    }

    if (elapsed >= TYPE_START_MS) {
      char shown[sizeof("by littlehakr")];
      const uint32_t typeElapsed = min(elapsed - TYPE_START_MS, TYPE_MS);
      uint8_t shownLen = (strlen(credit) * typeElapsed) / TYPE_MS;
      if (shownLen > strlen(credit)) shownLen = strlen(credit);
      memcpy(shown, credit, shownLen);
      shown[shownLen] = '\0';
      d.setTextSize(2);
      d.setTextColor(INTRO_MATRIX_HEAD);
      d.setCursor(creditX, creditY);
      d.print(shown);
      if ((millis() / 250) & 1) {
        d.drawFastVLine(creditX + shownLen * 12 + 4, creditY, 16, INTRO_SCAN);
      }
    }

    const uint32_t spent = millis() - frameStart;
    if (spent < FRAME_MS) delay(FRAME_MS - spent);
  }

  d.fillRoundRect(plateX, plateY, plateW, plateH, 8, INTRO_PLATE);
  d.drawRoundRect(plateX, plateY, plateW, plateH, 8, INTRO_PLATE_EDGE);
  d.drawFastHLine(plateX + 14, plateY + 22, plateW - 28, INTRO_MATRIX_DIM);
  d.drawFastHLine(plateX + 14, plateY + plateH - 22, plateW - 28, INTRO_MATRIX_DIM);
  d.setTextSize(4);
  d.setTextColor(COLOR_TEXT);
  d.setCursor(titleX, titleY);
  d.print(title);
  d.setTextSize(2);
  d.setTextColor(INTRO_MATRIX_HEAD);
  d.setCursor(creditX, creditY);
  d.print(credit);
  delay(250);
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
                bool selected = false, uint16_t color = COLOR_TEXT) {
  Adafruit_GFX& d = display.gfx();
  const uint16_t bg = selected ? COLOR_ACCENT : COLOR_PANEL;
  d.fillRoundRect(x, y, w, h, 8, bg);
  d.drawRoundRect(x, y, w, h, 8, selected ? COLOR_TEXT : COLOR_PANEL_2);
  d.setTextSize(2);
  d.setTextColor(selected ? COLOR_BG : color, bg);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t tw = 0;
  uint16_t th = 0;
  d.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
  d.setCursor(x + max(8, (w - static_cast<int>(tw)) / 2), y + (h - 14) / 2);
  d.print(label);
}

void drawWrapped(const char* text, int16_t x, int16_t y, uint8_t maxChars, uint8_t maxLines,
                 uint16_t color = COLOR_DIM) {
  Adafruit_GFX& d = display.gfx();
  d.setTextSize(2);
  d.setTextColor(color, COLOR_BG);
  const char* p = text;
  for (uint8_t line = 0; line < maxLines && *p; line++) {
    char buf[40];
    uint8_t len = 0;
    const char* start = p;
    const char* lastSpace = nullptr;
    while (*p && *p != '\n' && len < maxChars) {
      if (*p == ' ') lastSpace = p;
      p++;
      len++;
    }
    if (*p && *p != '\n' && lastSpace && lastSpace > start) {
      len = lastSpace - start;
      p = lastSpace + 1;
    } else if (*p == '\n') {
      p++;
    }
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    d.setCursor(x, y + line * 26);
    d.print(buf);
  }
}

void drawHome() {
  display.clear();
  display.header("Cypher Cube");
  drawButton(22, 70, 324, 56, "SD App Catalog", selectedHome == 0,
             sdMounted ? COLOR_TEXT : COLOR_WARN);
  drawButton(22, 140, 324, 56, "Launch Installed", selectedHome == 1,
             appPartitionValid ? COLOR_TEXT : COLOR_DIM);
  drawButton(22, 210, 324, 56, "Settings", selectedHome == 2);
  drawButton(22, 280, 324, 56, "System Info", selectedHome == 3);
  drawButton(22, 350, 324, 56, "Erase Installed", selectedHome == 4,
             appPartitionValid ? COLOR_WARN : COLOR_DIM);
  display.footer("Tap item", "BOOT: next/select");
  if (statusLine[0]) drawWrapped(statusLine, 22, 420, 26, 1, COLOR_DIM);
}

void drawApps() {
  display.clear();
  display.header("SD App Catalog");
  if (!sdMounted) {
    drawWrapped("No SD card mounted. Copy dist/sd-card to a FAT32 card.", 18, 80, 27, 5, COLOR_WARN);
    display.footer("Back", "Reload");
    return;
  }
  if (appCount == 0) {
    drawWrapped("No apps found. Run tools/build-apps.sh and package-sd.sh.", 18, 80, 27, 5, COLOR_WARN);
    display.footer("Back", "Reload");
    return;
  }

  if (selectedApp < appScroll) appScroll = selectedApp;
  if (selectedApp >= appScroll + 5) appScroll = selectedApp - 4;
  uint8_t visible = min<uint8_t>(5, appCount - appScroll);
  for (uint8_t i = 0; i < visible; i++) {
    const uint8_t idx = appScroll + i;
    AppEntry& app = apps[idx];
    drawButton(18, 58 + i * 58, 332, 48, app.name, idx == selectedApp,
               app.installable ? COLOR_TEXT : COLOR_DIM);
  }
  drawWrapped(apps[selectedApp].notes, 20, 360, 27, 2,
              apps[selectedApp].installable ? COLOR_DIM : COLOR_WARN);
  display.footer("Back  Reload", "More");
}

const char* timeoutLabel() {
  static char buf[16];
  if (screenTimeoutSec == 0) return "never";
  if (screenTimeoutSec < 60) {
    snprintf(buf, sizeof(buf), "%us", screenTimeoutSec);
  } else {
    snprintf(buf, sizeof(buf), "%um", screenTimeoutSec / 60);
  }
  return buf;
}

void enterSetTime() {
  editTime = WSensors::Rtc::now();
  if (!editTime.valid || editTime.year < 2020) {
    editTime = WSensors::DateTime{2026, 1, 1, 0, 0, 0, true};
  }
  timeField = 0;
  screenMode = SCREEN_SET_TIME;
  redraw();
}

constexpr int16_t SETTINGS_TOP = 48;
constexpr int16_t SETTINGS_ROW_H = 45;
constexpr int16_t SETTINGS_BTN_H = 42;
constexpr uint8_t SETTINGS_ROWS = 8;

int16_t settingRowY(uint8_t i) { return SETTINGS_TOP + i * SETTINGS_ROW_H; }

void drawSettings() {
  display.clear();
  display.header("Settings");
  char buf[40];
  drawButton(20, settingRowY(0), 328, SETTINGS_BTN_H,
             bootToApp ? "Boot: installed app" : "Boot: launcher");
  snprintf(buf, sizeof(buf), "Brightness: %u", brightness);
  drawButton(20, settingRowY(1), 328, SETTINGS_BTN_H, buf);
  snprintf(buf, sizeof(buf), "Sound: %s %u%%", soundMuted ? "muted" : "on", soundVolume);
  drawButton(20, settingRowY(2), 328, SETTINGS_BTN_H, buf);
  snprintf(buf, sizeof(buf), "Screen timeout: %s", timeoutLabel());
  drawButton(20, settingRowY(3), 328, SETTINGS_BTN_H, buf);
  snprintf(buf, sizeof(buf), "Shake to home: %s",
           !WSensors::Imu::available() ? "no IMU" : (imuGesturesOn ? "on" : "off"));
  drawButton(20, settingRowY(4), 328, SETTINGS_BTN_H, buf);
  drawButton(20, settingRowY(5), 328, SETTINGS_BTN_H,
             WSensors::Rtc::available() ? "Set time..." : "Set time (no RTC)");
  snprintf(buf, sizeof(buf), "Boot app: %s",
           defaultBootSlug[0] ? defaultBootSlug : "installed");
  drawButton(20, settingRowY(6), 328, SETTINGS_BTN_H, buf);
  drawButton(20, settingRowY(7), 328, SETTINGS_BTN_H, "Reload SD catalog");
  display.footer("Tap setting", "BOOT: back");
}

void drawSoundSettings() {
  display.clear();
  display.header("Sound");
  char buf[40];
  snprintf(buf, sizeof(buf), "Volume: %u%%", soundVolume);
  drawWrapped("These settings are stored on the device and are read by every app that uses the shared audio player.",
              20, 62, 29, 3, COLOR_DIM);
  drawButton(20, 156, 328, 54, buf, true, COLOR_ACCENT);
  drawButton(20, 228, 154, 56, "- 10%", false, COLOR_TEXT);
  drawButton(194, 228, 154, 56, "+ 10%", false, COLOR_TEXT);
  drawButton(20, 304, 328, 56, soundMuted ? "Mute: ON" : "Mute: OFF",
             soundMuted, soundMuted ? COLOR_WARN : COLOR_GOOD);
  drawButton(20, 368, 328, 38, "Back", false, COLOR_DIM);
  display.footer("Tap control", "BOOT: back");
}

void drawSetTime() {
  display.clear();
  display.header("Set Time");
  Adafruit_GFX& d = display.gfx();
  const char* labels[5] = {"Year", "Month", "Day", "Hour", "Min"};
  uint16_t vals[5] = {editTime.year, editTime.month, editTime.day,
                      editTime.hour, editTime.minute};
  for (uint8_t i = 0; i < 5; i++) {
    const int16_t y = 56 + i * 50;
    d.setTextSize(2);
    d.setTextColor(i == timeField ? COLOR_ACCENT : COLOR_DIM, COLOR_BG);
    d.setCursor(20, y + 12);
    d.print(labels[i]);
    drawButton(150, y, 44, 40, "-");
    char vbuf[8];
    snprintf(vbuf, sizeof(vbuf), (i == 0) ? "%04u" : "%02u", vals[i]);
    d.setTextSize(3);
    d.setTextColor(COLOR_TEXT, COLOR_BG);
    d.setCursor(212, y + 8);
    d.print(vbuf);
    drawButton(300, y, 44, 40, "+");
  }
  drawButton(22, 330, 148, 54, "Cancel");
  drawButton(198, 330, 148, 54, "Save", true);
  display.footer("Tap +/-", "BOOT: cancel");
}

void drawPickBootApp() {
  display.clear();
  display.header("Default Boot App");
  if (appCount == 0) {
    drawWrapped("No catalog apps loaded.", 18, 80, 27, 3, COLOR_WARN);
    display.footer("Back", nullptr);
    return;
  }
  if (selectedApp < appScroll) appScroll = selectedApp;
  if (selectedApp >= appScroll + 5) appScroll = selectedApp - 4;
  const uint8_t visible = min<uint8_t>(5, appCount - appScroll);
  // First row is the "none / installed" option, then the apps.
  drawButton(18, 58, 332, 44, "(boot whatever is installed)",
             defaultBootSlug[0] == '\0');
  for (uint8_t i = 0; i < visible && i < 4; i++) {
    const uint8_t idx = appScroll + i;
    AppEntry& app = apps[idx];
    drawButton(18, 110 + i * 58, 332, 48, app.name,
               strcmp(app.slug, defaultBootSlug) == 0, COLOR_TEXT);
  }
  display.footer("Tap to pick", "More");
}

void drawInfo() {
  display.clear();
  display.header("System Info");
  Adafruit_GFX& d = display.gfx();
  d.setTextSize(2);
  d.setTextColor(COLOR_TEXT, COLOR_BG);
  const esp_partition_t* part = appPartition();
  int y = 70;
  d.setCursor(20, y); d.printf("SD: %s", sdMounted ? "mounted" : "missing"); y += 30;
  d.setCursor(20, y); d.printf("Catalog: %u apps", appCount); y += 30;
  d.setCursor(20, y); d.printf("Installed: %s", appPartitionValid ? lastInstalled : "none"); y += 30;
  d.setCursor(20, y); d.printf("Boot: %s", bootToApp ? "app" : "launcher"); y += 30;
  d.setCursor(20, y); d.printf("Heap: %u", ESP.getFreeHeap()); y += 30;
  if (WSensors::Battery::available()) {
    const int pct = WSensors::Battery::percent();
    d.setCursor(20, y);
    d.printf("Batt: %d%% %s", pct, WSensors::Battery::isCharging() ? "CHG" : "");
    y += 30;
  }
  if (WSensors::Rtc::available()) {
    WSensors::DateTime t = WSensors::Rtc::now();
    d.setCursor(20, y);
    d.printf("Time: %04u-%02u-%02u %02u:%02u", t.year, t.month, t.day, t.hour,
             t.minute);
    y += 30;
  }
  if (part) {
    d.setCursor(20, y); d.printf("App1: 0x%06X", part->address); y += 30;
    d.setCursor(20, y); d.printf("Size: %u KB", part->size / 1024); y += 30;
  }
  d.setCursor(20, y); d.printf("Build: %s", __DATE__);
  display.footer("Tap or BOOT", "back");
}

void drawConfirmInstall() {
  display.clear();
  AppEntry& app = apps[selectedApp];
  const bool already = relaunchMatchesInstalled();
  display.header(already ? "Launch App?" : "Install App?");
  Adafruit_GFX& d = display.gfx();
  d.setTextSize(2);
  d.setTextColor(COLOR_ACCENT, COLOR_BG);
  d.setCursor(22, 70);
  d.print(app.name);
  drawWrapped(app.notes, 22, 108, 27, 4, COLOR_DIM);
  if (already) {
    drawWrapped("Already in the app slot. Launch instantly, or reinstall the copy.",
                22, 230, 27, 3, COLOR_DIM);
    drawButton(22, 330, 100, 60, "Cancel");
    drawButton(134, 330, 100, 60, "Reinstall", false, COLOR_WARN);
    drawButton(246, 330, 100, 60, "Launch", true);
  } else {
    drawWrapped("This copies the SD .bin into the app partition and reboots.",
                22, 230, 27, 4, COLOR_WARN);
    drawButton(22, 344, 148, 54, "Cancel");
    drawButton(198, 344, 148, 54, "Install", true);
  }
  display.footer("BOOT: cancel");
}

void drawInstallProgress(const char* appName, const char* phase, size_t written,
                         size_t total, uint8_t percent, uint16_t color) {
  if (percent > 100) percent = 100;
  display.clear();
  display.header("Installing");
  Adafruit_GFX& d = display.gfx();

  d.setTextSize(2);
  d.setTextColor(COLOR_TEXT, COLOR_BG);
  d.setCursor(22, 72);
  d.print(appName);

  d.setTextColor(color, COLOR_BG);
  d.setCursor(22, 120);
  d.print(phase);

  char pct[8];
  snprintf(pct, sizeof(pct), "%u%%", percent);
  d.setTextSize(3);
  d.setTextColor(COLOR_TEXT, COLOR_BG);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t tw = 0;
  uint16_t th = 0;
  d.getTextBounds(pct, 0, 0, &x1, &y1, &tw, &th);
  d.setCursor(max(22, static_cast<int>(display.width()) - static_cast<int>(tw) - 24), 118);
  d.print(pct);

  constexpr int16_t barX = 22;
  constexpr int16_t barY = 184;
  constexpr int16_t barW = 324;
  constexpr int16_t barH = 26;
  d.drawRoundRect(barX, barY, barW, barH, 8, COLOR_PANEL_2);
  d.fillRoundRect(barX + 3, barY + 3,
                  map(percent, 0, 100, 0, barW - 6), barH - 6, 6, color);

  d.setTextSize(2);
  d.setTextColor(COLOR_DIM, COLOR_BG);
  d.setCursor(22, 246);
  if (total > 0) {
    d.printf("%u KB / %u KB",
             static_cast<unsigned>((written + 1023) / 1024),
             static_cast<unsigned>((total + 1023) / 1024));
  } else {
    d.print("Preparing flash");
  }

  drawWrapped("Do not power off or remove the SD card.", 22, 304, 27, 2, COLOR_WARN);
  display.footer("Installing", "Please wait");
}

void redraw() {
  switch (screenMode) {
    case SCREEN_HOME: drawHome(); break;
    case SCREEN_APPS: drawApps(); break;
    case SCREEN_SETTINGS: drawSettings(); break;
    case SCREEN_SOUND: drawSoundSettings(); break;
    case SCREEN_SET_TIME: drawSetTime(); break;
    case SCREEN_PICK_BOOT_APP: drawPickBootApp(); break;
    case SCREEN_INFO: drawInfo(); break;
    case SCREEN_CONFIRM_INSTALL: drawConfirmInstall(); break;
    case SCREEN_MESSAGE: display.message(messageTitle, messageBody); break;
  }
  // Battery glyph + clock overlay in the header band, on every screen.
  WSensors::drawStatusChrome(display.gfx(), static_cast<int16_t>(display.width()));
}

TapEvent readTap() {
  TapEvent event;
  TouchPoint points[1];
  uint8_t pointCount = 0;
  const bool down = touch.read(points, 1, pointCount);
  if (down && !touchWasDown && pointCount > 0) {
    event.tapped = true;
    event.x = points[0].x;
    event.y = points[0].y;
  }
  touchWasDown = down;
  return event;
}

void launchInstalled() {
  if (!appPartitionValid) {
    showMessage("No App", "No valid app is installed in app1 yet.");
    redraw();
    return;
  }
  if (setBootPartition(appPartition())) {
    delay(150);
    ESP.restart();
  }
}

bool eraseInstalled() {
  const esp_partition_t* part = appPartition();
  if (!part) return false;
  esp_err_t err = esp_partition_erase_range(part, 0, part->size);
  if (err != ESP_OK) {
    Serial.printf("[erase] failed: %s\n", esp_err_to_name(err));
    return false;
  }
  appPartitionValid = false;
  lastInstalled[0] = '\0';
  lastSlug[0] = '\0';
  lastSize = 0;
  savePrefs();
  setStatus("Installed app erased");
  return true;
}

size_t roundUpToSector(size_t value) {
  return ((value + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
}

bool eraseForImage(const AppEntry& app, const esp_partition_t* part, size_t fileSize,
                   size_t eraseSize) {
  size_t erased = 0;
  uint8_t lastDrawPercent = 255;
  uint32_t lastProgressDrawAt = 0;
  while (erased < eraseSize) {
    const size_t todo = min<size_t>(FLASH_ERASE_CHUNK, eraseSize - erased);
    const uint8_t percent = static_cast<uint8_t>(min<size_t>(100, (erased * 100) / eraseSize));
    const uint32_t now = millis();
    if (lastDrawPercent == 255 || percent >= lastDrawPercent + 5 ||
        now - lastProgressDrawAt >= 300) {
      drawInstallProgress(app.name, "Erasing", erased, fileSize, percent, COLOR_WARN);
      lastDrawPercent = percent;
      lastProgressDrawAt = now;
    }
    Serial.printf("[install] erase %u/%u\n",
                  static_cast<unsigned>(erased / 1024),
                  static_cast<unsigned>(eraseSize / 1024));
    esp_err_t err = esp_partition_erase_range(part, erased, todo);
    if (err != ESP_OK) {
      Serial.printf("[install] erase failed at 0x%X: %s\n",
                    static_cast<unsigned>(erased), esp_err_to_name(err));
      return false;
    }
    erased += todo;
    yield();
    delay(1);
  }
  drawInstallProgress(app.name, "Erasing", fileSize, fileSize, 100, COLOR_WARN);
  return true;
}

bool relaunchMatchesInstalled() {
  if (selectedApp >= appCount || !appPartitionValid || !lastSlug[0]) return false;
  AppEntry& app = apps[selectedApp];
  if (strcmp(app.slug, lastSlug) != 0) return false;
  char path[128];
  snprintf(path, sizeof(path), "%s/%s", APP_DIR, app.binary);
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return false;
  const uint32_t sz = f.size();
  f.close();
  return sz == lastSize;
}

bool installSelectedApp(bool force) {
  if (selectedApp >= appCount) return false;
  AppEntry& app = apps[selectedApp];
  if (!app.installable) {
    showMessage("Not Ready", "This app is listed but not marked installable.");
    return false;
  }
  if (strstr(app.binary, "merged") || strstr(app.binary, "bootloader") ||
      strstr(app.binary, "partitions")) {
    showMessage("Rejected", "Use a sketch app .bin, not a merged flash image.");
    return false;
  }

  char path[128];
  snprintf(path, sizeof(path), "%s/%s", APP_DIR, app.binary);
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    showMessage("Missing Bin", "The app binary listed in apps.json was not found on SD.");
    return false;
  }

  const esp_partition_t* part = appPartition();
  if (!part) {
    file.close();
    showMessage("No App Slot", "The app1 partition is missing.");
    return false;
  }
  const size_t fileSize = file.size();
  if (fileSize == 0 || fileSize > part->size) {
    file.close();
    showMessage("Too Large", "The app binary is empty or bigger than app1.");
    return false;
  }

  uint8_t magic = 0;
  file.read(&magic, 1);
  file.seek(0);
  if (magic != ESP_APP_IMAGE_MAGIC) {
    file.close();
    showMessage("Bad Image", "The file does not look like an ESP32 app image.");
    return false;
  }

  // Fast path: the same app (slug + byte size) is already in app1. Skip the
  // erase/write entirely and just point boot at it. Saves flash wear and time.
  if (!force && appPartitionValid && lastSlug[0] &&
      strcmp(app.slug, lastSlug) == 0 && fileSize == lastSize) {
    file.close();
    Serial.printf("[install] %s already in app1; skipping copy\n", app.slug);
    if (setBootPartition(part)) {
      drawInstallProgress(app.name, "Launching", fileSize, fileSize, 100, COLOR_GOOD);
      delay(400);
      ESP.restart();
    }
    return true;
  }

  drawInstallProgress(app.name, "Preparing", 0, fileSize, 0, COLOR_ACCENT);
  const size_t eraseSize = roundUpToSector(fileSize);
  Serial.printf("[install] %s -> app1, %u bytes, erase %u/%u bytes\n",
                app.binary, static_cast<unsigned>(fileSize),
                static_cast<unsigned>(eraseSize), static_cast<unsigned>(part->size));
  if (!eraseForImage(app, part, fileSize, eraseSize)) {
    file.close();
    showMessage("Erase Failed", "Flash erase failed. Check serial logs.");
    return false;
  }

  uint8_t* buffer = static_cast<uint8_t*>(malloc(FLASH_CHUNK));
  if (!buffer) {
    file.close();
    showMessage("No Memory", "Could not allocate flash buffer.");
    return false;
  }

  size_t offset = 0;
  uint8_t lastDrawPercent = 255;
  uint32_t lastProgressDrawAt = 0;
  drawInstallProgress(app.name, "Writing", 0, fileSize, 0, COLOR_ACCENT);
  while (file.available()) {
    const size_t n = file.read(buffer, FLASH_CHUNK);
    if (n == 0) break;
    esp_err_t err = esp_partition_write(part, offset, buffer, n);
    if (err != ESP_OK) {
      free(buffer);
      file.close();
      showMessage("Write Failed", esp_err_to_name(err));
      return false;
    }
    offset += n;
    const uint8_t percent = static_cast<uint8_t>(min<size_t>(100, (offset * 100) / fileSize));
    const uint32_t now = millis();
    if (lastDrawPercent == 255 || percent >= lastDrawPercent + 2 ||
        now - lastProgressDrawAt >= 150 || offset >= fileSize) {
      drawInstallProgress(app.name, "Writing", offset, fileSize, percent, COLOR_ACCENT);
      lastDrawPercent = percent;
      lastProgressDrawAt = now;
      delay(1);
    }
  }
  free(buffer);
  file.close();

  copyField(lastInstalled, sizeof(lastInstalled), app.name);
  copyField(lastSlug, sizeof(lastSlug), app.slug);
  lastSize = static_cast<uint32_t>(fileSize);
  appPartitionValid = true;
  savePrefs();
  if (setBootPartition(part)) {
    drawInstallProgress(app.name, "Launching", fileSize, fileSize, 100, COLOR_GOOD);
    delay(600);
    ESP.restart();
  }
  return true;
}

void activateHomeSelection() {
  if (selectedHome == 0) {
    screenMode = SCREEN_APPS;
  } else if (selectedHome == 1) {
    launchInstalled();
    return;
  } else if (selectedHome == 2) {
    screenMode = SCREEN_SETTINGS;
  } else if (selectedHome == 3) {
    screenMode = SCREEN_INFO;
  } else if (selectedHome == 4) {
    if (eraseInstalled()) showMessage("Erased", "The app partition was erased. The launcher remains installed.");
  }
  redraw();
}

void handleHomeTap(uint16_t, uint16_t y) {
  if (y >= 70 && y < 126) {
    selectedHome = 0;
  } else if (y >= 140 && y < 196) {
    selectedHome = 1;
  } else if (y >= 210 && y < 266) {
    selectedHome = 2;
  } else if (y >= 280 && y < 336) {
    selectedHome = 3;
  } else if (y >= 350 && y < 406) {
    selectedHome = 4;
  } else {
    return;
  }
  activateHomeSelection();
}

void handleAppsTap(uint16_t x, uint16_t y) {
  if (y >= display.height() - 38) {
    if (x < 120) {
      screenMode = SCREEN_HOME;
    } else if (x < 260) {
      mountSd();
      loadManifest();
    } else if (appCount > 0) {
      selectedApp = min<uint8_t>(appCount - 1, selectedApp + 5);
    }
    redraw();
    return;
  }
  if (appCount == 0) return;
  if (y >= 58 && y < 58 + 5 * 58) {
    uint8_t row = (y - 58) / 58;
    uint8_t idx = appScroll + row;
    if (idx < appCount) {
      selectedApp = idx;
      screenMode = SCREEN_CONFIRM_INSTALL;
      redraw();
    }
  }
}

void cycleScreenTimeout() {
  if (screenTimeoutSec == 30) {
    screenTimeoutSec = 60;
  } else if (screenTimeoutSec == 60) {
    screenTimeoutSec = 120;
  } else if (screenTimeoutSec == 120) {
    screenTimeoutSec = 300;
  } else if (screenTimeoutSec == 300) {
    screenTimeoutSec = 0;
  } else {
    screenTimeoutSec = 30;
  }
}

void handleSettingsTap(uint16_t, uint16_t y) {
  int row = -1;
  for (uint8_t i = 0; i < SETTINGS_ROWS; i++) {
    if (y >= settingRowY(i) && y < settingRowY(i) + SETTINGS_BTN_H) {
      row = i;
      break;
    }
  }
  switch (row) {
    case 0:
      bootToApp = !bootToApp;
      savePrefs();
      break;
    case 1:
      brightness = brightness >= 240 ? 80 : brightness + 40;
      display.setBrightness(brightness);
      savePrefs();
      break;
    case 2:
      screenMode = SCREEN_SOUND;
      redraw();
      return;
    case 3:
      cycleScreenTimeout();
      savePrefs();
      break;
    case 4:
      if (WSensors::Imu::available()) {
        imuGesturesOn = !imuGesturesOn;
        savePrefs();
      }
      break;
    case 5:
      if (WSensors::Rtc::available()) {
        enterSetTime();
        return;
      }
      break;
    case 6:
      screenMode = SCREEN_PICK_BOOT_APP;
      redraw();
      return;
    case 7:
      mountSd();
      loadManifest();
      break;
    default:
      return;
  }
  redraw();
}

void handleSoundTap(uint16_t x, uint16_t y) {
  if (y >= 228 && y < 284) {
    if (x < 184) {
      soundVolume = soundVolume >= 10 ? soundVolume - 10 : 0;
    } else {
      soundVolume = soundVolume <= 90 ? soundVolume + 10 : 100;
    }
    savePrefs();
  } else if (y >= 304 && y < 360) {
    soundMuted = !soundMuted;
    savePrefs();
  } else if (y >= 368 && y < 406) {
    screenMode = SCREEN_SETTINGS;
  }
  redraw();
}

void adjustTimeField(int delta) {
  switch (timeField) {
    case 0:
      editTime.year = constrain(editTime.year + delta, 2020, 2099);
      break;
    case 1:
      editTime.month = constrain(editTime.month + delta, 1, 12);
      break;
    case 2:
      editTime.day = constrain(editTime.day + delta, 1, 31);
      break;
    case 3:
      editTime.hour = (editTime.hour + 24 + delta) % 24;
      break;
    case 4:
      editTime.minute = (editTime.minute + 60 + delta) % 60;
      break;
    default:
      break;
  }
}

void handleSetTimeTap(uint16_t x, uint16_t y) {
  // Field rows.
  for (uint8_t i = 0; i < 5; i++) {
    const int16_t rowY = 56 + i * 50;
    if (y >= rowY && y < rowY + 40) {
      timeField = i;
      if (x >= 150 && x < 194) {
        adjustTimeField(-1);
      } else if (x >= 300 && x < 344) {
        adjustTimeField(1);
      }
      redraw();
      return;
    }
  }
  // Cancel / Save.
  if (y >= 330 && y < 384) {
    if (x < 184) {
      screenMode = SCREEN_SETTINGS;
    } else {
      editTime.second = 0;
      WSensors::Rtc::set(editTime);
      setStatus("Clock updated");
      screenMode = SCREEN_SETTINGS;
    }
    redraw();
  }
}

void handlePickBootAppTap(uint16_t x, uint16_t y) {
  if (y >= display.height() - 38) {
    if (x >= 240 && appCount > 0) {
      selectedApp = min<uint8_t>(appCount - 1, selectedApp + 4);
    } else {
      screenMode = SCREEN_SETTINGS;
    }
    redraw();
    return;
  }
  if (y >= 58 && y < 102) {
    defaultBootSlug[0] = '\0';
    bootToApp = false;
    savePrefs();
    screenMode = SCREEN_SETTINGS;
    redraw();
    return;
  }
  if (appCount == 0) return;
  if (y >= 110 && y < 110 + 4 * 58) {
    const uint8_t row = (y - 110) / 58;
    const uint8_t idx = appScroll + row;
    if (idx < appCount) {
      copyField(defaultBootSlug, sizeof(defaultBootSlug), apps[idx].slug);
      bootToApp = true;
      savePrefs();
      screenMode = SCREEN_SETTINGS;
      redraw();
    }
  }
}

void handleConfirmTap(uint16_t x, uint16_t y) {
  if (relaunchMatchesInstalled()) {
    if (y >= 330 && y < 390) {
      if (x < 122) {
        screenMode = SCREEN_APPS;
        redraw();
      } else if (x < 234) {
        installSelectedApp(true);  // reinstall (full copy)
      } else {
        installSelectedApp(false);  // launch (skip copy)
      }
    }
    return;
  }
  if (y >= 344 && y < 398) {
    if (x < 184) {
      screenMode = SCREEN_APPS;
      redraw();
    } else {
      installSelectedApp(false);
    }
  }
}

void goBack() {
  if (screenMode == SCREEN_HOME) return;
  if (screenMode == SCREEN_CONFIRM_INSTALL) {
    screenMode = SCREEN_APPS;
  } else if (screenMode == SCREEN_SOUND || screenMode == SCREEN_SET_TIME ||
             screenMode == SCREEN_PICK_BOOT_APP) {
    screenMode = SCREEN_SETTINGS;
  } else {
    screenMode = SCREEN_HOME;
  }
  redraw();
}

void noteActivity() { lastActivityAt = millis(); }

void wakeScreen() {
  screenAsleep = false;
  display.setBrightness(brightness);
  noteActivity();
  redraw();
}

void maybeSleep() {
  if (screenAsleep || screenTimeoutSec == 0) return;
  if (millis() - lastActivityAt >= static_cast<uint32_t>(screenTimeoutSec) * 1000UL) {
    display.setBrightness(0);
    screenAsleep = true;
  }
}

// Shake-to-home gesture, polled while the screen is awake.
// (Flip-to-sleep was removed: static face-down detection depends on the board's
//  IMU axis orientation and misfired during normal handling, turning the screen
//  off instantly. The screen timeout already covers idle sleep. Shake-to-home
//  uses acceleration magnitude, which is sign-independent and safe.)
void checkImuGestures() {
  if (!imuGesturesOn || !WSensors::Imu::available()) return;
  static uint32_t lastCheck = 0;
  const uint32_t now = millis();
  if (now - lastCheck < 150) return;
  lastCheck = now;

  static uint32_t lastShake = 0;
  if (WSensors::Imu::isShaken(2.4f) && now - lastShake > 1500) {
    lastShake = now;
    noteActivity();
    if (screenMode != SCREEN_HOME) goHome();
  }
}

void goHome() {
  screenMode = SCREEN_HOME;
  redraw();
}

void handleBootButton(bool down) {
  const uint32_t now = millis();
  if (down && !bootWasDown) {
    bootDownAt = now;
    noteActivity();
  } else if (!down && bootWasDown) {
    if (swallowBootRelease) {
      swallowBootRelease = false;
      bootWasDown = down;
      return;
    }
    noteActivity();
    const uint32_t held = now - bootDownAt;
    if (screenMode == SCREEN_HOME) {
      if (held > 900) {
        activateHomeSelection();
      } else {
        selectedHome = (selectedHome + 1) % 5;
        redraw();
      }
    } else if (held > 900) {
      goHome();
    } else {
      goBack();
    }
  }
  bootWasDown = down;
}

void printStatus() {
  const esp_partition_t* part = appPartition();
  Serial.println("status:");
  Serial.printf("  sd=%s apps=%u\n", sdMounted ? "mounted" : "missing", appCount);
  Serial.printf("  installed=%s valid=%s\n", lastInstalled[0] ? lastInstalled : "none",
                appPartitionValid ? "yes" : "no");
  Serial.printf("  boot=%s heap=%u\n", bootToApp ? "app" : "launcher", ESP.getFreeHeap());
  Serial.printf("  sound=%u%% mute=%s\n", soundVolume, soundMuted ? "on" : "off");
  if (part) Serial.printf("  app1=0x%06X size=%uKB\n", part->address, part->size / 1024);
}

void printApps() {
  if (appCount == 0) {
    Serial.println("no apps loaded");
    return;
  }
  for (uint8_t i = 0; i < appCount; i++) {
    Serial.printf("%u. %s [%s] %s\n", i + 1, apps[i].slug, apps[i].version,
                  apps[i].installable ? "ready" : "not-installable");
  }
}

int findAppBySlug(const char* slug) {
  for (uint8_t i = 0; i < appCount; i++) {
    if (strcmp(apps[i].slug, slug) == 0) return i;
  }
  return -1;
}

void runSerialCommand(char* cmd) {
  while (*cmd == ' ') cmd++;
  if (!*cmd) return;
  Serial.printf("> %s\n", cmd);
  if (SerialShell::handleLine(cmd)) {
    return;
  }
  if (strcmp(cmd, "help") == 0) {
    Serial.println("help, status, apps, reload, launch, erase, install <slug>, boot launcher, boot app, bright <0-255>, sound [volume <0-100>|mute on|off], screen on|off");
    SerialShell::printHelp();
  } else if (strcmp(cmd, "status") == 0) {
    printStatus();
  } else if (strcmp(cmd, "apps") == 0) {
    printApps();
  } else if (strcmp(cmd, "reload") == 0) {
    mountSd();
    loadManifest();
    redraw();
  } else if (strcmp(cmd, "launch") == 0 || strcmp(cmd, "boot app") == 0) {
    launchInstalled();
  } else if (strcmp(cmd, "boot launcher") == 0) {
    if (setBootPartition(launcherPartition())) ESP.restart();
  } else if (strcmp(cmd, "erase") == 0) {
    eraseInstalled();
    redraw();
  } else if (strncmp(cmd, "install ", 8) == 0) {
    int idx = findAppBySlug(cmd + 8);
    if (idx < 0) {
      Serial.println("app slug not found");
    } else {
      selectedApp = static_cast<uint8_t>(idx);
      installSelectedApp(false);
    }
  } else if (strncmp(cmd, "bright", 6) == 0 && (cmd[6] == ' ' || cmd[6] == '\0')) {
    const char* arg = cmd + 6;
    while (*arg == ' ') arg++;
    if (!*arg) {
      Serial.printf("brightness=%u\n", brightness);
    } else {
      int v = atoi(arg);
      if (v < 0) v = 0;
      if (v > 255) v = 255;
      brightness = static_cast<uint8_t>(v);
      screenAsleep = false;
      display.setBrightness(brightness);
      noteActivity();
      savePrefs();
      Serial.printf("brightness=%u\n", brightness);
    }
  } else if (strncmp(cmd, "sound", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
    const char* arg = cmd + 5;
    while (*arg == ' ') arg++;
    if (!*arg) {
      Serial.printf("sound volume=%u mute=%s\n", soundVolume, soundMuted ? "on" : "off");
    } else if (strncmp(arg, "volume ", 7) == 0) {
      int value = atoi(arg + 7);
      if (value < 0 || value > 100) {
        Serial.println("sound volume must be 0 through 100");
      } else {
        soundVolume = static_cast<uint8_t>(value);
        savePrefs();
        Serial.printf("sound volume=%u\n", soundVolume);
      }
    } else if (strcmp(arg, "mute on") == 0) {
      soundMuted = true;
      savePrefs();
      Serial.println("sound mute=on");
    } else if (strcmp(arg, "mute off") == 0) {
      soundMuted = false;
      savePrefs();
      Serial.println("sound mute=off");
    } else {
      Serial.println("sound [volume <0-100>|mute on|off]");
    }
  } else if (strcmp(cmd, "screen on") == 0) {
    wakeScreen();
    Serial.println("screen on");
  } else if (strcmp(cmd, "screen off") == 0) {
    display.setBrightness(0);
    screenAsleep = true;
    Serial.println("screen off");
  } else {
    Serial.println("unknown command; type help");
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      if (serialLineLen > 0) {
        serialLine[serialLineLen] = '\0';
        runSerialCommand(serialLine);
        serialLineLen = 0;
      }
      continue;
    }
    if (serialLineLen < sizeof(serialLine) - 1) {
      serialLine[serialLineLen++] = c;
    }
  }
}
}

void launcherSetup() {
  Serial.begin(SERIAL_BAUD);
  delay(250);
  Serial.println();
  Serial.println("[boot] Cypher Cube");

  const bool forceMenu = forceLauncherAtBoot();
  loadPrefs();
  appPartitionValid = readAppMagic();

  display.begin();
  display.setBrightness(brightness);
  touch.begin();
  WSensors::begin();  // battery / RTC / IMU on the shared Wire bus
  drawStartupIntro();
  mountSd();
  loadManifest();

  if (forceMenu) {
    bootToApp = false;
    savePrefs();
    setStatus("BOOT held: staying in launcher");
  } else if (bootToApp) {
    const int idx = defaultBootSlug[0] ? findAppBySlug(defaultBootSlug) : -1;
    if (idx >= 0) {
      Serial.printf("[boot] auto-booting default app %s\n", defaultBootSlug);
      selectedApp = static_cast<uint8_t>(idx);
      installSelectedApp(false);  // skip-recopy fast-boots if already installed
    } else if (appPartitionValid) {
      Serial.println("[boot] auto-launching installed app");
      launchInstalled();
    }
  }

  setBootPartition(launcherPartition());
  redraw();
  noteActivity();
  Serial.println("[serial] type help");
}

void launcherLoop() {
  handleSerial();
  const bool bootNow = bootButtonDown();
  TapEvent tap = readTap();

  // While asleep, the first touch or BOOT press only wakes the screen.
  if (screenAsleep) {
    const bool bootEdge = bootNow && !bootWasDown;
    if (tap.tapped || bootEdge) {
      if (bootEdge) swallowBootRelease = true;
      wakeScreen();
    }
    bootWasDown = bootNow;
    delay(12);
    return;
  }

  handleBootButton(bootNow);

  if (!tap.tapped) {
    checkImuGestures();
    maybeSleep();
    delay(12);
    return;
  }

  noteActivity();
  switch (screenMode) {
    case SCREEN_HOME: handleHomeTap(tap.x, tap.y); break;
    case SCREEN_APPS: handleAppsTap(tap.x, tap.y); break;
    case SCREEN_SETTINGS: handleSettingsTap(tap.x, tap.y); break;
    case SCREEN_SOUND: handleSoundTap(tap.x, tap.y); break;
    case SCREEN_SET_TIME: handleSetTimeTap(tap.x, tap.y); break;
    case SCREEN_PICK_BOOT_APP: handlePickBootAppTap(tap.x, tap.y); break;
    case SCREEN_INFO:
    case SCREEN_MESSAGE:
      goBack();
      break;
    case SCREEN_CONFIRM_INSTALL:
      handleConfirmTap(tap.x, tap.y);
      break;
  }
}
