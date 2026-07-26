#include "WaveshareAmoledAudio.h"

#include <ESP_I2S.h>
#include <FS.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string.h>

#include "Es8311.h"

namespace WaveshareAmoledAudio {
namespace {

constexpr int kPinI2sMclk = 16;
constexpr int kPinI2sBclk = 9;
constexpr int kPinI2sWs = 45;
constexpr int kPinI2sDataOut = 8;
constexpr int kPinI2sDataIn = 10;
constexpr int kPinAmpEnable = 46;
constexpr uint32_t kDefaultRate = 16000;
constexpr uint32_t kAmpHoldMs = 750;
constexpr uint32_t kDriverIdleMs = 10000;
constexpr const char* kPreferencesNamespace = "wamoledos";
constexpr const char* kVolumeKey = "soundVol";
constexpr const char* kMuteKey = "soundMute";

I2SClass i2s;
SemaphoreHandle_t playDone = nullptr;
bool initialized = false;
bool codecReady = false;
bool driverStarted = false;
bool ampOn = false;
bool playing = false;  // main loop only, cleared only after the worker exits
volatile bool stopRequested = false;
uint32_t activeRate = kDefaultRate;
uint32_t idleSince = 0;
uint8_t outputVolume = 70;
bool outputMuted = false;
char activePath[128] = "";
char queuedPath[128] = "";
bool queued = false;
char loopPath[128] = "";
bool looping = false;
uint16_t activeChannels = 0;
uint32_t activeDataOffset = 0;
uint32_t activeDataBytes = 0;

uint16_t readLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

bool supportedRate(uint32_t sampleRate) {
  switch (sampleRate) {
    case 8000:
    case 16000:
    case 22050:
    case 32000:
    case 44100:
    case 48000:
      return true;
    default:
      return false;
  }
}

bool parseWav(const char* path, uint32_t& sampleRate, uint16_t& channels,
              uint32_t& dataOffset, uint32_t& dataBytes) {
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    Serial.printf("[audio] open failed: %s\n", path ? path : "(null)");
    return false;
  }

  uint8_t riff[12];
  if (file.read(riff, sizeof(riff)) != sizeof(riff) || memcmp(riff, "RIFF", 4) != 0 ||
      memcmp(riff + 8, "WAVE", 4) != 0) {
    Serial.printf("[audio] unsupported WAV header: %s\n", path);
    file.close();
    return false;
  }

  bool fmtFound = false;
  bool dataFound = false;
  uint16_t format = 0;
  uint16_t bitsPerSample = 0;
  while (file.available() && (!fmtFound || !dataFound)) {
    uint8_t chunk[8];
    if (file.read(chunk, sizeof(chunk)) != sizeof(chunk)) break;
    const uint32_t chunkBytes = readLe32(chunk + 4);
    const uint32_t chunkStart = file.position();
    if (memcmp(chunk, "fmt ", 4) == 0 && chunkBytes >= 16) {
      uint8_t fmt[16];
      if (file.read(fmt, sizeof(fmt)) != sizeof(fmt)) break;
      format = readLe16(fmt);
      channels = readLe16(fmt + 2);
      sampleRate = readLe32(fmt + 4);
      bitsPerSample = readLe16(fmt + 14);
      fmtFound = true;
    } else if (memcmp(chunk, "data", 4) == 0) {
      dataOffset = chunkStart;
      dataBytes = chunkBytes;
      dataFound = true;
    }
    const uint32_t next = chunkStart + chunkBytes + (chunkBytes & 1U);
    if (!file.seek(next)) break;
  }
  file.close();

  if (!fmtFound || !dataFound || format != 1 || bitsPerSample != 16 ||
      (channels != 1 && channels != 2) || !supportedRate(sampleRate)) {
    Serial.printf("[audio] WAV must be PCM16 mono/stereo at a supported rate: %s\n", path);
    return false;
  }
  return true;
}

void loadDeviceSoundSettings() {
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, true)) return;
  outputVolume = preferences.getUChar(kVolumeKey, 70);
  if (outputVolume > 100) outputVolume = 70;
  outputMuted = preferences.getBool(kMuteKey, false);
  preferences.end();
}

void setAmp(bool on) {
  if (on == ampOn) return;
  if (!on && codecReady) {
    Es8311::mute(true);
    delay(5);
  }
  digitalWrite(kPinAmpEnable, on ? HIGH : LOW);
  ampOn = on;
  if (on && codecReady) Es8311::mute(outputMuted);
}

