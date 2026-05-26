#include "StoryGameLite.h"

#include <SD_MMC.h>
#include <WaveshareAmoledAppKit.h>

namespace WaveshareAmoledPorts {
namespace {

enum StoryScreen : uint8_t {
  ScreenPlay = 0,
  ScreenSheet,
  ScreenQuest,
  ScreenLore,
  ScreenPack,
  ScreenCount
};

const char* kScreenNames[ScreenCount] = {"Play", "Sheet", "Quest", "Lore", "Pack"};

const StoryGameProfile* current = nullptr;
StoryScreen screen = ScreenPlay;
uint16_t dayCount = 1;
uint8_t chapter = 0;
uint8_t level = 1;
uint16_t xp = 0;
int16_t health = 76;
int16_t focus = 58;
int16_t kit = 52;
int16_t standing = 22;
int16_t wealth = 12;
uint8_t inventoryMask = 0;
uint8_t selected = 0;
char history0[128] = "Campaign ready. Choose an action.";
char history1[128] = "Your journal is waiting for the first mark.";
char history2[128] = "";
uint32_t lastDraw = 0;

int16_t clampValue(int16_t value, int16_t low, int16_t high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

void clampStats() {
  health = clampValue(health, 0, 99);
  focus = clampValue(focus, 0, 99);
  kit = clampValue(kit, 0, 99);
  standing = clampValue(standing, 0, 99);
  wealth = clampValue(wealth, 0, 999);
  if (level < 1) level = 1;
  if (level > 9) level = 9;
  if (chapter > 3) chapter = 3;
}

void journalPath(char* out, size_t outSize) {
  snprintf(out, outSize, "%s/saves/%s.txt", WaveshareAmoled::kGameOsPath,
           current ? current->slug : "game");
}

void statePath(char* out, size_t outSize) {
  snprintf(out, outSize, "%s/states/%s.txt", WaveshareAmoled::kGameOsPath,
           current ? current->slug : "game");
}

bool ensureGameDirs() {
  if (!WaveshareAmoled::sdReady() && !WaveshareAmoled::mountSd()) return false;
  WaveshareAmoled::ensureDir(WaveshareAmoled::kGameOsPath);
  WaveshareAmoled::ensureDir("/waveshare-os/cardputer-game-os/saves");
  WaveshareAmoled::ensureDir("/waveshare-os/cardputer-game-os/states");
  return true;
}

int readField(const char* line, const char* key, int fallback) {
  const char* p = strstr(line, key);
  if (!p) return fallback;
  return atoi(p + strlen(key));
}

void pushHistory(const char* line) {
  strncpy(history2, history1, sizeof(history2) - 1);
  history2[sizeof(history2) - 1] = '\0';
  strncpy(history1, history0, sizeof(history1) - 1);
  history1[sizeof(history1) - 1] = '\0';
  strncpy(history0, line ? line : "", sizeof(history0) - 1);
  history0[sizeof(history0) - 1] = '\0';
}

bool saveState() {
  if (!current || !ensureGameDirs()) return false;
  char path[96];
  statePath(path, sizeof(path));
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) return false;
  file.printf("v=2 day=%u chapter=%u level=%u xp=%u hp=%d focus=%d kit=%d "
              "stand=%d wealth=%d inv=%u selected=%u screen=%u\n",
              dayCount, chapter, level, xp, health, focus, kit, standing,
              wealth, inventoryMask, selected, screen);
  file.close();
  return true;
}

bool loadState() {
  if (!current || !ensureGameDirs()) return false;
  char path[96];
  statePath(path, sizeof(path));
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) return false;
  char line[220];
  const size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
  file.close();
  line[len] = '\0';
  if (readField(line, "v=", 0) < 2) return false;
  dayCount = static_cast<uint16_t>(readField(line, "day=", dayCount));
  chapter = static_cast<uint8_t>(readField(line, "chapter=", chapter));
  level = static_cast<uint8_t>(readField(line, "level=", level));
  xp = static_cast<uint16_t>(readField(line, "xp=", xp));
  health = readField(line, "hp=", health);
  focus = readField(line, "focus=", focus);
  kit = readField(line, "kit=", kit);
  standing = readField(line, "stand=", standing);
  wealth = readField(line, "wealth=", wealth);
  inventoryMask = static_cast<uint8_t>(readField(line, "inv=", inventoryMask));
  selected = static_cast<uint8_t>(readField(line, "selected=", selected) % 4);
  screen = static_cast<StoryScreen>(readField(line, "screen=", 0) % ScreenCount);
  clampStats();
  snprintf(history0, sizeof(history0), "Loaded: day %u, act %u, level %u.",
           dayCount, static_cast<unsigned>(chapter + 1), level);
  return true;
}

void writeJournal(const StoryGameAction& action) {
  if (!current || !ensureGameDirs()) return;
  char path[96];
  journalPath(path, sizeof(path));
  char line[260];
  snprintf(line, sizeof(line),
           "day=%u act=%u level=%u xp=%u hp=%d focus=%d kit=%d stand=%d "
           "wealth=%d inv=%u action=%s result=%s",
           dayCount, static_cast<unsigned>(chapter + 1), level, xp, health,
           focus, kit, standing, wealth, inventoryMask, action.label,
           action.result);
  WaveshareAmoled::appendLine(path, line);
}

void setDefaultCampaignState() {
  dayCount = 1;
  chapter = 0;
  level = 1;
  xp = 0;
  health = 76;
  focus = 58;
  kit = 52;
  standing = 22;
  wealth = 12;
  inventoryMask = 0;
  selected = 0;
  screen = ScreenPlay;
  snprintf(history0, sizeof(history0), "New campaign started.");
  snprintf(history1, sizeof(history1), "%s", current ? current->premise : "");
  history2[0] = '\0';
}

void resetCampaign() {
  setDefaultCampaignState();
  saveState();
}

void drawTab(uint8_t index, bool active) {
  Adafruit_GFX& d = WaveshareAmoled::display();
  const int16_t tabBase = WaveshareAmoled::kDisplayWidth / ScreenCount;
  const int16_t x = index * tabBase;
  const int16_t w = (index == ScreenCount - 1)
                        ? WaveshareAmoled::kDisplayWidth - x
                        : tabBase;
  const uint16_t bg = active ? (current ? current->accent : WaveshareAmoled::kColorAccent)
                             : WaveshareAmoled::kColorPanel;
  d.fillRect(x, 42, w, 30, bg);
  d.drawRect(x, 42, w, 30, WaveshareAmoled::kColorPanel2);
  d.setTextSize(1);
  d.setTextColor(active ? WaveshareAmoled::kColorBg : WaveshareAmoled::kColorDim, bg);
  d.setCursor(x + 7, 53);
  d.print(kScreenNames[index]);
}

void drawFrame(const char* right) {
  WaveshareAmoled::clear();
  WaveshareAmoled::header(current ? current->title : "RPG", right);
  for (uint8_t i = 0; i < ScreenCount; i++) drawTab(i, i == screen);
}

void drawValueBar(int16_t x, int16_t y, int16_t w, const char* label, int16_t value,
                  uint16_t color) {
  Adafruit_GFX& d = WaveshareAmoled::display();
  char text[28];
  snprintf(text, sizeof(text), "%s %d", label ? label : "Stat", value);
  d.setTextSize(1);
  d.setTextColor(WaveshareAmoled::kColorDim, WaveshareAmoled::kColorBg);
  d.setCursor(x, y);
  d.print(text);
  d.drawRoundRect(x, y + 12, w, 12, 5, WaveshareAmoled::kColorPanel2);
  const int16_t fill = map(clampValue(value, 0, 99), 0, 99, 0, w - 4);
  d.fillRoundRect(x + 2, y + 14, fill, 8, 4, color);
}

void drawSmallBox(int16_t x, int16_t y, int16_t w, const char* top, const char* bottom,
                  uint16_t color) {
  Adafruit_GFX& d = WaveshareAmoled::display();
  d.fillRoundRect(x, y, w, 42, 7, WaveshareAmoled::kColorPanel);
  d.drawRoundRect(x, y, w, 42, 7, color);
  d.setTextSize(1);
  d.setTextColor(WaveshareAmoled::kColorDim, WaveshareAmoled::kColorPanel);
  d.setCursor(x + 8, y + 8);
  d.print(top ? top : "");
  d.setTextColor(WaveshareAmoled::kColorText, WaveshareAmoled::kColorPanel);
  d.setCursor(x + 8, y + 24);
  d.print(bottom ? bottom : "");
}

void drawPlay() {
  char right[18];
  snprintf(right, sizeof(right), "D%u A%u", dayCount, static_cast<unsigned>(chapter + 1));
  drawFrame(right);
  WaveshareAmoled::wrapped(current->actNames[chapter], 18, 86, 29, 1,
                           current->accent);
  WaveshareAmoled::wrapped(current->questText[chapter], 18, 112, 29, 2,
                           WaveshareAmoled::kColorDim);
  drawValueBar(18, 166, 94, current->statLabels[0], health, WaveshareAmoled::kColorBad);
  drawValueBar(137, 166, 94, current->statLabels[1], focus, current->accent);
  drawValueBar(256, 166, 94, current->statLabels[2], kit, WaveshareAmoled::kColorWarn);

  for (uint8_t i = 0; i < 4; i++) {
    const int16_t x = 18 + (i % 2) * 171;
    const int16_t y = 216 + (i / 2) * 60;
    WaveshareAmoled::button(x, y, 155, 48, current->actions[i].label,
                            selected == i, current->accent);
  }

  WaveshareAmoled::wrapped(history0, 18, 338, 30, 2, WaveshareAmoled::kColorText);
  WaveshareAmoled::footer("Tap tabs/actions", "Hold BOOT home");
}

void drawSheet() {
  char right[18];
  snprintf(right, sizeof(right), "LV %u", level);
  drawFrame(right);
  WaveshareAmoled::wrapped(current->role, 18, 88, 30, 1, current->accent);
  WaveshareAmoled::wrapped(current->premise, 18, 116, 30, 3, WaveshareAmoled::kColorDim);
  drawValueBar(18, 206, 150, current->statLabels[0], health, WaveshareAmoled::kColorBad);
  drawValueBar(200, 206, 150, current->statLabels[1], focus, current->accent);
  drawValueBar(18, 252, 150, current->statLabels[2], kit, WaveshareAmoled::kColorWarn);
  drawValueBar(200, 252, 150, current->statLabels[3], standing, WaveshareAmoled::kColorGood);
  char xpText[18];
  snprintf(xpText, sizeof(xpText), "%u/100 XP", xp);
  char wealthText[18];
  snprintf(wealthText, sizeof(wealthText), "%d", wealth);
  drawSmallBox(18, 316, 150, "Level", xpText, current->accent);
  drawSmallBox(200, 316, 150, current->statLabels[4], wealthText, WaveshareAmoled::kColorWarn);
  WaveshareAmoled::footer("Swipe screens", "screen sheet");
}

void drawQuest() {
  drawFrame("Quest");
  WaveshareAmoled::wrapped(current->actNames[chapter], 18, 88, 30, 1, current->accent);
  WaveshareAmoled::wrapped(current->questText[chapter], 18, 120, 30, 4,
                           WaveshareAmoled::kColorText);
  char progress[72];
  snprintf(progress, sizeof(progress), "Progress: act %u of 4, day %u, level %u.",
           static_cast<unsigned>(chapter + 1), dayCount, level);
  WaveshareAmoled::wrapped(progress, 18, 236, 30, 2, WaveshareAmoled::kColorDim);
  WaveshareAmoled::wrapped(history0, 18, 294, 30, 2, WaveshareAmoled::kColorText);
  WaveshareAmoled::wrapped(history1, 18, 350, 30, 2, WaveshareAmoled::kColorDim);
  WaveshareAmoled::footer("Recent turns", "screen quest");
}

void drawLore() {
  drawFrame("Lore");
  WaveshareAmoled::wrapped(current->actNames[chapter], 18, 88, 30, 1, current->accent);
  WaveshareAmoled::wrapped(current->lore[chapter], 18, 120, 30, 5,
                           WaveshareAmoled::kColorText);
  WaveshareAmoled::wrapped("Older campfire notes:", 18, 270, 30, 1,
                           WaveshareAmoled::kColorDim);
  WaveshareAmoled::wrapped(history1, 18, 304, 30, 2, WaveshareAmoled::kColorDim);
  WaveshareAmoled::wrapped(history2, 18, 360, 30, 2, WaveshareAmoled::kColorDim);
  WaveshareAmoled::footer("World notes", "screen lore");
}

void drawPack() {
  drawFrame("Pack");
  char wealthText[36];
  snprintf(wealthText, sizeof(wealthText), "%s: %d", current->statLabels[4], wealth);
  WaveshareAmoled::wrapped(wealthText, 18, 88, 30, 1, current->accent);
  WaveshareAmoled::wrapped("Unlocked kit and favors carry between days.", 18, 116, 30, 2,
                           WaveshareAmoled::kColorDim);
  for (uint8_t i = 0; i < 4; i++) {
    const bool owned = (inventoryMask & (1 << i)) != 0;
    const int16_t x = 18 + (i % 2) * 171;
    const int16_t y = 184 + (i / 2) * 68;
    drawSmallBox(x, y, 155, owned ? "Acquired" : "Locked",
                 current->inventory[i],
                 owned ? current->accent : WaveshareAmoled::kColorPanel2);
  }
  drawValueBar(18, 340, 150, current->statLabels[3], standing, WaveshareAmoled::kColorGood);
  drawValueBar(200, 340, 150, current->statLabels[2], kit, WaveshareAmoled::kColorWarn);
  WaveshareAmoled::footer("Long press item", "screen pack");
}

void draw() {
  if (!current) return;
  switch (screen) {
    case ScreenSheet: drawSheet(); break;
    case ScreenQuest: drawQuest(); break;
    case ScreenLore: drawLore(); break;
    case ScreenPack: drawPack(); break;
    case ScreenPlay:
    default: drawPlay(); break;
  }
  lastDraw = millis();
}

void awardXp(uint8_t amount) {
  xp += amount;
  while (xp >= 100 && level < 9) {
    xp -= 100;
    level++;
    health = clampValue(health + 12, 0, 99);
    focus = clampValue(focus + 8, 0, 99);
    pushHistory("Level gained. The campaign sheet has changed.");
  }
}

void maybeAdvanceChapter() {
  uint8_t nextChapter = static_cast<uint8_t>((dayCount - 1) / 5);
  if (nextChapter > 3) nextChapter = 3;
  if (nextChapter > chapter) {
    chapter = nextChapter;
    pushHistory(current->questText[chapter]);
  }
}

void applyAction(uint8_t index) {
  if (!current || index > 3) return;
  selected = index;
  const StoryGameAction& action = current->actions[index];
  dayCount++;
  health += action.healthDelta;
  focus += action.focusDelta;
  kit += action.kitDelta;
  standing += action.standingDelta;
  wealth += action.wealthDelta;
  inventoryMask |= action.itemMask;
  awardXp(action.xpGain);
  clampStats();
  if (health == 0) {
    health = 24;
    focus = clampValue(focus - 10, 0, 99);
    kit = clampValue(kit - 10, 0, 99);
    pushHistory("You survived a collapse and limped back changed.");
  }
  maybeAdvanceChapter();
  pushHistory(action.result);
  writeJournal(action);
  saveState();
  draw();
}

bool setScreenByName(const char* name) {
  for (uint8_t i = 0; i < ScreenCount; i++) {
    if (strcmp(name, kScreenNames[i]) == 0) {
      screen = static_cast<StoryScreen>(i);
      return true;
    }
  }
  if (strcmp(name, "play") == 0) screen = ScreenPlay;
  else if (strcmp(name, "sheet") == 0) screen = ScreenSheet;
  else if (strcmp(name, "quest") == 0) screen = ScreenQuest;
  else if (strcmp(name, "lore") == 0) screen = ScreenLore;
  else if (strcmp(name, "pack") == 0) screen = ScreenPack;
  else return false;
  return true;
}

void handleTap(const WaveshareAmoled::Event& event) {
  if (event.y >= 42 && event.y < 72) {
    uint8_t tab = static_cast<uint8_t>(
        event.x / (WaveshareAmoled::kDisplayWidth / ScreenCount));
    if (tab >= ScreenCount) tab = ScreenCount - 1;
    screen = static_cast<StoryScreen>(tab);
    draw();
    return;
  }
  if (screen != ScreenPlay) return;
  for (uint8_t i = 0; i < 4; i++) {
    const int16_t x = 18 + (i % 2) * 171;
    const int16_t y = 216 + (i / 2) * 60;
    if (WaveshareAmoled::hit(event, x, y, 155, 48)) {
      applyAction(i);
      break;
    }
  }
}

}  // namespace

