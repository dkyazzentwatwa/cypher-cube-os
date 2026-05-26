#pragma once

#include "WaveshareAmoledReturn.h"

namespace CypherPuter {
inline void returnToLauncher(uint32_t delayMs = 250) {
  WaveshareAmoledOs::returnToLauncher(delayMs);
}
}  // namespace CypherPuter

namespace CypherOs {
inline void returnToLauncher(uint32_t delayMs = 250) {
  WaveshareAmoledOs::returnToLauncher(delayMs);
}
}  // namespace CypherOs

inline void cypherPuterReturnToLauncher(uint32_t delayMs = 250) {
  WaveshareAmoledOs::returnToLauncher(delayMs);
}

inline void cypherOsReturnToLauncher(uint32_t delayMs = 250) {
  WaveshareAmoledOs::returnToLauncher(delayMs);
}
