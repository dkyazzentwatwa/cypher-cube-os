# Repository Guidelines

This repo is Arduino CLI only. Do not add PlatformIO, ESP-IDF project files,
online OTA catalogs, USB mass-storage flows, or multi-board support unless the
user explicitly changes scope.

## Commands

Build the launcher with the repo helper. It passes the local Waveshare display
and touch library paths explicitly, which is required on this Mac:

```bash
./tools/build-launcher.sh
```

The raw profile is still defined in `sketch.yaml` for reference:

```bash
arduino-cli compile --profile waveshare .
```

Flash with the local helper:

```bash
./tools/flash-launcher.sh /dev/cu.usbmodemXXXX
```

Build and package SD apps:

```bash
./tools/build-apps.sh
./tools/package-sd.sh
```

The app builder discovers sibling repos beside this checkout by default. Use
`WAVESHARE_OS_WORKSPACE_ROOT=/path/to/repos` or the per-app overrides documented
in the README for nonstandard layouts.

## Scope

- Target hardware: Waveshare ESP32-S3-Touch-AMOLED-1.8.
- Display/touch stack: SH8601 QSPI AMOLED and FT3168 touch.
- Supported launch model: SD catalog installs app `.bin` files into the app
  partition, then reboots.
- Input model: touch-first launcher with BOOT button and serial CLI fallback.
- Keep the launcher small and local. No online OTA catalog, WebUI, USB mass
  storage, multi-board ports, PlatformIO, or ESP-IDF project files.

## App Status

The catalog is the Cypher OS app set minus `starbeam_v2`, plus the two local
Waveshare diagnostics. `cypher-drive`, `cypher-chat`, and `flock-you` are direct
Waveshare AMOLED profile builds. Cardputer keyboard/audio/pin-heavy apps live
here as touch-native Lite or hardware-required ports. Keep per-app user docs in
`docs/apps/` whenever the catalog changes.
