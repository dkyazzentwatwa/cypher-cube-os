#pragma once

// Shared onboard-sensor HAL for the Waveshare ESP32-S3-Touch-AMOLED-1.8.
//
// Subsystems: Battery (AXP2101), RTC (PCF85063), IMU (QMI8658), Audio (ES8311).
// Each subsystem lazy-inits and is independently optional: a missing or failed
// chip never blocks boot and its accessors degrade gracefully (available()==false,
// percent()==-1, etc.).
//
// This layer does NOT own or re-init the display/touch. It reuses the shared
// Wire bus (SDA 15 / SCL 14) that WaveshareAmoledAppKit and DisplayPort already
// bring up; begin() calls Wire.begin() idempotently with the shared pins.

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <Wire.h>

namespace WaveshareAmoled {
namespace Sensors {

// I2C addresses on the shared bus (no conflicts: touch 0x38, expander 0x20).
constexpr uint8_t kAddrAxp2101 = 0x34;
constexpr uint8_t kAddrPcf85063 = 0x51;
constexpr uint8_t kAddrQmi8658 = 0x6B;
constexpr uint8_t kAddrEs8311 = 0x18;

// Shared I2C pins (match src/BoardConfig.h and the AppKit constants).
constexpr int kPinSda = 15;
constexpr int kPinScl = 14;

// ES8311 I2S pinout for this board.
constexpr int kPinI2sMck = 16;
constexpr int kPinI2sBck = 9;
constexpr int kPinI2sDin = 10;   // codec -> ESP (mic capture)
constexpr int kPinI2sWs = 45;
constexpr int kPinI2sDout = 8;   // ESP -> codec (speaker)
constexpr int kPinPaEnable = 46; // power-amplifier enable

// Initialize every available subsystem on the shared Wire bus. Safe to call
// once after display/touch begin(). Audio is initialized lazily on first use.
void begin(TwoWire& wire = Wire);

// ---------------------------------------------------------------- Battery ---
namespace Battery {
bool available();
int percent();        // 0..100, or -1 if unknown / no battery
uint16_t voltageMv(); // battery voltage in mV, 0 if unknown
bool isCharging();
bool isVbusPresent();
bool isPresent();
}  // namespace Battery

// -------------------------------------------------------------------- RTC ---
struct DateTime {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  bool valid = false;
};

namespace Rtc {
bool available();
DateTime now();
void set(const DateTime& dt);
// Writes "HH:MM" into out (needs >= 6 bytes). Returns false if no RTC.
bool formatHhMm(char* out, size_t len);
}  // namespace Rtc

// -------------------------------------------------------------------- IMU ---
struct Vec3 {
  float x = 0;
  float y = 0;
  float z = 0;
};

enum Orientation {
  OrientUnknown,
  OrientFaceUp,
  OrientFaceDown,
  OrientPortrait,
  OrientPortraitInverted,
  OrientLandscapeLeft,
  OrientLandscapeRight,
};

namespace Imu {
bool available();
bool readAccel(Vec3& a);  // units of g
bool readGyro(Vec3& g);   // degrees/sec
bool pitchRollDeg(float& pitch, float& roll);
Orientation orientation();
bool isFaceDown();
// True if total acceleration magnitude exceeds thresholdG (shake gesture).
bool isShaken(float thresholdG = 2.2f);
}  // namespace Imu

// ------------------------------------------------------------------ Audio ---
// Implemented in Phase 4 (ESP-IDF i2s_std + ES8311 register init). Declared
// here so apps and the launcher can link against the stable API now.
namespace Audio {
bool available();
bool begin(uint32_t sampleRate = 16000);
void end();
void playTone(uint16_t freqHz, uint16_t ms);
void beep();
bool playWavFile(const char* path);
bool startRecordWav(const char* path, uint32_t sampleRate = 16000);
void stopRecord();
bool isRecording();
}  // namespace Audio

// --------------------------------------------------------------- Chrome ---
// Draws a battery glyph + "HH:MM" into the top-right of the header band.
// Reads Battery/Rtc directly; draws nothing for any unavailable subsystem.
void drawStatusChrome(Adafruit_GFX& gfx, int16_t screenW, int16_t headerH = 42);

}  // namespace Sensors
}  // namespace WaveshareAmoled
