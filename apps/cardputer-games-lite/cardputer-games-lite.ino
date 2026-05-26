#include <SD_MMC.h>
#include <WaveshareAmoledAppKit.h>

using namespace WaveshareAmoled;

enum Mode : uint8_t {
  MODE_MENU,
  MODE_TARGET,
  MODE_SWIPE,
  MODE_REACTION,
  MODE_MEMORY,
  MODE_RESULTS
};

struct ScoreEntry {
  char game[12];
  uint16_t score;
  uint16_t detail;
};

Mode mode = MODE_MENU;
Mode lastMode = MODE_TARGET;
ScoreEntry bestScores[4] = {{"target", 0, 0}, {"swipe", 0, 0},
                            {"reaction", 0, 0}, {"memory", 0, 0}};
ScoreEntry recentScores[5];
uint8_t recentCount = 0;
uint16_t score = 0;
uint16_t roundCount = 0;
uint16_t targetX = 120;
uint16_t targetY = 180;
uint8_t streak = 0;
uint8_t misses = 0;
uint8_t swipeDir = 0;
uint32_t modeStartedAt = 0;
uint32_t reactionAt = 0;
uint16_t reactionBest = 9999;
uint32_t reactionTotal = 0;
bool reactionReady = false;
uint8_t memoryPattern[6];
uint8_t memoryStep = 0;
uint8_t memoryRound = 0;
bool showingPattern = false;
uint32_t patternUntil = 0;
char statusText[128] = "Pick a game.";
char resultTitle[32] = "Results";
char resultBody[180] = "";

uint8_t gameIndex(const char* game) {
  if (strcmp(game, "target") == 0) return 0;
  if (strcmp(game, "swipe") == 0) return 1;
  if (strcmp(game, "reaction") == 0) return 2;
  return 3;
}

void ensureGameDir() {
  ensureDir("/waveshare-os/cardputer-games");
}

void loadScores() {
  recentCount = 0;
  if (!sdReady()) return;
  ensureGameDir();
  File file = SD_MMC.open("/waveshare-os/cardputer-games/scores.txt", FILE_READ);
  if (!file) return;
  while (file.available()) {
    char line[96];
    const size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = '\0';
    char game[12] = "";
    unsigned parsedScore = 0;
    unsigned detail = 0;
    if (sscanf(line, "%11s score=%u detail=%u", game, &parsedScore, &detail) == 3) {
      const uint8_t idx = gameIndex(game);
      if (parsedScore > bestScores[idx].score) {
        bestScores[idx].score = parsedScore;
        bestScores[idx].detail = detail;
      }
      if (recentCount < 5) {
        strncpy(recentScores[recentCount].game, game, sizeof(recentScores[recentCount].game) - 1);
        recentScores[recentCount].score = parsedScore;
        recentScores[recentCount].detail = detail;
        recentCount++;
      } else {
        for (uint8_t i = 1; i < 5; i++) recentScores[i - 1] = recentScores[i];
        strncpy(recentScores[4].game, game, sizeof(recentScores[4].game) - 1);
        recentScores[4].score = parsedScore;
        recentScores[4].detail = detail;
      }
    }
  }
  file.close();
}

void saveScore(const char* game, uint16_t detail) {
  if (!sdReady()) return;
  ensureGameDir();
  char line[96];
  snprintf(line, sizeof(line), "%s score=%u detail=%u", game, score, detail);
  appendLine("/waveshare-os/cardputer-games/scores.txt", line);
  loadScores();
}

void drawScoreLine(int16_t y, const char* label, const ScoreEntry& entry) {
  char line[48];
  snprintf(line, sizeof(line), "%s %u", label, entry.score);
  display().setTextSize(1);
  display().setTextColor(kColorDim, kColorBg);
  display().setCursor(22, y);
  display().print(line);
}

void drawMenu() {
  mode = MODE_MENU;
  clear();
  header("Cardputer Games", "Arcade");
  wrapped("Touch arcade shelf with high scores, streaks, reaction stats, and quick replay.", 18, 54, 30, 2);
  button(18, 118, 155, 48, "Target Tap", false, kColorAccent);
  button(195, 118, 155, 48, "Swipe Drill", false, kColorGood);
  button(18, 178, 155, 48, "Reaction", false, kColorWarn);
  button(195, 178, 155, 48, "Memory Grid", false, 0xF81F);
  display().setTextSize(2);
  display().setTextColor(kColorText, kColorBg);
  display().setCursor(18, 252);
  display().print("Best Scores");
  drawScoreLine(288, "Target", bestScores[0]);
  drawScoreLine(310, "Swipe", bestScores[1]);
  drawScoreLine(332, "React", bestScores[2]);
  drawScoreLine(354, "Memory", bestScores[3]);
  footer("Tap to play", "Hold BOOT home");
}

