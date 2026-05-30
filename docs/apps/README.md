# Cypher Cube Catalog Apps

This folder documents each app that `./tools/build-apps.sh` writes into
`dist/apps/apps.json`.

Status meanings:

- `ready`: direct Cypher Cube build or full local diagnostic app.
- `cypherbox_port`: grouped Cypherbox Mini-derived wireless app adapted for
  Cypher Cube hardware, WiFi/BLE radio, touch, SD, and serial.
- `build_failed`: source was discovered but did not compile; not installable.

All installable app binaries are sketch `.bin` files intended for the launcher's
`app1` slot. Merged, bootloader, and partition images are not packaged.

The current catalog intentionally excludes NFC, APDU Lab, tag emulation, GPS,
and wardriver features because those require external PN532/GPS modules.
