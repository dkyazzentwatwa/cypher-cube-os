#include "WaveshareAmoledSensors.h"

#include <math.h>

#include <ESP_I2S.h>
#include <FS.h>
#include <SD_MMC.h>

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>

#include "Es8311.h"

namespace WaveshareAmoled {
namespace Sensors {

namespace {

TwoWire* g_wire = &Wire;

// Each subsystem is brought up independently; a failure leaves its flag false.
XPowersPMU g_pmu;
bool g_batteryReady = false;

SensorPCF85063 g_rtc;
bool g_rtcReady = false;

SensorQMI8658 g_imu;
bool g_imuReady = false;

void initBattery() {
  g_batteryReady = g_pmu.begin(*g_wire, kAddrAxp2101, kPinSda, kPinScl);
  if (!g_batteryReady) {
    Serial.println("[sensors] AXP2101 not found");
    return;
  }
  g_pmu.enableBattDetection();
  g_pmu.enableBattVoltageMeasure();
  g_pmu.enableVbusVoltageMeasure();
  g_pmu.enableSystemVoltageMeasure();
  Serial.printf("[sensors] AXP2101 ok id=0x%X\n",
                static_cast<unsigned>(g_pmu.getChipID()));
}

void initRtc() {
  g_rtcReady = g_rtc.begin(*g_wire, kPinSda, kPinScl);
  Serial.printf("[sensors] PCF85063 %s\n", g_rtcReady ? "ok" : "not found");
}

void initImu() {
  g_imuReady = g_imu.begin(*g_wire, QMI8658_L_SLAVE_ADDRESS, kPinSda, kPinScl);
  if (!g_imuReady) {
    Serial.println("[sensors] QMI8658 not found");
    return;
  }
  g_imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                            SensorQMI8658::ACC_ODR_125Hz,
                            SensorQMI8658::LPF_MODE_0);
  g_imu.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS,
                        SensorQMI8658::GYR_ODR_112_1Hz,
                        SensorQMI8658::LPF_MODE_0);
  g_imu.enableAccelerometer();
  g_imu.enableGyroscope();
  Serial.println("[sensors] QMI8658 ok");
}

// ---- Audio state (ES8311 + I2S) ----
I2SClass g_i2s;
bool g_audioReady = false;
bool g_audioStarted = false;
uint32_t g_audioRate = 16000;
volatile bool g_recording = false;
TaskHandle_t g_recTask = nullptr;
File g_recFile;
volatile uint32_t g_recBytes = 0;

void writeWavHeader(File& f, uint32_t sampleRate, uint16_t channels,
                    uint16_t bits, uint32_t dataBytes) {
  const uint32_t byteRate = sampleRate * channels * bits / 8;
  const uint16_t blockAlign = channels * bits / 8;
  const uint32_t chunkSize = 36 + dataBytes;
  const uint32_t sub1 = 16;
  const uint16_t fmt = 1;
  f.seek(0);
  f.write(reinterpret_cast<const uint8_t*>("RIFF"), 4);
  f.write(reinterpret_cast<const uint8_t*>(&chunkSize), 4);
  f.write(reinterpret_cast<const uint8_t*>("WAVE"), 4);
  f.write(reinterpret_cast<const uint8_t*>("fmt "), 4);
  f.write(reinterpret_cast<const uint8_t*>(&sub1), 4);
  f.write(reinterpret_cast<const uint8_t*>(&fmt), 2);
  f.write(reinterpret_cast<const uint8_t*>(&channels), 2);
  f.write(reinterpret_cast<const uint8_t*>(&sampleRate), 4);
  f.write(reinterpret_cast<const uint8_t*>(&byteRate), 4);
  f.write(reinterpret_cast<const uint8_t*>(&blockAlign), 2);
  f.write(reinterpret_cast<const uint8_t*>(&bits), 2);
  f.write(reinterpret_cast<const uint8_t*>("data"), 4);
  f.write(reinterpret_cast<const uint8_t*>(&dataBytes), 4);
}

void recordTaskFn(void*) {
  uint8_t buf[1024];
  while (g_recording) {
    const size_t n = g_i2s.readBytes(reinterpret_cast<char*>(buf), sizeof(buf));
    if (n > 0) {
      g_recFile.write(buf, n);
      g_recBytes += n;
    } else {
      vTaskDelay(1);
    }
  }
  writeWavHeader(g_recFile, g_audioRate, 1, 16, g_recBytes);
  g_recFile.flush();
  g_recFile.close();
  g_recTask = nullptr;
  vTaskDelete(nullptr);
}

}  // namespace

void begin(TwoWire& wire) {
  g_wire = &wire;
  // Idempotent: the display/touch stack has already brought Wire up, but calling
  // again with the shared pins is harmless and covers init-order surprises.
  g_wire->begin(kPinSda, kPinScl);
  initBattery();
  initRtc();
  initImu();
}

