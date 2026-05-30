#pragma once

// Standalone USB-serial shell baked into the launcher firmware.
//
// Exposes WiFi, BLE, and SD-card control over the USB CDC serial port so the
// ESP32-S3 stays fully drivable even with no SD card inserted (and therefore no
// installable apps). Text-only: the launcher has no WiFi/BLE display UI.
//
// Self-contained: depends only on the ESP32 core (WiFi.h, SD_MMC), the
// NimBLE-Arduino library, and WaveshareAmoledSensors. It does not touch any
// launcher internals; the launcher just routes matching command lines here.

namespace SerialShell {

// Handle one trimmed command line. Returns true if the line matched one of the
// shell's command groups (wifi / ble / sd / sensors), false otherwise so the
// caller can fall through to its own dispatch.
bool handleLine(char* cmd);

// Print the shell's command groups (used by the launcher's `help` command).
void printHelp();

}  // namespace SerialShell
