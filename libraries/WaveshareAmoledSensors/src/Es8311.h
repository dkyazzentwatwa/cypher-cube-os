#pragma once

// Minimal self-contained ES8311 codec control over I2C (Wire).
//
// Ported from Espressif's esp-adf ES8311 driver
// (components/audio_hal/driver/es8311) — same register map and clock
// coefficient table — but stripped of the esp-adf i2c_bus/board abstractions so
// it talks directly to an Arduino TwoWire bus.
//
// Assumes the standard board wiring for this AMOLED module: MCLK is supplied by
// the ESP32 I2S peripheral (FROM_MCLK_PIN), MCLK = 256 * sampleRate, codec in
// I2S slave mode, 16-bit samples, onboard analog microphone (not DMIC).

#include <Arduino.h>
#include <Wire.h>

namespace Es8311 {

// Probes and initializes the codec for full-duplex 16-bit audio at sampleRate.
// Returns false if the chip does not ACK on I2C (e.g. wrong board).
bool init(TwoWire& wire, uint8_t addr, uint32_t sampleRate);

// DAC (speaker) volume, 0..100. 0xBF register value == 0 dB.
void setVolume(uint8_t percent);

// ADC (microphone) PGA gain, 0..7 (maps to ES8311 REG16 0x00..0x07 range).
void setMicGain(uint8_t gain);

void mute(bool muted);

}  // namespace Es8311
