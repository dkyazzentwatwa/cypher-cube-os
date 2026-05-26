#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-}"
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=custom"

ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli || true)}"
if [[ -z "${ARDUINO_CLI}" && -x /opt/homebrew/bin/arduino-cli ]]; then
  ARDUINO_CLI="/opt/homebrew/bin/arduino-cli"
fi

if [[ -z "${ARDUINO_CLI}" ]]; then
  echo "[flash] arduino-cli not found on PATH"
  exit 127
fi

if [[ -z "${PORT}" ]]; then
  PORT="$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "${PORT}" ]]; then
  echo "[flash] connect the Waveshare ESP32-S3, then run: arduino-cli board list"
  exit 1
fi

if [[ ! -f "${ROOT}/dist/launcher/waveshare-amoled-os.ino.bin" ]]; then
  echo "[flash] launcher build missing; compiling first"
  "${ROOT}/tools/build-launcher.sh"
fi

echo "[flash] touching ${PORT} at 1200 baud"
stty -f "${PORT}" 1200 || true
sleep 2

NEW_PORT="$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1 || true)"
if [[ -n "${NEW_PORT}" ]]; then
  PORT="${NEW_PORT}"
fi

echo "[flash] uploading on ${PORT}"
"${ARDUINO_CLI}" upload \
  --fqbn "${FQBN}" \
  --input-dir "${ROOT}/dist/launcher" \
  -p "${PORT}" \
  "${ROOT}"

echo "[flash] done"