// ---------------------------------------------------------------- Battery ---
namespace Battery {

bool available() { return g_batteryReady; }

int percent() {
  if (!g_batteryReady || !g_pmu.isBatteryConnect()) return -1;
  return g_pmu.getBatteryPercent();
}

uint16_t voltageMv() {
  if (!g_batteryReady) return 0;
  return g_pmu.getBattVoltage();
}

bool isCharging() { return g_batteryReady && g_pmu.isCharging(); }
bool isVbusPresent() { return g_batteryReady && g_pmu.isVbusIn(); }
bool isPresent() { return g_batteryReady && g_pmu.isBatteryConnect(); }

}  // namespace Battery

// -------------------------------------------------------------------- RTC ---
namespace Rtc {

bool available() { return g_rtcReady; }

DateTime now() {
  DateTime out;
  if (!g_rtcReady) return out;
  RTC_DateTime dt = g_rtc.getDateTime();
  out.year = dt.getYear();
  out.month = dt.getMonth();
  out.day = dt.getDay();
  out.hour = dt.getHour();
  out.minute = dt.getMinute();
  out.second = dt.getSecond();
  out.valid = true;
  return out;
}

void set(const DateTime& dt) {
  if (!g_rtcReady) return;
  g_rtc.setDateTime(
      RTC_DateTime(dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second));
}

bool formatHhMm(char* out, size_t len) {
  if (!g_rtcReady || !out || len < 6) return false;
  DateTime dt = now();
  snprintf(out, len, "%02u:%02u", dt.hour, dt.minute);
  return true;
}

}  // namespace Rtc

// -------------------------------------------------------------------- IMU ---
namespace Imu {

bool available() { return g_imuReady; }

bool readAccel(Vec3& a) {
  if (!g_imuReady) return false;
  return g_imu.getAccelerometer(a.x, a.y, a.z);
}

bool readGyro(Vec3& g) {
  if (!g_imuReady) return false;
  return g_imu.getGyroscope(g.x, g.y, g.z);
}

bool pitchRollDeg(float& pitch, float& roll) {
  Vec3 a;
  if (!readAccel(a)) return false;
  // Standard accelerometer tilt: pitch about X, roll about Y.
  pitch = atan2f(-a.x, sqrtf(a.y * a.y + a.z * a.z)) * 180.0f / PI;
  roll = atan2f(a.y, a.z) * 180.0f / PI;
  return true;
}

Orientation orientation() {
  Vec3 a;
  if (!readAccel(a)) return OrientUnknown;
  // Z dominant -> face up/down.
  if (a.z > 0.7f) return OrientFaceUp;
  if (a.z < -0.7f) return OrientFaceDown;
  // Otherwise pick the dominant in-plane axis.
  if (fabsf(a.y) >= fabsf(a.x)) {
    return a.y >= 0 ? OrientPortrait : OrientPortraitInverted;
  }
  return a.x >= 0 ? OrientLandscapeRight : OrientLandscapeLeft;
}

bool isFaceDown() { return orientation() == OrientFaceDown; }

bool isShaken(float thresholdG) {
  Vec3 a;
  if (!readAccel(a)) return false;
  float mag = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
  return mag > thresholdG;
}

}  // namespace Imu

