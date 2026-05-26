# Cardputer Games

Status: `lite`

Source route: native Waveshare touch arcade shelf in this repo.

## Waveshare Controls

- Tap `Target Tap`, `Swipe Drill`, `Reaction`, or `Memory Grid`.
- In games, tap targets, swipe as prompted, react on green, or repeat the grid
  pattern.
- Result screens offer `Replay` and `Menu`.
- Short BOOT returns to the game menu; hold BOOT to return to the launcher.
- Serial fallback: `target`, `swipe`, `reaction`, `memory`, `scores`, `menu`,
  `home`.

## SD Data

Scores append to `/waveshare-os/cardputer-games/scores.txt`. The menu shows
best scores and result screens show recent runs.

## Port Notes

This is a touch-native arcade replacement for the keyboard-heavy Cardputer
games launcher surface, with timed rounds, streaks, reaction timing, memory
patterns, and persistent score history.
