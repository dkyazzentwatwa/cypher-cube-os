#include "Launcher.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

#include "BoardConfig.h"
#include "DisplayPort.h"
#include "TouchInput.h"

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
uint8_t selectedApp = 0;
uint8_t appScroll = 0;
ScreenMode screenMode = SCREEN_HOME;
bool sdMounted = false;
bool appPartitionValid = false;
bool bootToApp = false;
bool touchWasDown = false;
uint8_t brightness = AMOLED_BRIGHTNESS;
char lastInstalled[40] = "";
char statusLine[96] = "";
char messageTitle[40] = "";
char messageBody[180] = "";
char serialLine[96] = "";
uint8_t serialLineLen = 0;
bool bootWasDown = false;
uint32_t bootDownAt = 0;

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
  bootToApp = prefs.getBool("bootToApp", false);
  if (prefs.getBool("returnOnce", false)) {
    prefs.putBool("returnOnce", false);
    bootToApp = false;
    prefs.putBool("bootToApp", false);
  }
  String installed = prefs.getString("lastApp", "");
  copyField(lastInstalled, sizeof(lastInstalled), installed.c_str());
}

void savePrefs() {
  prefs.putUChar("bright", brightness);
  prefs.putBool("bootToApp", bootToApp);
  prefs.putString("lastApp", lastInstalled);
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
  char countBuf[18];
  snprintf(countBuf, sizeof(countBuf), "%u apps", appCount);
  display.header("AMOLED OS", countBuf);
  drawButton(22, 70, 324, 56, "SD App Catalog", false, sdMounted ? COLOR_TEXT : COLOR_WARN);
  drawButton(22, 140, 324, 56, "Launch Installed", false,
             appPartitionValid ? COLOR_TEXT : COLOR_DIM);
  drawButton(22, 210, 324, 56, "Settings");
  drawButton(22, 280, 324, 56, "System Info");
  drawButton(22, 350, 324, 56, "Erase Installed", false,
             appPartitionValid ? COLOR_WARN : COLOR_DIM);
  display.footer("Tap item", "BOOT: back");
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

void drawSettings() {
  display.clear();
  display.header("Settings");
  drawButton(22, 78, 324, 60, bootToApp ? "Boot: installed app" : "Boot: launcher");
  char bright[32];
  snprintf(bright, sizeof(bright), "Brightness: %u", brightness);
  drawButton(22, 158, 324, 60, bright);
  drawButton(22, 238, 324, 60, "Reload SD catalog");
  drawButton(22, 318, 324, 60, "Back");
  display.footer("Tap setting", "BOOT: back");
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
  display.header("Install App?");
  Adafruit_GFX& d = display.gfx();
  d.setTextSize(2);
  d.setTextColor(COLOR_ACCENT, COLOR_BG);
  d.setCursor(22, 70);
  d.print(app.name);
  drawWrapped(app.notes, 22, 108, 27, 4, COLOR_DIM);
  drawWrapped("This copies the SD .bin into the app partition and reboots.", 22, 230, 27, 4, COLOR_WARN);
  drawButton(22, 344, 148, 54, "Cancel");
  drawButton(198, 344, 148, 54, "Install", true);
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
    case SCREEN_INFO: drawInfo(); break;
    case SCREEN_CONFIRM_INSTALL: drawConfirmInstall(); break;
    case SCREEN_MESSAGE: display.message(messageTitle, messageBody); break;
  }
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

bool installSelectedApp() {
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
  appPartitionValid = true;
  savePrefs();
  if (setBootPartition(part)) {
    drawInstallProgress(app.name, "Launching", fileSize, fileSize, 100, COLOR_GOOD);
    delay(600);
    ESP.restart();
  }
  return true;
}

void handleHomeTap(uint16_t, uint16_t y) {
  if (y >= 70 && y < 126) {
    screenMode = SCREEN_APPS;
  } else if (y >= 140 && y < 196) {
    launchInstalled();
    return;
  } else if (y >= 210 && y < 266) {
    screenMode = SCREEN_SETTINGS;
  } else if (y >= 280 && y < 336) {
    screenMode = SCREEN_INFO;
  } else if (y >= 350 && y < 406) {
    if (eraseInstalled()) showMessage("Erased", "The app partition was erased. The launcher remains installed.");
  }
  redraw();
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

void handleSettingsTap(uint16_t, uint16_t y) {
  if (y >= 78 && y < 138) {
    bootToApp = !bootToApp;
    savePrefs();
  } else if (y >= 158 && y < 218) {
    brightness = brightness >= 240 ? 80 : brightness + 40;
    display.setBrightness(brightness);
    savePrefs();
  } else if (y >= 238 && y < 298) {
    mountSd();
    loadManifest();
  } else if (y >= 318 && y < 378) {
    screenMode = SCREEN_HOME;
  }
  redraw();
}

void handleConfirmTap(uint16_t x, uint16_t y) {
  if (y >= 344 && y < 398) {
    if (x < 184) {
      screenMode = SCREEN_APPS;
      redraw();
    } else {
      installSelectedApp();
    }
  }
}

void goBack() {
  if (screenMode == SCREEN_HOME) return;
  screenMode = (screenMode == SCREEN_CONFIRM_INSTALL) ? SCREEN_APPS : SCREEN_HOME;
  redraw();
}

void goHome() {
  screenMode = SCREEN_HOME;
  redraw();
}

void handleBootButton() {
  const bool down = bootButtonDown();
  const uint32_t now = millis();
  if (down && !bootWasDown) {
    bootDownAt = now;
  } else if (!down && bootWasDown) {
    const uint32_t held = now - bootDownAt;
    if (held > 900) {
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
  if (strcmp(cmd, "help") == 0) {
    Serial.println("help, status, apps, reload, launch, erase, install <slug>, boot launcher, boot app");
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
      installSelectedApp();
    }
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
  Serial.println("[boot] Waveshare AMOLED OS");

  const bool forceMenu = forceLauncherAtBoot();
  loadPrefs();
  appPartitionValid = readAppMagic();

  display.begin();
  display.setBrightness(brightness);
  touch.begin();
  mountSd();
  loadManifest();

  if (forceMenu) {
    bootToApp = false;
    savePrefs();
    setStatus("BOOT held: staying in launcher");
  } else if (bootToApp && appPartitionValid) {
    Serial.println("[boot] auto-launching installed app");
    launchInstalled();
  }

  setBootPartition(launcherPartition());
  redraw();
  Serial.println("[serial] type help");
}

void launcherLoop() {
  handleSerial();
  handleBootButton();
  TapEvent tap = readTap();
  if (!tap.tapped) {
    delay(12);
    return;
  }

  switch (screenMode) {
    case SCREEN_HOME: handleHomeTap(tap.x, tap.y); break;
    case SCREEN_APPS: handleAppsTap(tap.x, tap.y); break;
    case SCREEN_SETTINGS: handleSettingsTap(tap.x, tap.y); break;
    case SCREEN_INFO:
    case SCREEN_MESSAGE:
      goBack();
      break;
    case SCREEN_CONFIRM_INSTALL:
      handleConfirmTap(tap.x, tap.y);
      break;
  }
}
