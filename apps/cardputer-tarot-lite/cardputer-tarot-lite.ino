#include <WaveshareAmoledAppKit.h>

using namespace WaveshareAmoled;

const char* cards[] = {
  "The Fool", "The Magician", "The High Priestess", "The Empress", "The Emperor",
  "The Hierophant", "The Lovers", "The Chariot", "Strength", "The Hermit",
  "Wheel of Fortune", "Justice", "The Hanged Man", "Death", "Temperance",
  "The Devil", "The Tower", "The Star", "The Moon", "The Sun", "Judgement",
  "The World",
  "Ace of Wands", "Two of Wands", "Three of Wands", "Four of Wands",
  "Five of Wands", "Six of Wands", "Seven of Wands", "Eight of Wands",
  "Nine of Wands", "Ten of Wands", "Page of Wands", "Knight of Wands",
  "Queen of Wands", "King of Wands",
  "Ace of Cups", "Two of Cups", "Three of Cups", "Four of Cups",
  "Five of Cups", "Six of Cups", "Seven of Cups", "Eight of Cups",
  "Nine of Cups", "Ten of Cups", "Page of Cups", "Knight of Cups",
  "Queen of Cups", "King of Cups",
  "Ace of Swords", "Two of Swords", "Three of Swords", "Four of Swords",
  "Five of Swords", "Six of Swords", "Seven of Swords", "Eight of Swords",
  "Nine of Swords", "Ten of Swords", "Page of Swords", "Knight of Swords",
  "Queen of Swords", "King of Swords",
  "Ace of Pentacles", "Two of Pentacles", "Three of Pentacles",
  "Four of Pentacles", "Five of Pentacles", "Six of Pentacles",
  "Seven of Pentacles", "Eight of Pentacles", "Nine of Pentacles",
  "Ten of Pentacles", "Page of Pentacles", "Knight of Pentacles",
  "Queen of Pentacles", "King of Pentacles"
};

constexpr uint8_t kCardCount = sizeof(cards) / sizeof(cards[0]);
uint8_t spread[3] = {0, 1, 2};
bool reversed[3] = {false, false, false};
uint8_t spreadSize = 1;
char note[96] = "Tap one-card or three-card draw.";

void saveReading() {
  if (!sdReady()) return;
  ensureDir("/waveshare-os/tarot");
  char line[180];
  if (spreadSize == 1) {
    snprintf(line, sizeof(line), "one,%s,%s", cards[spread[0]],
             reversed[0] ? "reversed" : "upright");
  } else {
    snprintf(line, sizeof(line), "three,%s/%s,%s/%s,%s/%s",
             cards[spread[0]], reversed[0] ? "R" : "U",
             cards[spread[1]], reversed[1] ? "R" : "U",
             cards[spread[2]], reversed[2] ? "R" : "U");
  }
  appendLine("/waveshare-os/tarot/history.csv", line);
}

void drawCardBox(uint8_t slot, const char* label) {
  const int16_t x = 18;
  const int16_t y = 96 + slot * 86;
  display().fillRoundRect(x, y, 332, 70, 8, kColorPanel);
  display().drawRoundRect(x, y, 332, 70, 8, kColorPanel2);
  display().setTextSize(2);
  display().setTextColor(kColorDim, kColorPanel);
  display().setCursor(x + 14, y + 10);
  display().print(label);
  display().setTextColor(kColorText, kColorPanel);
  display().setCursor(x + 14, y + 36);
  display().print(cards[spread[slot]]);
  if (reversed[slot]) {
    display().setTextColor(kColorWarn, kColorPanel);
    display().setCursor(x + 248, y + 36);
    display().print("rev");
  }
}

void drawScreen() {
  clear();
  header("Cardputer Tarot", spreadSize == 1 ? "1 card" : "3 cards");
  wrapped("Touch-first Tarot Lite. Readings save to SD so the full Cardputer deck history has a Waveshare landing spot.", 18, 54, 30, 2);
  drawCardBox(0, spreadSize == 1 ? "Reading" : "Past");
  if (spreadSize == 3) {
    drawCardBox(1, "Present");
    drawCardBox(2, "Next");
  }
  button(18, 348, 155, 52, "One Card", false, kColorAccent);
  button(195, 348, 155, 52, "Three", false, kColorWarn);
  footer("Tap draw", "Hold BOOT home");
}

void drawSpread(uint8_t count) {
  spreadSize = count;
  for (uint8_t i = 0; i < count; i++) {
    spread[i] = random(0, kCardCount);
    reversed[i] = random(0, 2) == 1;
  }
  saveReading();
  drawScreen();
}

void setup() {
  begin("Tarot Lite", true);
  serialHelp("one, three, home");
  randomSeed(esp_random());
  drawSpread(1);
}

void loop() {
  Event event = poll();
  if (isHome(event)) returnToLauncher();
  if (event.type == EventBootShort) returnToLauncher();
  if (event.type == EventTap) {
    if (hit(event, 18, 348, 155, 52)) drawSpread(1);
    if (hit(event, 195, 348, 155, 52)) drawSpread(3);
  } else if (event.type == EventSwipeLeft || event.type == EventSwipeRight) {
    drawSpread(spreadSize);
  } else if (event.type == EventSerialLine) {
    if (strcmp(event.line, "home") == 0) returnToLauncher();
    if (strcmp(event.line, "one") == 0) drawSpread(1);
    if (strcmp(event.line, "three") == 0) drawSpread(3);
  }
  delay(16);
}