void drawHud(const char* title) {
  char right[28];
  snprintf(right, sizeof(right), "%u pts", score);
  header(title, right);
}

void drawResults() {
  mode = MODE_RESULTS;
  clear();
  header(resultTitle, "Saved");
  wrapped(resultBody, 18, 62, 30, 5, kColorText);
  display().setTextSize(2);
  display().setTextColor(kColorDim, kColorBg);
  display().setCursor(22, 232);
  display().print("Recent");
  const uint8_t start = recentCount > 4 ? recentCount - 4 : 0;
  for (uint8_t i = start; i < recentCount; i++) {
    char line[56];
    snprintf(line, sizeof(line), "%s %u", recentScores[i].game, recentScores[i].score);
    display().setCursor(24, 266 + (i - start) * 26);
    display().print(line);
  }
  button(18, 372, 155, 48, "Replay", false, kColorGood);
  button(195, 372, 155, 48, "Menu", false, kColorAccent);
}

void finishGame(const char* game, uint16_t detail, const char* title, const char* body) {
  saveScore(game, detail);
  snprintf(resultTitle, sizeof(resultTitle), "%s", title);
  snprintf(resultBody, sizeof(resultBody), "%s", body);
  drawResults();
}

void newTarget() {
  targetX = random(48, 320);
  targetY = random(98, 330);
}

void drawTarget() {
  clear();
  drawHud("Target Tap");
  display().drawCircle(targetX, targetY, 36, kColorAccent);
  display().fillCircle(targetX, targetY, 22, kColorAccent);
  char line[64];
  snprintf(line, sizeof(line), "Round %u/20  Streak %u  Miss %u",
           roundCount, streak, misses);
  wrapped(line, 18, 354, 30, 1, kColorDim);
  wrapped(statusText, 18, 382, 30, 1, kColorText);
  footer("30 sec timed", "BOOT menu");
}

void startTarget() {
  mode = MODE_TARGET;
  lastMode = MODE_TARGET;
  score = 0;
  roundCount = 0;
  streak = 0;
  misses = 0;
  modeStartedAt = millis();
  snprintf(statusText, sizeof(statusText), "Tap fast. Streaks add bonus.");
  newTarget();
  drawTarget();
}

const char* dirName(uint8_t dir) {
  switch (dir) {
    case 0: return "UP";
    case 1: return "DOWN";
    case 2: return "LEFT";
    default: return "RIGHT";
  }
}

void drawSwipe() {
  clear();
  drawHud("Swipe Drill");
  display().setTextSize(5);
  display().setTextColor(kColorGood, kColorBg);
  display().setCursor(78, 148);
  display().print(dirName(swipeDir));
  char line[72];
  snprintf(line, sizeof(line), "Round %u/16  Streak %u", roundCount, streak);
  wrapped(line, 24, 270, 30, 1, kColorDim);
  wrapped(statusText, 24, 306, 30, 2, kColorText);
  footer("Swipe anywhere", "BOOT menu");
}

void startSwipe() {
  mode = MODE_SWIPE;
  lastMode = MODE_SWIPE;
  score = 0;
  roundCount = 0;
  streak = 0;
  misses = 0;
  swipeDir = random(0, 4);
  snprintf(statusText, sizeof(statusText), "Match the shown direction.");
  drawSwipe();
}

void drawReaction() {
  clear(reactionReady ? 0x0400 : kColorBg);
  drawHud("Reaction");
  display().setTextSize(3);
  display().setTextColor(reactionReady ? kColorGood : kColorDim,
                         reactionReady ? 0x0400 : kColorBg);
  display().setCursor(42, 156);
  display().print(reactionReady ? "TAP NOW" : "Wait for green");
  char line[64];
  const uint16_t avg = roundCount ? reactionTotal / roundCount : 0;
  snprintf(line, sizeof(line), "Round %u/6  Best %ums Avg %ums", roundCount, reactionBest, avg);
  wrapped(line, 18, 286, 30, 2, reactionReady ? kColorText : kColorDim,
          reactionReady ? 0x0400 : kColorBg);
  wrapped(statusText, 18, 348, 30, 2, reactionReady ? kColorText : kColorDim,
          reactionReady ? 0x0400 : kColorBg);
  footer("Fast tap", "BOOT menu");
}

