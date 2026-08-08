#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${ROOT}/dist/launcher"
STAGING_ROOT="${ROOT}/build/launcher-source"
STAGING_SKETCH="${STAGING_ROOT}/waveshare-amoled-os"
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
rm -rf "${STAGING_ROOT}"
mkdir -p "${STAGING_SKETCH}"
rsync -a \
  --exclude ".git" \
  --exclude "build" \
  --exclude "dist" \
  --exclude ".DS_Store" \
  "${ROOT}/" "${STAGING_SKETCH}/"
mv "${STAGING_SKETCH}/cypher-cube-os.ino" "${STAGING_SKETCH}/waveshare-amoled-os.ino"

echo "[launcher] compiling Waveshare AMOLED OS"
"${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  --library "${ARDUINO_LIB_ROOT}/Adafruit_GFX_Library" \
  --library "${ARDUINO_LIB_ROOT}/Adafruit_BusIO" \
  --library "${ARDUINO_LIB_ROOT}/Adafruit_XCA9554" \
  --library "${ARDUINO_LIB_ROOT}/GFX_Library_for_Arduino" \
  --library "${ARDUINO_LIB_ROOT}/Arduino_DriveBus" \
  --library "${ARDUINO_LIB_ROOT}/XPowersLib" \
  --library "${ARDUINO_LIB_ROOT}/SensorLib" \
  --library "${ARDUINO_LIB_ROOT}/NimBLE-Arduino" \
  --library "${ROOT}/libraries/WaveshareAmoledReturn" \
  --library "${ROOT}/libraries/WaveshareAmoledSensors" \
  --output-dir "${OUT}" \
  "${STAGING_SKETCH}"

echo "[launcher] output: ${OUT}"
