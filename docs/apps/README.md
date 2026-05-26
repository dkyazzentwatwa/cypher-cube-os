# Waveshare Catalog Apps

This folder documents each app that `./tools/build-apps.sh` writes into
`dist/apps/apps.json`.

Status meanings:

- `ready`: direct Waveshare AMOLED build or full local diagnostic app.
- `lite`: touch-first simplified port that avoids Cardputer keyboard, speaker,
  or pin assumptions.
- `campaign_lite`: touch-first Story Lite RPG with campaign state, stats,
  quest/lore screens, inventory, and SD journal/state files.
- `build_failed`: source was discovered but did not compile; not installable.

All installable app binaries are sketch `.bin` files intended for the launcher's
`app1` slot. Merged, bootloader, and partition images are not packaged.
