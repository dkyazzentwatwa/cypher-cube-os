#include "Es8311.h"

namespace WaveshareAmoledAudio {
namespace Es8311 {
namespace {

constexpr uint8_t kAddress = 0x18;
constexpr uint8_t kRegReset = 0x00;
constexpr uint8_t kRegClock = 0x01;
constexpr uint8_t kRegClockDivider = 0x02;
constexpr uint8_t kRegAdcOsr = 0x03;
constexpr uint8_t kRegDacOsr = 0x04;
constexpr uint8_t kRegAdcDacDivider = 0x05;
constexpr uint8_t kRegBclk = 0x06;
constexpr uint8_t kRegLrckHigh = 0x07;
constexpr uint8_t kRegLrckLow = 0x08;
constexpr uint8_t kRegDacSerial = 0x09;
constexpr uint8_t kRegAdcSerial = 0x0A;
constexpr uint8_t kRegPower0 = 0x0D;
constexpr uint8_t kRegPower1 = 0x0E;
constexpr uint8_t kRegDacEnable = 0x12;
constexpr uint8_t kRegDacRamp = 0x13;
constexpr uint8_t kRegAdcVolume = 0x17;
constexpr uint8_t kRegAnalogMic = 0x14;
constexpr uint8_t kRegAdcGain = 0x16;
constexpr uint8_t kRegAdcControl = 0x1C;
constexpr uint8_t kRegDacMute = 0x31;
constexpr uint8_t kRegDacVolume = 0x32;
constexpr uint8_t kRegDacRampRate = 0x37;

struct Coeff {
  uint32_t sampleRate;
  uint8_t preDiv;
  uint8_t preMulti;
  uint8_t adcDiv;
  uint8_t dacDiv;
  uint8_t fsMode;
  uint8_t lrckHigh;
  uint8_t lrckLow;
  uint8_t bclkDiv;
  uint8_t adcOsr;
  uint8_t dacOsr;
};

constexpr Coeff kCoeffs[] = {
    {8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
};

TwoWire* codecWire = &Wire;

bool writeReg(uint8_t reg, uint8_t value) {
  codecWire->beginTransmission(kAddress);
  codecWire->write(reg);
  codecWire->write(value);
  return codecWire->endTransmission() == 0;
}

uint8_t readReg(uint8_t reg) {
  codecWire->beginTransmission(kAddress);
  codecWire->write(reg);
  if (codecWire->endTransmission(false) != 0) return 0;
  if (codecWire->requestFrom(static_cast<int>(kAddress), 1) != 1) return 0;
  return static_cast<uint8_t>(codecWire->read());
}

const Coeff* findCoeff(uint32_t sampleRate) {
  for (const Coeff& coeff : kCoeffs) {
    if (coeff.sampleRate == sampleRate) return &coeff;
  }
  return nullptr;
}

void configureSampleRate(const Coeff& coeff) {
  uint8_t value = readReg(kRegClockDivider) & 0x07;
  value |= (coeff.preDiv - 1) << 5;
  value |= coeff.preMulti << 3;
  writeReg(kRegClockDivider, value);

  value = ((coeff.adcDiv - 1) << 4) | (coeff.dacDiv - 1);
  writeReg(kRegAdcDacDivider, value);

  value = (readReg(kRegAdcOsr) & 0x80) | (coeff.fsMode << 6) | coeff.adcOsr;
  writeReg(kRegAdcOsr, value);
  value = (readReg(kRegDacOsr) & 0x80) | coeff.dacOsr;
  writeReg(kRegDacOsr, value);
  writeReg(kRegLrckHigh, (readReg(kRegLrckHigh) & 0xC0) | coeff.lrckHigh);
  writeReg(kRegLrckLow, coeff.lrckLow);

  value = readReg(kRegBclk) & 0xE0;
  value |= coeff.bclkDiv < 19 ? (coeff.bclkDiv - 1) : coeff.bclkDiv;
  writeReg(kRegBclk, value);
}

}  // namespace

bool begin(TwoWire& wire, uint32_t sampleRate) {
  const Coeff* coeff = findCoeff(sampleRate);
  if (!coeff) return false;
  codecWire = &wire;
  codecWire->beginTransmission(kAddress);
  if (codecWire->endTransmission() != 0) return false;

  // This is Waveshare's ES8311 initialization sequence for the exact 1.8-in
  // AMOLED board: 16-bit I2S, onboard analog microphone path, codec slave.
  writeReg(kRegReset, 0x1F);
  writeReg(kRegReset, 0x00);
  writeReg(kRegReset, 0x80);
  writeReg(kRegReset, readReg(kRegReset) & 0xBF);
  writeReg(kRegClock, 0x3F);
  writeReg(kRegClock, readReg(kRegClock) & 0x7F);
  configureSampleRate(*coeff);
  writeReg(kRegClock, readReg(kRegClock) & ~0x40);
  writeReg(kRegBclk, readReg(kRegBclk) & ~0x20);
  writeReg(kRegDacSerial, 0x0C);
  writeReg(kRegAdcSerial, 0x0C);
  writeReg(kRegAdcVolume, 0xC8);
  writeReg(kRegPower0, 0x01);
  writeReg(kRegPower1, 0x02);
  writeReg(kRegDacEnable, 0x00);
  writeReg(kRegDacRamp, 0x10);
  writeReg(kRegAdcControl, 0x6A);
  writeReg(kRegDacRampRate, 0x08);
  writeReg(kRegAnalogMic, 0x1A);
  writeReg(kRegAdcGain, 0x07);
  setVolume(70);
  return true;
}

void setVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  if (percent == 0) {
    writeReg(kRegDacVolume, 0);
    return;
  }
  int16_t halfDb = percent <= 80
      ? static_cast<int16_t>(-80 + (static_cast<int32_t>(percent) * 80) / 80)
      : static_cast<int16_t>((static_cast<int32_t>(percent - 80) * 20) / 20);
  int16_t reg = 0xBF + halfDb;
  if (reg < 1) reg = 1;
  if (reg > 0xFF) reg = 0xFF;
  writeReg(kRegDacVolume, static_cast<uint8_t>(reg));
}

void mute(bool muted) {
  uint8_t value = readReg(kRegDacMute) & 0x9F;
  writeReg(kRegDacMute, muted ? static_cast<uint8_t>(value | 0x60) : value);
}

}  // namespace Es8311
}  // namespace WaveshareAmoledAudio
