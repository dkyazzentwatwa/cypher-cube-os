# Cypher Cube Hardware Notes

The Cypher Cube v1 target is the Waveshare ESP32-S3-Touch-AMOLED-1.8.

The implementation uses the current Waveshare-documented hardware path:

- Display: SH8601 over QSPI
- Touch: FT3168 over I2C
- I/O expander: XCA9554/TCA9554 style expander at `0x20`
- SD: SD_MMC 1-bit mode
- Flash: 16 MB
- PSRAM: 8 MB OPI

Pin constants are kept in `src/BoardConfig.h` so the launcher and app examples
have one place to audit if Waveshare revises the board or if a different SKU is
used.
