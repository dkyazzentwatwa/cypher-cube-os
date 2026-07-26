# Cypher Drive

Status: `ready`

Source route: direct AMOLED profile build from the sibling `cypher-drive` repo.

## Cypher Cube Controls

- Uses the existing Cypher Cube touch UI from `cypher-drive`.
- BOOT and touch behavior are controlled by the upstream app profile.
- Launcher return is available through the `CypherPuterReturn.h`
  compatibility alias when the app calls the helper.
- Includes the upstream **QR link** screen for a saved web link.

## QR Link

Open **QR link** from the Cypher Drive menu to show the saved URL as a
scannable QR code. If no link is saved, the screen shows setup hints.

Program the link over USB serial at `115200` baud:

```text
qr status
qr set https://example.com
qr show
qr clear
```

The same QR commands are available over Nordic UART BLE while Cypher Drive is
running. Connect to `CYPHER-DRIVE-QR`; the app pauses QR BLE advertising during
active BLE scans and resumes it after scanning stops.

## SD Data

Runtime files remain governed by `cypher-drive`. The Cypher Cube launcher only
packages `cypher-drive.bin` under `/waveshare-os/apps/`.

## Port Notes

Built with `BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18`. HID/payload behavior is
not expanded in this port pass.
