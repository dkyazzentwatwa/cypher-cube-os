# Cypher Drive

Status: `ready`

Source route: direct AMOLED profile build from the sibling `cypher-drive` repo.

## Cypher Cube Controls

- Uses the existing Cypher Cube touch UI from `cypher-drive`.
- BOOT and touch behavior are controlled by the upstream app profile.
- Launcher return is available through the `CypherPuterReturn.h`
  compatibility alias when the app calls the helper.

## SD Data

Runtime files remain governed by `cypher-drive`. The Cypher Cube launcher only
packages `cypher-drive.bin` under `/waveshare-os/apps/`.

## Port Notes

Built with `BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18`. HID/payload behavior is
not expanded in this port pass.
