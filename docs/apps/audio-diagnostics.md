# Audio Diagnostics

Status: `ready`

Source route: local Cypher Cube diagnostic sketch and the shared `WaveshareAmoledAudio` playback library.

## Cypher Cube Controls

- Put a 16-bit PCM WAV file at `/waveshare-os/audio/test.wav` on the microSD card.
- Tap `Play test.wav` to start or restart the clip, `Stop` to request a stop, and `Vol +10` to raise output volume.
- Serial fallback: `play`, `stop`, `volume 0-100`, and `home`.
- Hold BOOT to return to the launcher.

## Accepted Audio

The current player accepts standard RIFF PCM WAV files, 16-bit mono or stereo, at 8, 16, 22.05, 32, 44.1, or 48 kHz. It reads directly from SD_MMC and keeps the app loop responsive while playback runs in a FreeRTOS task.

## Proof State

`compile-ready` once built. Actual speaker output, useful volume, clip transitions, and SD-card behavior remain on-device checks.
