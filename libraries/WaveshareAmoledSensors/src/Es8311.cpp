#include "Es8311.h"

namespace Es8311 {

namespace {

// Register map (subset used here) — see es8311 datasheet / esp-adf driver.
constexpr uint8_t REG00 = 0x00;  // reset
constexpr uint8_t REG01 = 0x01;  // clk manager: mclk src + clock enables
constexpr uint8_t REG02 = 0x02;  // clk divider / multiplier
constexpr uint8_t REG03 = 0x03;  // adc fsmode + osr
constexpr uint8_t REG04 = 0x04;  // dac osr
constexpr uint8_t REG05 = 0x05;  // adc/dac clk divider
constexpr uint8_t REG06 = 0x06;  // bclk inverter + divider
constexpr uint8_t REG07 = 0x07;  // lrck divider high
constexpr uint8_t REG08 = 0x08;  // lrck divider low
constexpr uint8_t REG09 = 0x09;  // dac serial digital port
constexpr uint8_t REG0A = 0x0A;  // adc serial digital port
constexpr uint8_t REG0B = 0x0B;
constexpr uint8_t REG0C = 0x0C;
constexpr uint8_t REG0D = 0x0D;  // power up/down
constexpr uint8_t REG0E = 0x0E;  // power up/down
constexpr uint8_t REG10 = 0x10;
constexpr uint8_t REG11 = 0x11;
constexpr uint8_t REG12 = 0x12;  // enable DAC
constexpr uint8_t REG13 = 0x13;
constexpr uint8_t REG14 = 0x14;  // DMIC select + analog PGA gain
constexpr uint8_t REG15 = 0x15;  // ADC ramp / dmic sense
constexpr uint8_t REG16 = 0x16;  // ADC
constexpr uint8_t REG17 = 0x17;  // ADC volume
constexpr uint8_t REG1B = 0x1B;
constexpr uint8_t REG1C = 0x1C;
constexpr uint8_t REG31 = 0x31;  // DAC mute
constexpr uint8_t REG32 = 0x32;  // DAC volume
constexpr uint8_t REG37 = 0x37;  // DAC ramprate
constexpr uint8_t REG44 = 0x44;  // GPIO dac2adc
constexpr uint8_t REG45 = 0x45;  // GP control

// Clock coefficients for MCLK = 256 * rate (FROM_MCLK_PIN). Subset of the
// esp-adf coeff_div table covering the rates we use.
struct Coeff {
  uint32_t mclk;
  uint32_t rate;
  uint8_t pre_div;
  uint8_t pre_multi;
  uint8_t adc_div;
  uint8_t dac_div;
  uint8_t fs_mode;
  uint8_t lrck_h;
  uint8_t lrck_l;
  uint8_t bclk_div;
  uint8_t adc_osr;
  uint8_t dac_osr;
};

constexpr Coeff kCoeffs[] = {
    {4096000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2048000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {5644800, 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {11289600, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
};

TwoWire* g_wire = &Wire;
uint8_t g_addr = 0x18;

bool writeReg(uint8_t reg, uint8_t val) {
  g_wire->beginTransmission(g_addr);
  g_wire->write(reg);
  g_wire->write(val);
  return g_wire->endTransmission() == 0;
}

uint8_t readReg(uint8_t reg) {
  g_wire->beginTransmission(g_addr);
  g_wire->write(reg);
  if (g_wire->endTransmission(false) != 0) return 0;
  if (g_wire->requestFrom(static_cast<int>(g_addr), 1) < 1) return 0;
  return g_wire->read();
}

const Coeff* findCoeff(uint32_t rate) {
  const uint32_t mclk = rate * 256;
  for (const Coeff& c : kCoeffs) {
    if (c.rate == rate && c.mclk == mclk) return &c;
  }
  return nullptr;
}

void configSample(const Coeff& c) {
  uint8_t regv = readReg(REG02) & 0x07;
  regv |= (c.pre_div - 1) << 5;
  uint8_t datmp = 0;
  switch (c.pre_multi) {
    case 2: datmp = 1; break;
    case 4: datmp = 2; break;
    case 8: datmp = 3; break;
    default: datmp = 0; break;
  }
  regv |= datmp << 3;
  writeReg(REG02, regv);

  regv = 0;
  regv |= (c.adc_div - 1) << 4;
  regv |= (c.dac_div - 1) << 0;
  writeReg(REG05, regv);

  regv = readReg(REG03) & 0x80;
  regv |= c.fs_mode << 6;
  regv |= c.adc_osr << 0;
  writeReg(REG03, regv);

  regv = readReg(REG04) & 0x80;
  regv |= c.dac_osr << 0;
  writeReg(REG04, regv);

  regv = readReg(REG07) & 0xC0;
  regv |= c.lrck_h << 0;
  writeReg(REG07, regv);

  regv = 0;
  regv |= c.lrck_l << 0;
  writeReg(REG08, regv);

  regv = readReg(REG06) & 0xE0;
  if (c.bclk_div < 19) {
    regv |= (c.bclk_div - 1) << 0;
  } else {
    regv |= c.bclk_div << 0;
  }
  writeReg(REG06, regv);
}

}  // namespace

bool init(TwoWire& wire, uint8_t addr, uint32_t sampleRate) {
  g_wire = &wire;
  g_addr = addr;

  // Probe: does the codec ACK?
  g_wire->beginTransmission(g_addr);
  if (g_wire->endTransmission() != 0) return false;

  const Coeff* c = findCoeff(sampleRate);
  if (!c) c = findCoeff(16000);  // fall back to a known-good rate

  // I2C noise immunity (written twice per esp-adf note).
  writeReg(REG44, 0x08);
  writeReg(REG44, 0x08);

  writeReg(REG01, 0x30);
  writeReg(REG02, 0x00);
  writeReg(REG03, 0x10);
  writeReg(REG16, 0x24);
  writeReg(REG04, 0x10);
  writeReg(REG05, 0x00);
  writeReg(REG0B, 0x00);
  writeReg(REG0C, 0x00);
  writeReg(REG10, 0x1F);
  writeReg(REG11, 0x7F);
  writeReg(REG00, 0x80);  // reset, then slave mode (bit6 = 0)

  uint8_t regv = readReg(REG00);
  regv &= 0xBF;  // slave mode
  writeReg(REG00, regv);

  writeReg(REG01, 0x3F);
  // MCLK from the dedicated MCLK pin (FROM_MCLK_PIN): clear bit7.
  regv = readReg(REG01) & 0x7F;
  writeReg(REG01, regv);

  if (c) configSample(*c);

  // MCLK / SCLK not inverted.
  regv = readReg(REG01) & ~0x40;
  writeReg(REG01, regv);
  regv = readReg(REG06) & ~0x20;
  writeReg(REG06, regv);

  writeReg(REG13, 0x10);
  writeReg(REG1B, 0x0A);
  writeReg(REG1C, 0x6A);

  // ---- start ADC + DAC (es8311_start, ES_MODULE_ADC_DAC) ----
  uint8_t dac_iface = readReg(REG09) & 0xBF;
  uint8_t adc_iface = readReg(REG0A) & 0xBF;
  // both directions active (clear bit6 on each)
  writeReg(REG09, dac_iface);
  writeReg(REG0A, adc_iface);

  writeReg(REG17, 0xBF);
  writeReg(REG0E, 0x02);
  writeReg(REG12, 0x00);
  writeReg(REG14, 0x1A);  // analog mic, default PGA

  // Analog microphone (not DMIC): clear bit6 of REG14.
  regv = readReg(REG14) & ~0x40;
  writeReg(REG14, regv);

  writeReg(REG0D, 0x01);
  writeReg(REG15, 0x40);
  writeReg(REG37, 0x08);
  writeReg(REG45, 0x00);
  writeReg(REG44, 0x58);  // internal reference (ADCL + DACR)

  setVolume(70);
  return true;
}

void setVolume(uint8_t percent) {
  if (percent > 100) percent = 100;
  // 0xBF == 0 dB; scale linearly up to it.
  const uint8_t reg = static_cast<uint8_t>((static_cast<uint16_t>(percent) * 0xBF) / 100);
  writeReg(REG32, reg);
}

void setMicGain(uint8_t gain) {
  if (gain > 7) gain = 7;
  writeReg(REG16, gain);
}

void mute(bool muted) {
  uint8_t regv = readReg(REG31) & 0x9F;
  writeReg(REG31, muted ? (regv | 0x60) : regv);
}

}  // namespace Es8311
