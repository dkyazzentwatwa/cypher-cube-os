# Cypher Cube

**A local-first SD app launcher for the Cypher Cube, built on the Waveshare
ESP32-S3-Touch-AMOLED-1.8.**

This is the Cypher Cube version of the tiny-OS idea: flash a launcher once, copy
app `.bin` files to a FAT32 SD card, tap an app in the catalog, and the
launcher installs that app into the app partition before rebooting.

ESP32 apps do not execute directly from SD. The launcher lives in `ota_0`, SD
app binaries live under `/waveshare-os/apps`, and the selected sketch `.bin` is
copied into `ota_1`.

## Cypher Cube Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.8
- ESP32-S3R8 with 8 MB PSRAM and 16 MB flash
- SH8601 QSPI AMOLED, 368x448
- FT3168 capacitive touch
- SD_MMC card slot
- BOOT button fallback input
- AXP2101 PMU (battery), PCF85063 RTC, QMI8658 6-axis IMU, ES8311 mic+speaker —
  exposed through the shared `WaveshareAmoledSensors` HAL

Waveshare documents the board, Arduino examples, SH8601 display, FT3168 touch,
and SD demo flow here:

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

`XPowersLib` (AXP2101 battery) and `SensorLib` (PCF85063 RTC + QMI8658 IMU) back
the shared `WaveshareAmoledSensors` HAL used by the launcher and the onboard
sensor apps. ES8311 audio uses the in-core `ESP_I2S` library, so no extra
install is needed.

Install Waveshare's offline `Arduino_DriveBus` library from the official sample
package. It should appear in `arduino-cli lib list` as the FT3168 drive bus
library.

## Build And Flash Launcher

Compile with the repo helper. It passes the local Waveshare display/touch
library folders explicitly:

```bash
./tools/build-launcher.sh
```

The profile is also recorded in `sketch.yaml`. If your Arduino CLI config
already searches `~/Documents/Arduino/libraries`, this may work directly:

```bash
arduino-cli compile --profile waveshare .
```

Flash:

```bash
arduino-cli board list
./tools/flash-launcher.sh /dev/cu.usbmodemXXXX
```

The flash helper touches the current port at 1200 baud, waits for the ESP32-S3
to re-enumerate, then uploads with the `waveshare` profile.

## Build And Package SD Apps

Build the real catalog:

```bash
./tools/build-apps.sh
./tools/package-sd.sh
```

Copy the contents of `dist/sd-card` to the root of a FAT32 SD card.

Expected layout:

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

The generated manifest currently ships onboard-hardware apps (`pomodoro` and
`tricorder`) built on the shared `WaveshareAmoledSensors` HAL, two local Cypher
Cube diagnostics (`sd-status`, `touch-diagnostics`), direct AMOLED profile
builds for `cypher-drive`, `cypher-chat`, and `flock-you`, plus two grouped
Cypherbox Mini-derived wireless ports for the Cypher Cube. Catalog order puts
featured apps first and the diagnostic utilities last:

- `cypherbox-wifi-tools`: WiFi scan, heatmap, captive portal, active WiFi lab
  attacks, read-only SD web server, and USB serial commands.
- `cypherbox-ble-tools`: BLE Serial, BLE scan/export, BLE advertising spam,
  BLE HID DuckyScript payloads, mouse jiggler, pairing tools, and USB serial
  commands.

NFC, APDU Lab, tag emulation, GPS, and wardriver are intentionally not included
in this Cypher Cube catalog pass because they require external PN532/GPS hardware.
Legacy Cardputer Lite and GameOS source folders may remain in the repo, but they
are not built, packaged, or advertised by `tools/build-apps.sh`.

The Cypher Cube wireless ports include active lab tools. Use them only on radios,
networks, devices, and hosts you own or have explicit permission to test.

Sibling repo discovery defaults to repos beside this checkout. Override as
needed:

```bash
WAVESHARE_OS_WORKSPACE_ROOT=/path/to/repos ./tools/build-apps.sh
WAVESHARE_OS_CYPHER_DRIVE_DIR=/path/to/cypher-drive ./tools/build-apps.sh
WAVESHARE_OS_CYPHER_CHAT_DIR=/path/to/cypher-chat ./tools/build-apps.sh
WAVESHARE_OS_FLOCK_YOU_DIR=/path/to/flock-you ./tools/build-apps.sh
```

Per-app operating docs live in [`docs/apps`](docs/apps).

## Controls

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

Apps must be compiled for the same Waveshare AMOLED target and partition layout.
Package sketch app `.bin` files only, not merged flash images. Apps that support
return can include:

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

## Scope

This repo is intentionally single-target and local-first. Do not add PlatformIO,
ESP-IDF project files, online catalogs, WebUI, USB mass storage, or multi-board
support unless the project scope changes.
