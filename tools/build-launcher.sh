#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${ROOT}/dist/launcher"
ARDUINO_LIB_ROOT="${ARDUINO_LIB_ROOT:-${HOME}/Documents/Arduino/libraries}"
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=custom"

ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli || true)}"
if [[ -z "${ARDUINO_CLI}" && -x /opt/homebrew/bin/arduino-cli ]]; then
  ARDUINO_CLI="/opt/homebrew/bin/arduino-cli"
fi

if [[ -z "${ARDUINO_CLI}" ]]; then
  echo "[launcher] arduino-cli not found on PATH"
  exit 127
fi

rm -rf "${OUT}"
mkdir -p "${OUT}"

echo "[launcher] compiling Waveshare AMOLED OS"
"${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  --library "${ARDUINO_LIB_ROOT}/Adafruit_GFX_Library" \
  --library "${ARDUINO_LIB_ROOT}/Adafruit_BusIO" \
  --library "${ARDUINO_LIB_ROOT}/Adafruit_XCA9554" \
  --library "${ARDUINO_LIB_ROOT}/GFX_Library_for_Arduino" \
  --library "${ARDUINO_LIB_ROOT}/Arduino_DriveBus" \
  --output-dir "${OUT}" \
  "${ROOT}"

echo "[launcher] output: ${OUT}"
