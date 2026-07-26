#pragma once

#include <Arduino.h>

namespace WaveshareAmoledAudio {

// A small asynchronous player for SD_MMC WAV assets. It accepts standard RIFF
// PCM, 16-bit mono or stereo files at 8, 16, 22.05, 32, 44.1, or 48 kHz.
// Call update() from loop so a finished worker is reaped safely.
bool begin();
bool ready();
bool playWav(const char* path);
bool requestPlayWav(const char* path);
// Keeps this WAV looping with a short task-boundary gap. One-shot requests
// temporarily interrupt the loop, then it resumes automatically.
bool loopWav(const char* path);
void stop();
void update();
bool isPlaying();
const char* playingPath();
void setVolume(uint8_t percent);
uint8_t volume();
bool muted();

}  // namespace WaveshareAmoledAudio
