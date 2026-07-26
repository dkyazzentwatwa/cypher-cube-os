#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace WaveshareAmoledAudio {
namespace Es8311 {

// Initializes the onboard ES8311 as a 16-bit I2S slave. The supported rates
// match the codec's MCLK = 256 * sample-rate clock table.
bool begin(TwoWire& wire, uint32_t sampleRate);

// 0 mutes. 1..80 is the clean -40..0 dB range; 81..100 is a deliberate
// digital boost for quiet material.
void setVolume(uint8_t percent);
void mute(bool muted);

}  // namespace Es8311
}  // namespace WaveshareAmoledAudio