void startReaction() {
  mode = MODE_REACTION;
  lastMode = MODE_REACTION;
  score = 0;
  roundCount = 0;
  reactionBest = 9999;
  reactionTotal = 0;
  reactionReady = false;
  reactionAt = millis() + random(1200, 3600);
  snprintf(statusText, sizeof(statusText), "Early taps count as misses.");
  drawReaction();
}

void drawMemoryGrid(uint8_t lit) {
  clear();
  drawHud("Memory Grid");
  char line[64];
  snprintf(line, sizeof(line), "Round %u/6  Step %u", memoryRound + 1, memoryStep + 1);
  wrapped(line, 18, 74, 30, 1, kColorDim);
  for (uint8_t i = 0; i < 4; i++) {
    const int16_t x = 54 + (i % 2) * 138;
    const int16_t y = 126 + (i / 2) * 112;
    const uint16_t color = i == lit ? kColorWarn : (i == 0 ? kColorAccent : i == 1 ? kColorGood : i == 2 ? 0xF81F : 0x7DFF);
    display().fillRoundRect(x, y, 106, 84, 8, color);
    display().drawRoundRect(x, y, 106, 84, 8, kColorText);
  }
  wrapped(statusText, 18, 360, 30, 2, kColorText);
  footer("Repeat pattern", "BOOT menu");
}

void showMemoryStep() {
  showingPattern = true;
  patternUntil = millis() + 650;
  snprintf(statusText, sizeof(statusText), "Watch the pattern.");
  drawMemoryGrid(memoryPattern[memoryStep]);
}

void startMemory() {
  mode = MODE_MEMORY;
  lastMode = MODE_MEMORY;
  score = 0;
  roundCount = 0;
  memoryRound = 0;
  memoryStep = 0;
  misses = 0;
  for (uint8_t i = 0; i < 6; i++) memoryPattern[i] = random(0, 4);
  showMemoryStep();
}

void replayLast() {
  if (lastMode == MODE_TARGET) startTarget();
  else if (lastMode == MODE_SWIPE) startSwipe();
  else if (lastMode == MODE_REACTION) startReaction();
  else startMemory();
}

void handleSerial(const char* line) {
  if (strcmp(line, "home") == 0) returnToLauncher();
  if (strcmp(line, "menu") == 0) drawMenu();
  if (strcmp(line, "target") == 0) startTarget();
  if (strcmp(line, "swipe") == 0) startSwipe();
  if (strcmp(line, "reaction") == 0) startReaction();
  if (strcmp(line, "memory") == 0) startMemory();
  if (strcmp(line, "scores") == 0) {
    Serial.printf("target=%u swipe=%u reaction=%u memory=%u\n",
                  bestScores[0].score, bestScores[1].score,
                  bestScores[2].score, bestScores[3].score);
  }
}

void setup() {
  begin("Cardputer Games", true);
  serialHelp("menu, target, swipe, reaction, memory, scores, home");
  randomSeed(esp_random());
  loadScores();
  drawMenu();
}

