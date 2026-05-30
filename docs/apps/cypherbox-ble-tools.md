# Cypher Cube BLE Tools

Status: `cypherbox_port`

Touch-native Cypher Cube port of the Cypherbox Mini BLE tool family. This app
uses the built-in ESP32-S3 BLE radio, touch screen, SD card, BOOT button, USB
serial, and an optional Nordic UART BLE Serial shell.

## Features

- BLE Serial foreground mode advertised as `WAVE-CYPHERBOX`.
- BLE scan with vendor decode, per-device browsing, USB output, and CSV export.
- BLE advertising-spam lab vectors for Apple, Samsung, Google Fast Pair,
  Microsoft Swift Pair, or rotating all vectors.
- BT HID DuckyScript launcher from `/waveshare-os/cypherbox/payloads`.
- Standalone BLE mouse jiggler.
- HID pairing/bond management.
- Shared SD file listing under `/waveshare-os/cypherbox`.

## Controls

- Tap menu rows to run tools.
- Swipe up/down or left/right to browse vectors, devices, and payloads.
- Long press in BLE scan saves `ble_NNN.csv` to the shared logs folder.
- BOOT exits foreground tools.
- Hold BOOT returns to the launcher.

## Serial

USB serial is `115200` baud. BLE Serial exposes a Nordic UART shell for the same
human-oriented status and SD commands, while alternate BLE tools must be launched
from touch or USB serial after leaving BLE Serial.

```text
help
status
ble serial
ble scan
ble list
ble spam
ble hid
ble mouse
ble pairing
sd list
stop
```

NFC, APDU, GPS, and wardriver commands report unsupported because this port does
not assume external PN532 or GPS hardware.

Use active BLE spam and HID tools only on devices and hosts you own or have
explicit permission to test.
