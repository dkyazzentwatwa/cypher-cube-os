# Cypher Cube Technical Instructions

This document holds the build, flash, SD-card, and app-porting details for
Cypher Cube. The main README is the friendly overview; this file is the bench
notes.

## Hardware Target

- Waveshare ESP32-S3-Touch-AMOLED-1.8
- ESP32-S3R8 with 8 MB PSRAM and 16 MB flash
- SH8601 QSPI AMOLED, 368x448
- FT3168 capacitive touch
- SD_MMC card slot
- BOOT button fallback input
- AXP2101 PMU, PCF85063 RTC, QMI8658 6-axis IMU, ES8311 mic and speaker

Waveshare board references:

- https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8
- https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8/Development-Environment-Setup-Arduino
- https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8

## Install Dependencies

Install Arduino CLI and the ESP32 core, then install the regular libraries:

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "GFX Library for Arduino"
arduino-cli lib install "Adafruit XCA9554"
arduino-cli lib install "Adafruit BusIO"
arduino-cli lib install "NimBLE-Arduino"
arduino-cli lib install "XPowersLib"
arduino-cli lib install "SensorLib"
```

`XPowersLib` backs the AXP2101 battery path. `SensorLib` backs the PCF85063 RTC
and QMI8658 IMU path. ES8311 audio uses the in-core `ESP_I2S` library, so no
extra library install is needed for audio.

Install Waveshare's offline `Arduino_DriveBus` library from the official sample
package. It should appear in `arduino-cli lib list` as the FT3168 drive bus
library.

## Build And Flash Launcher

Compile with the repo helper. It passes the local Waveshare display and touch
library folders explicitly:

```bash
./tools/build-launcher.sh
```

The raw profile is also recorded in `sketch.yaml`:

```bash
arduino-cli compile --profile waveshare .
```

Flash with:

```bash
arduino-cli board list
./tools/flash-launcher.sh /dev/cu.usbmodemXXXX
```

The flash helper touches the current port at 1200 baud, waits for the ESP32-S3
to re-enumerate, then uploads with the `waveshare` profile.

## Build And Package SD Apps

Build the app catalog and package the SD-card folder:

```bash
./tools/build-apps.sh
./tools/package-sd.sh
```

Copy the contents of `dist/sd-card` to the root of a FAT32 SD card.

Expected SD layout:

```text
/waveshare-os/apps/apps.json
/waveshare-os/apps/pomodoro.bin
/waveshare-os/apps/cypher-chat.bin
/waveshare-os/apps/cypher-drive.bin
/waveshare-os/apps/flock-you.bin
/waveshare-os/apps/cypherbox-wifi-tools.bin
/waveshare-os/apps/cypherbox-ble-tools.bin
/waveshare-os/apps/tricorder.bin
/waveshare-os/apps/sd-status.bin
/waveshare-os/apps/touch-diagnostics.bin
/waveshare-os/cypherbox/COUNTER.TXT
/waveshare-os/cypherbox/logs/
/waveshare-os/cypherbox/payloads/macos/hello.duck
```

Sibling repo discovery defaults to repos beside this checkout. Override as
needed:

```bash
WAVESHARE_OS_WORKSPACE_ROOT=/path/to/repos ./tools/build-apps.sh
WAVESHARE_OS_CYPHER_DRIVE_DIR=/path/to/cypher-drive ./tools/build-apps.sh
WAVESHARE_OS_CYPHER_CHAT_DIR=/path/to/cypher-chat ./tools/build-apps.sh
WAVESHARE_OS_FLOCK_YOU_DIR=/path/to/flock-you ./tools/build-apps.sh
```

## Launcher Controls

- Touch app rows and buttons to navigate.
- Short BOOT press: back.
- Long BOOT press: home.
- Hold BOOT during launcher startup: force launcher and disable auto-boot app.
- Serial fallback is available at `115200` baud.

Serial commands:

```text
help
status
apps
reload
launch
erase
install <slug>
boot launcher
boot app
```

## App Contract

ESP32 apps do not execute directly from SD. The launcher lives in `ota_0`, SD
app binaries live under `/waveshare-os/apps`, and the selected sketch `.bin` is
copied into `ota_1`.

Apps must be compiled for the same Waveshare AMOLED target and partition layout.
Package sketch app `.bin` files only, not merged flash images.

Apps that support return can include:

```cpp
#include <WaveshareAmoledReturn.h>

WaveshareAmoledOs::returnToLauncher();
```

The helper sets the one-shot launcher return flag, points boot back to `ota_0`,
and restarts.

Cardputer-derived apps can also include the compatibility alias:

```cpp
#include <CypherPuterReturn.h>

cypherPuterReturnToLauncher();
```

New local ports should use `WaveshareAmoledAppKit`, which standardizes SH8601
display init, FT3168 touch, SD_MMC, BOOT short/back, BOOT long/home, serial
fallback, and launcher return.

## Repo Scope

This repo is Arduino CLI only and intentionally single-target. Do not add
PlatformIO, ESP-IDF project files, online OTA catalogs, USB mass storage, WebUI,
or multi-board support unless the project scope changes.
