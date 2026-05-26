#pragma once

#include <Arduino.h>

namespace WaveshareAmoledPorts {

struct StoryGameAction {
  const char* label;
  const char* result;
  int8_t healthDelta;
  int8_t focusDelta;
  int8_t kitDelta;
  int8_t standingDelta;
  int8_t wealthDelta;
  uint8_t xpGain;
  uint8_t itemMask;
};

struct StoryGameProfile {
  const char* title;
  const char* slug;
  const char* role;
  const char* premise;
  const char* actNames[4];
  const char* questText[4];
  const char* lore[4];
  const char* statLabels[5];
  const char* inventory[4];
  StoryGameAction actions[4];
  uint16_t accent;
};

void beginStoryGame(const StoryGameProfile& profile);
void loopStoryGame();

}  // namespace WaveshareAmoledPorts
