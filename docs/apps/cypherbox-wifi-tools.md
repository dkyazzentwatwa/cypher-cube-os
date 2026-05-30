# Cypher Cube WiFi Tools

Status: `cypherbox_port`

Touch-native Cypher Cube port of the Cypherbox Mini WiFi tool family. This app
uses only the Cypher Cube's built-in ESP32-S3 radio, touch screen, SD card, BOOT
button, and USB serial shell.

## Features

- Passive AP scan with SSID, BSSID, channel, RSSI, and auth display.
- Channel heatmap for 2.4 GHz WiFi.
- Captive portal with 10 demo templates and SD capture logging.
- Active WiFi lab tools: deauth target, deauth all, beacon flood, probe flood,
  and PMKID capture.
- Read-only SD web server over AP `WAVE-CYPHERBOX`.
- Shared SD file listing under `/waveshare-os/cypherbox`.

## Controls

- Tap menu rows to run tools.
- Swipe up/down or left/right to move through lists.
- BOOT exits foreground tools.
- Hold BOOT returns to the launcher.

## Serial

USB serial is `115200` baud.

```text
help
status
wifi scan
wifi list
wifi heatmap
web start
portal list
portal start <0-9>
attack deauth-target
attack deauth-all
attack beacon
attack probe
attack pmkid
sd list
stop
```

NFC, APDU, GPS, and wardriver commands report unsupported because this port does
not assume external PN532 or GPS hardware.

Use active tools only on networks and devices you own or have explicit
permission to test.
