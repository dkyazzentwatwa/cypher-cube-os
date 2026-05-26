#pragma once

#include <Arduino.h>

constexpr uint32_t SERIAL_BAUD = 115200;

constexpr uint16_t LCD_WIDTH = 368;
constexpr uint16_t LCD_HEIGHT = 448;
constexpr uint8_t LCD_ROTATION = 0;
constexpr uint8_t DISPLAY_TEXT_SIZE = 2;

constexpr int PIN_LCD_SDIO0 = 4;
constexpr int PIN_LCD_SDIO1 = 5;
constexpr int PIN_LCD_SDIO2 = 6;
constexpr int PIN_LCD_SDIO3 = 7;
constexpr int PIN_LCD_SCLK = 11;
constexpr int PIN_LCD_CS = 12;
constexpr int PIN_LCD_RST = -1;

constexpr int PIN_TOUCH_SDA = 15;
constexpr int PIN_TOUCH_SCL = 14;
constexpr int PIN_TOUCH_RST = -1;
constexpr int PIN_TOUCH_INT = 21;

constexpr int PIN_SD_CLK = 2;
constexpr int PIN_SD_CMD = 1;
constexpr int PIN_SD_D0 = 3;

constexpr int PIN_BOOT_BUTTON = 0;
constexpr bool BOOT_BUTTON_ACTIVE_LOW = true;

constexpr uint8_t AMOLED_EXPANDER_I2C_ADDR = 0x20;
constexpr uint8_t AMOLED_BRIGHTNESS = 220;

constexpr const char* APP_DIR = "/waveshare-os/apps";
constexpr const char* APP_MANIFEST = "/waveshare-os/apps/apps.json";
constexpr const char* PREF_NS = "wamoledos";
constexpr uint8_t MAX_APPS = 32;
constexpr size_t FLASH_CHUNK = 4096;
constexpr uint8_t ESP_APP_IMAGE_MAGIC = 0xE9;

constexpr uint16_t COLOR_BG = 0x0000;
constexpr uint16_t COLOR_PANEL = 0x1082;
constexpr uint16_t COLOR_PANEL_2 = 0x2104;
constexpr uint16_t COLOR_TEXT = 0xFFFF;
constexpr uint16_t COLOR_DIM = 0xBDF7;
constexpr uint16_t COLOR_ACCENT = 0x07FF;
constexpr uint16_t COLOR_GOOD = 0x07E0;
constexpr uint16_t COLOR_WARN = 0xFD20;
constexpr uint16_t COLOR_BAD = 0xF800;