void beginStoryGame(const StoryGameProfile& profile) {
  current = &profile;
  WaveshareAmoled::begin(profile.title, true);
  setDefaultCampaignState();
  if (loadState()) {
    pushHistory(profile.premise);
  }
  WaveshareAmoled::serialHelp(
      "help, status, screen play|sheet|quest|lore|pack, act 1-4, save, reset, home");
  draw();
}

void loopStoryGame() {
  if (!current) return;
  const WaveshareAmoled::Event event = WaveshareAmoled::poll();
  if (WaveshareAmoled::isHome(event)) WaveshareAmoled::returnToLauncher();

  if (event.type == WaveshareAmoled::EventSwipeLeft ||
      event.type == WaveshareAmoled::EventSwipeDown) {
    screen = static_cast<StoryScreen>((screen + 1) % ScreenCount);
    draw();
  } else if (event.type == WaveshareAmoled::EventSwipeRight ||
             event.type == WaveshareAmoled::EventSwipeUp) {
    screen = static_cast<StoryScreen>((screen + ScreenCount - 1) % ScreenCount);
    draw();
  } else if (event.type == WaveshareAmoled::EventTap ||
             event.type == WaveshareAmoled::EventLongPress) {
    handleTap(event);
  } else if (event.type == WaveshareAmoled::EventSerialLine) {
    if (strcmp(event.line, "home") == 0) {
      WaveshareAmoled::returnToLauncher();
    } else if (strncmp(event.line, "act ", 4) == 0) {
      const int action = atoi(event.line + 4);
      if (action >= 1 && action <= 4) applyAction(static_cast<uint8_t>(action - 1));
    } else if (strncmp(event.line, "screen ", 7) == 0) {
      if (setScreenByName(event.line + 7)) draw();
    } else if (strcmp(event.line, "save") == 0) {
      Serial.printf("save=%d\n", saveState() ? 1 : 0);
    } else if (strcmp(event.line, "reset") == 0) {
      resetCampaign();
      draw();
    } else if (strcmp(event.line, "status") == 0 || strcmp(event.line, "help") == 0) {
      Serial.printf("%s day=%u act=%u level=%u xp=%u hp=%d focus=%d kit=%d "
                    "standing=%d wealth=%d inv=%u screen=%s\n",
                    current->title, dayCount, static_cast<unsigned>(chapter + 1),
                    level, xp, health, focus, kit, standing, wealth,
                    inventoryMask, kScreenNames[screen]);
    }
  }

  if (millis() - lastDraw > 30000) draw();
  delay(16);
}

}  // namespace WaveshareAmoledPorts