bool ensureDriver(uint32_t sampleRate) {
  if (driverStarted && activeRate == sampleRate && codecReady) return true;
  if (driverStarted) {
    i2s.end();
    driverStarted = false;
    codecReady = false;
  }
  i2s.setPins(kPinI2sBclk, kPinI2sWs, kPinI2sDataOut, kPinI2sDataIn, kPinI2sMclk);
  if (!i2s.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("[audio] I2S begin failed");
    return false;
  }
  if (!Es8311::begin(Wire, sampleRate)) {
    Serial.println("[audio] ES8311 not found");
    i2s.end();
    return false;
  }
  activeRate = sampleRate;
  driverStarted = true;
  codecReady = true;
  Es8311::setVolume(outputVolume);
  Es8311::mute(outputMuted);
  Serial.printf("[audio] ES8311 ready rate=%u\n", static_cast<unsigned>(sampleRate));
  return true;
}

void playTask(void*) {
  File file = SD_MMC.open(activePath, FILE_READ);
  if (file && file.seek(activeDataOffset)) {
    uint32_t remaining = activeDataBytes;
    uint8_t input[1024];
    int16_t stereo[512];
    while (!stopRequested && remaining > 0) {
      size_t wanted = remaining < sizeof(input) ? remaining : sizeof(input);
      const size_t frameBytes = activeChannels * sizeof(int16_t);
      wanted -= wanted % frameBytes;
      if (wanted == 0) break;
      const size_t read = file.read(input, wanted);
      if (read == 0) break;
      remaining -= read;
      if (activeChannels == 2) {
        i2s.write(input, read);
      } else {
        const int16_t* mono = reinterpret_cast<const int16_t*>(input);
        const size_t frames = read / sizeof(int16_t);
        for (size_t i = 0; i < frames; ++i) {
          stereo[2 * i] = mono[i];
          stereo[2 * i + 1] = mono[i];
        }
        i2s.write(reinterpret_cast<const uint8_t*>(stereo), frames * 2 * sizeof(int16_t));
      }
    }
    file.close();
  }
  xSemaphoreGive(playDone);
  vTaskDelete(nullptr);
}

bool start(const char* path) {
  if (!path || playing) return false;
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  uint32_t dataOffset = 0;
  uint32_t dataBytes = 0;
  if (!parseWav(path, sampleRate, channels, dataOffset, dataBytes) ||
      !ensureDriver(sampleRate)) {
    return false;
  }
  strncpy(activePath, path, sizeof(activePath) - 1);
  activePath[sizeof(activePath) - 1] = '\0';
  activeChannels = channels;
  activeDataOffset = dataOffset;
  activeDataBytes = dataBytes;
  stopRequested = false;
  xSemaphoreTake(playDone, 0);
  playing = true;
  setAmp(true);
  if (xTaskCreate(playTask, "wavplay", 6144, nullptr, 1, nullptr) != pdPASS) {
    playing = false;
    Serial.println("[audio] playback task creation failed");
    return false;
  }
  return true;
}

}  // namespace

bool begin() {
  if (initialized) return playDone != nullptr;
  loadDeviceSoundSettings();
  pinMode(kPinAmpEnable, OUTPUT);
  digitalWrite(kPinAmpEnable, LOW);
  playDone = xSemaphoreCreateBinary();
  initialized = playDone != nullptr;
  return initialized;
}

bool ready() {
  return codecReady;
}

bool playWav(const char* path) {
  if (!begin()) return false;
  looping = false;
  queued = false;
  return start(path);
}

bool requestPlayWav(const char* path) {
  if (!path || !begin()) return false;
  if (!playing) return start(path);
  strncpy(queuedPath, path, sizeof(queuedPath) - 1);
  queuedPath[sizeof(queuedPath) - 1] = '\0';
  queued = true;
  stopRequested = true;
  return true;
}

bool loopWav(const char* path) {
  if (!path || !begin()) return false;
  strncpy(loopPath, path, sizeof(loopPath) - 1);
  loopPath[sizeof(loopPath) - 1] = '\0';
  looping = true;
  return requestPlayWav(loopPath);
}

void stop() {
  looping = false;
  queued = false;
  stopRequested = true;
}

void update() {
  if (!initialized) return;
  if (playing && xSemaphoreTake(playDone, 0) == pdTRUE) {
    playing = false;
    idleSince = millis();
  }
  if (queued && !playing) {
    queued = false;
    start(queuedPath);
  }
  if (looping && !playing && !queued && !start(loopPath)) {
    // A removed/unreadable loop asset should fail once, not hammer SD and
    // serial on every frame until the app is restarted.
    looping = false;
  }
  if (!playing && ampOn && millis() - idleSince >= kAmpHoldMs) setAmp(false);
  if (!playing && driverStarted && millis() - idleSince >= kDriverIdleMs) {
    i2s.end();
    driverStarted = false;
    codecReady = false;
  }
}

bool isPlaying() {
  return playing;
}

const char* playingPath() {
  return activePath;
}

void setVolume(uint8_t percent) {
  outputVolume = percent > 100 ? 100 : percent;
  if (codecReady) Es8311::setVolume(outputVolume);
}

uint8_t volume() {
  return outputVolume;
}

bool muted() {
  return outputMuted;
}

}  // namespace WaveshareAmoledAudio