// ------------------------------------------------------------------ Audio ---
namespace Audio {

bool available() { return g_audioReady; }

bool begin(uint32_t sampleRate) {
  if (g_audioStarted && g_audioRate == sampleRate) return g_audioReady;
  if (g_audioStarted) end();
  g_audioRate = sampleRate;

  if (!Es8311::init(*g_wire, kAddrEs8311, sampleRate)) {
    Serial.println("[sensors] ES8311 not found");
    g_audioReady = false;
    return false;
  }
  pinMode(kPinPaEnable, OUTPUT);
  digitalWrite(kPinPaEnable, HIGH);

  g_i2s.setPins(kPinI2sBck, kPinI2sWs, kPinI2sDout, kPinI2sDin, kPinI2sMck);
  if (!g_i2s.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_MONO)) {
    Serial.println("[sensors] I2S begin failed");
    g_audioReady = false;
    return false;
  }
  g_i2s.configureRX(sampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  g_audioStarted = true;
  g_audioReady = true;
  Serial.printf("[sensors] ES8311 audio ok @ %uHz\n",
                static_cast<unsigned>(sampleRate));
  return true;
}

void end() {
  if (g_recording) stopRecord();
  if (g_audioStarted) {
    g_i2s.end();
    g_audioStarted = false;
  }
  digitalWrite(kPinPaEnable, LOW);
  g_audioReady = false;
}

void playTone(uint16_t freqHz, uint16_t ms) {
  if (!g_audioReady && !begin(g_audioRate)) return;
  if (freqHz == 0 || ms == 0) return;
  const uint32_t rate = g_audioRate;
  const size_t total = static_cast<uint32_t>(rate) * ms / 1000;
  const float step = 2.0f * PI * freqHz / rate;
  int16_t buf[256];
  float phase = 0;
  size_t done = 0;
  while (done < total) {
    size_t chunk = total - done;
    if (chunk > 256) chunk = 256;
    for (size_t i = 0; i < chunk; i++) {
      buf[i] = static_cast<int16_t>(sinf(phase) * 8000.0f);
      phase += step;
      if (phase > 2.0f * PI) phase -= 2.0f * PI;
    }
    g_i2s.write(reinterpret_cast<uint8_t*>(buf), chunk * 2);
    done += chunk;
  }
}

void beep() { playTone(2000, 40); }

bool playWavFile(const char* path) {
  if (!g_audioReady && !begin(g_audioRate)) return false;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return false;
  uint8_t hdr[44];
  if (f.read(hdr, 44) != 44) {
    f.close();
    return false;
  }
  const uint32_t rate = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | (hdr[27] << 24);
  const uint16_t channels = hdr[22] | (hdr[23] << 8);
  const bool reconfig = rate > 0 && rate != g_audioRate;
  if (reconfig) {
    g_i2s.configureTX(rate, I2S_DATA_BIT_WIDTH_16BIT,
                      channels >= 2 ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO);
  }
  uint8_t buf[1024];
  size_t n;
  while ((n = f.read(buf, sizeof(buf))) > 0) {
    g_i2s.write(buf, n);
  }
  f.close();
  if (reconfig) {
    g_i2s.configureTX(g_audioRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  }
  return true;
}

bool startRecordWav(const char* path, uint32_t sampleRate) {
  if (g_recording) return false;
  if (!begin(sampleRate)) return false;
  g_recFile = SD_MMC.open(path, FILE_WRITE);
  if (!g_recFile) return false;
  g_recBytes = 0;
  writeWavHeader(g_recFile, sampleRate, 1, 16, 0);  // placeholder, patched on stop
  g_recording = true;
  if (xTaskCreate(recordTaskFn, "wavrec", 4096, nullptr, 1, &g_recTask) != pdPASS) {
    g_recording = false;
    g_recFile.close();
    return false;
  }
  return true;
}

void stopRecord() {
  if (!g_recording) return;
  g_recording = false;
  while (g_recTask != nullptr) delay(5);  // task patches header + closes file
}

bool isRecording() { return g_recording; }

}  // namespace Audio

// --------------------------------------------------------------- Chrome ---
void drawStatusChrome(Adafruit_GFX& gfx, int16_t screenW, int16_t headerH) {
  constexpr int16_t kInset = 12;
  constexpr int16_t kBodyW = 26;
  constexpr int16_t kBodyH = 14;
  constexpr int16_t kNubW = 3;
  constexpr int16_t kNubH = 6;

  const int16_t bodyX = screenW - kInset - kNubW - kBodyW;
  const int16_t bodyY = (headerH - kBodyH) / 2;

  int pct = Battery::percent();
  const bool charging = Battery::isCharging();
  const bool vbus = Battery::isVbusPresent();

  if (Battery::available()) {
    uint16_t fillColor = 0x07E0;  // green
    if (charging) {
      fillColor = 0x07FF;  // accent (cyan) while charging
    } else if (pct >= 0 && pct <= 20) {
      fillColor = 0xF800;  // red when low
    } else if (pct >= 0 && pct <= 45) {
      fillColor = 0xFD20;  // amber mid-low
    }

    gfx.drawRect(bodyX, bodyY, kBodyW, kBodyH, 0xFFFF);
    gfx.fillRect(bodyX + kBodyW, bodyY + (kBodyH - kNubH) / 2, kNubW, kNubH,
                 0xFFFF);

    if (pct >= 0) {
      int16_t innerMax = kBodyW - 4;
      int16_t w = (innerMax * pct) / 100;
      if (w > 0) gfx.fillRect(bodyX + 2, bodyY + 2, w, kBodyH - 4, fillColor);
    } else if (vbus) {
      // No battery but on USB: fill solid to signal external power.
      gfx.fillRect(bodyX + 2, bodyY + 2, kBodyW - 4, kBodyH - 4, 0x07FF);
    }
  }

  // Clock to the left of the battery glyph.
  char hhmm[6];
  if (Rtc::formatHhMm(hhmm, sizeof(hhmm))) {
    gfx.setTextSize(2);
    gfx.setTextColor(0xFFFF);  // transparent background, drawn over fresh header
    int16_t textW = 5 * 12;    // 5 glyphs * 12px at size 2
    int16_t textX = bodyX - 10 - textW;
    if (textX < 4) textX = 4;
    gfx.setCursor(textX, (headerH - 16) / 2);
    gfx.print(hhmm);
  }
}

}  // namespace Sensors
}  // namespace WaveshareAmoled
