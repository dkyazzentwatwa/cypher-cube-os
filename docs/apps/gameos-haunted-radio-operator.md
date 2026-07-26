# Haunted Radio Operator

Status: `prototype`

Source route: standalone Waveshare touch-and-audio game. It replaces the earlier four-choice campaign wrapper for this title.

## Game Loop

Tune the fictional FM band, find a station whose carrier is clear enough, then tap `STABILIZE` repeatedly before the signal decays. A completed capture unlocks its logbook entry and persists the night-shift progress to SD.

## Cypher Cube Controls

- Tap the frequency track for fine tuning.
- Swipe up/down to move the dial by 0.5 MHz.
- Tap `SCAN` to jump to the next signal, then tap `STABILIZE` to capture it.
- Swipe left for the archive and right to return to the radio.
- Serial fallback: `tune 875-1080`, `scan`, `stabilize`, `archive`, `radio`, `status`, `reset`, and `home`.

## SD Assets And Saves

- Save state: `/waveshare-os/games/haunted-radio/state.txt`
- Optional 16-bit PCM WAV files: `background.wav`, `static.wav`, and `signal-01.wav` through `signal-04.wav` under `/waveshare-os/audio/haunted-radio/`
- WAV playback accepts mono/stereo 8, 16, 22.05, 32, 44.1, or 48 kHz assets.

Missing clips do not break gameplay. When present, `background.wav` loops during play and the one-shot clips temporarily interrupt it, then it resumes. All clips are played directly from SD through the shared ES8311 audio layer.

## Proof State

`compile-ready` after the app build completes. Touch feel, speaker level, WAV compatibility, SD state persistence, and launcher installation require physical-cube verification.
