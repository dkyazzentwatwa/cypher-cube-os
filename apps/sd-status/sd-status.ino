#include <Adafruit_XCA9554.h>
#include <Arduino_GFX_Library.h>
#include <SD_MMC.h>
#include <Wire.h>

#include <WaveshareAmoledIntro.h>
#include <WaveshareAmoledReturn.h>

constexpr uint16_t LCD_WIDTH = 368;
constexpr uint16_t LCD_HEIGHT = 448;
constexpr int PIN_LCD_SDIO0 = 4;
constexpr int PIN_LCD_SDIO1 = 5;
constexpr int PIN_LCD_SDIO2 = 6;
constexpr int PIN_LCD_SDIO3 = 7;
constexpr int PIN_LCD_SCLK = 11;
constexpr int PIN_LCD_CS = 12;
constexpr int PIN_TOUCH_SDA = 15;
constexpr int PIN_TOUCH_SCL = 14;
constexpr int PIN_SD_CLK = 2;
constexpr int PIN_SD_CMD = 1;
constexpr int PIN_SD_D0 = 3;
constexpr int PIN_BOOT_BUTTON = 0;
constexpr uint8_t AMOLED_EXPANDER_I2C_ADDR = 0x20;

Arduino_DataBus* bus = nullptr;
Arduino_SH8601* gfx = nullptr;
Adafruit_XCA9554 expander;

void setupDisplay() {
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
  if (expander.begin(AMOLED_EXPANDER_I2C_ADDR, &Wire)) {
    for (uint8_t pin = 0; pin < 3; pin++) {
      expander.pinMode(pin, OUTPUT);
      expander.digitalWrite(pin, LOW);
    }
    delay(20);
    for (uint8_t pin = 0; pin < 3; pin++) expander.digitalWrite(pin, HIGH);
  }
  bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_SDIO0, PIN_LCD_SDIO1,
                              PIN_LCD_SDIO2, PIN_LCD_SDIO3);
  gfx = new Arduino_SH8601(bus, -1, 0, LCD_WIDTH, LCD_HEIGHT);
  gfx->begin();
  gfx->setBrightness(220);
  WaveshareAmoledIntro::draw(*gfx, LCD_WIDTH, LCD_HEIGHT, "SD Status");
  gfx->fillScreen(0x0000);
  gfx->fillRect(0, 0, LCD_WIDTH, 42, 0x0186);
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF, 0x0186);
  gfx->setCursor(14, 13);
  gfx->print("SD Status");
}

void drawLine(int y, const char* label, const String& value) {
  gfx->setTextSize(2);
  gfx->setTextColor(0xBDF7, 0x0000);
  gfx->setCursor(20, y);
  gfx->print(label);
  gfx->setTextColor(0xFFFF, 0x0000);
  gfx->setCursor(170, y);
  gfx->print(value);
}

void drawStatus() {
  gfx->fillRect(0, 58, LCD_WIDTH, 330, 0x0000);
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  bool ok = SD_MMC.begin("/sdcard", true, false, 25000);
  drawLine(76, "Mounted", ok ? "yes" : "no");
  if (ok) {
    drawLine(112, "Type", String(SD_MMC.cardType()));
    drawLine(148, "Size MB", String(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
    drawLine(184, "Used MB", String(SD_MMC.usedBytes() / (1024ULL * 1024ULL)));
    drawLine(220, "Total MB", String(SD_MMC.totalBytes() / (1024ULL * 1024ULL)));
    File manifest = SD_MMC.open("/waveshare-os/apps/apps.json", FILE_READ);
    drawLine(256, "Catalog", manifest ? "found" : "missing");
    if (manifest) manifest.close();
  }
  gfx->setTextColor(0xBDF7, 0x0000);
  gfx->setCursor(20, 400);
  gfx->print("BOOT returns to launcher");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  setupDisplay();
  drawStatus();
}

void loop() {
  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
    delay(500);
    if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
      WaveshareAmoledOs::returnToLauncher();
    }
  }
  delay(50);
}
