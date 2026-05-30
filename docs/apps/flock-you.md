# Flock You

Status: `ready`

Source route: direct Cypher Cube AMOLED profile build from sibling `flock-you`.

## Cypher Cube Controls

- Touch and BOOT behavior follow the upstream AMOLED profile.
- Swipe pages and use the on-screen detector UI.
- Serial fallback remains available at `115200`.

## SD Data

The app uses its own storage behavior for logs and status. The launcher packages
only `flock-you.bin`.

## Port Notes

Built with `BOARD_PROFILE=ESP32_WAVESHARE_AMOLED_18`. `starbeam_v2` remains
excluded from this catalog.