void loop() {
  Event event = poll();
  if (isHome(event)) returnToLauncher();

  if (event.type == EventSerialLine) handleSerial(event.line);

  if (event.type == EventBootShort && mode != MODE_MENU) {
    drawMenu();
  } else if (event.type == EventBootShort) {
    returnToLauncher();
  }

  if (mode == MODE_MENU && event.type == EventTap) {
    if (hit(event, 18, 118, 155, 48)) startTarget();
    if (hit(event, 195, 118, 155, 48)) startSwipe();
    if (hit(event, 18, 178, 155, 48)) startReaction();
    if (hit(event, 195, 178, 155, 48)) startMemory();
  } else if (mode == MODE_RESULTS && event.type == EventTap) {
    if (hit(event, 18, 372, 155, 48)) replayLast();
    if (hit(event, 195, 372, 155, 48)) drawMenu();
  } else if (mode == MODE_TARGET && event.type == EventTap) {
    const int16_t dx = static_cast<int16_t>(event.x) - static_cast<int16_t>(targetX);
    const int16_t dy = static_cast<int16_t>(event.y) - static_cast<int16_t>(targetY);
    roundCount++;
    if (dx * dx + dy * dy < 38 * 38) {
      streak++;
      score += 10 + streak * 2;
      snprintf(statusText, sizeof(statusText), "Hit. Bonus streak active.");
      newTarget();
    } else {
      streak = 0;
      misses++;
      snprintf(statusText, sizeof(statusText), "Miss. Recenter and recover.");
    }
    if (roundCount >= 20 || millis() - modeStartedAt > 30000) {
      char body[150];
      snprintf(body, sizeof(body), "Target run complete. Score %u, misses %u, final streak %u.",
               score, misses, streak);
      finishGame("target", misses, "Target Results", body);
    } else {
      drawTarget();
    }
  } else if (mode == MODE_SWIPE &&
             (event.type == EventSwipeUp || event.type == EventSwipeDown ||
              event.type == EventSwipeLeft || event.type == EventSwipeRight)) {
    const uint8_t got = event.type == EventSwipeUp ? 0 :
                        event.type == EventSwipeDown ? 1 :
                        event.type == EventSwipeLeft ? 2 : 3;
    roundCount++;
    if (got == swipeDir) {
      streak++;
      score += 10 + streak * 3;
      snprintf(statusText, sizeof(statusText), "Correct. Combo keeps climbing.");
    } else {
      streak = 0;
      misses++;
      snprintf(statusText, sizeof(statusText), "Wrong way. Combo reset.");
    }
    swipeDir = random(0, 4);
    if (roundCount >= 16) {
      char body[150];
      snprintf(body, sizeof(body), "Swipe drill complete. Score %u with %u misses.",
               score, misses);
      finishGame("swipe", misses, "Swipe Results", body);
    } else {
      drawSwipe();
    }
  } else if (mode == MODE_REACTION) {
    if (!reactionReady && millis() > reactionAt) {
      reactionReady = true;
      drawReaction();
    }
    if (event.type == EventTap) {
      if (reactionReady) {
        const uint16_t reaction = static_cast<uint16_t>(millis() - reactionAt);
        if (reaction < reactionBest) reactionBest = reaction;
        reactionTotal += reaction;
        score += reaction < 1000 ? (1000 - reaction) / 5 + 20 : 5;
        roundCount++;
        snprintf(statusText, sizeof(statusText), "%ums reaction.", reaction);
      } else {
        misses++;
        score = score > 12 ? score - 12 : 0;
        snprintf(statusText, sizeof(statusText), "Too early. Penalty.");
      }
      if (roundCount >= 6) {
        char body[150];
        const uint16_t avg = roundCount ? reactionTotal / roundCount : 0;
        snprintf(body, sizeof(body), "Reaction test complete. Score %u, best %ums, average %ums.",
                 score, reactionBest, avg);
        finishGame("reaction", avg, "Reaction Results", body);
      } else {
        reactionReady = false;
        reactionAt = millis() + random(1200, 3600);
        drawReaction();
      }
    }
  } else if (mode == MODE_MEMORY) {
    if (showingPattern && millis() > patternUntil) {
      showingPattern = false;
      memoryStep = 0;
      snprintf(statusText, sizeof(statusText), "Repeat the lit cells.");
      drawMemoryGrid(255);
    } else if (!showingPattern && event.type == EventTap) {
      uint8_t cell = 255;
      for (uint8_t i = 0; i < 4; i++) {
        const int16_t x = 54 + (i % 2) * 138;
        const int16_t y = 126 + (i / 2) * 112;
        if (hit(event, x, y, 106, 84)) cell = i;
      }
      if (cell != 255) {
        if (cell == memoryPattern[memoryStep]) {
          score += 12 + memoryRound * 3;
          memoryStep++;
          if (memoryStep > memoryRound) {
            memoryRound++;
            roundCount++;
            if (memoryRound >= 6) {
              char body[150];
              snprintf(body, sizeof(body), "Memory grid clear. Score %u with %u misses.",
                       score, misses);
              finishGame("memory", misses, "Memory Results", body);
            } else {
              memoryStep = 0;
              showMemoryStep();
            }
          } else {
            snprintf(statusText, sizeof(statusText), "Good. Keep going.");
            drawMemoryGrid(255);
          }
        } else {
          misses++;
          score = score > 10 ? score - 10 : 0;
          snprintf(statusText, sizeof(statusText), "Missed pattern. Watch again.");
          memoryStep = 0;
          showMemoryStep();
        }
      }
    }
  }
  delay(16);
}
