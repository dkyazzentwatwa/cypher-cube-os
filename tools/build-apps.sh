#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="${ROOT}/dist/apps"
BUILD_ROOT="${ROOT}/build/apps"
SOURCE_ROOT="${BUILD_ROOT}/sources"
OUT_ROOT="${BUILD_ROOT}/out"
CATALOG_TSV="${BUILD_ROOT}/catalog.tsv"
WORKSPACE_ROOT="${WAVESHARE_OS_WORKSPACE_ROOT:-$(cd "${ROOT}/.." && pwd)}"
ARDUINO_LIB_ROOT="${ARDUINO_LIB_ROOT:-${HOME}/Documents/Arduino/libraries}"

FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=custom"
VERSION="0.1.0"

RETURN_LIB="${ROOT}/libraries/WaveshareAmoledReturn"
APPKIT_LIB="${ROOT}/libraries/WaveshareAmoledAppKit"
PORTS_LIB="${ROOT}/libraries/WaveshareAmoledPorts"

ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli || true)}"
if [[ -z "${ARDUINO_CLI}" && -x /opt/homebrew/bin/arduino-cli ]]; then
  ARDUINO_CLI="/opt/homebrew/bin/arduino-cli"
fi

if [[ -z "${ARDUINO_CLI}" ]]; then
  echo "[apps] arduino-cli not found on PATH"
  exit 127
fi

rm -rf "${DIST}" "${BUILD_ROOT}"
mkdir -p "${DIST}" "${SOURCE_ROOT}" "${OUT_ROOT}"
: > "${CATALOG_TSV}"

if [[ -f "${ROOT}/tools/catalog/generate-gameos-ports.py" ]]; then
  python3 "${ROOT}/tools/catalog/generate-gameos-ports.py" >/dev/null
fi

LIB_FLAGS=(
  --library "${RETURN_LIB}"
  --library "${APPKIT_LIB}"
  --library "${PORTS_LIB}"
)

add_lib() {
  local name="$1"
  if [[ -d "${ARDUINO_LIB_ROOT}/${name}" ]]; then
    LIB_FLAGS+=(--library "${ARDUINO_LIB_ROOT}/${name}")
  fi
}

add_lib "Adafruit_GFX_Library"
add_lib "Adafruit_BusIO"
add_lib "Adafruit_XCA9554"
add_lib "GFX_Library_for_Arduino"
add_lib "Arduino_DriveBus"
add_lib "ArduinoJson"
add_lib "XPowersLib"
add_lib "TinyGPSPlus"
add_lib "M5GFX"
add_lib "M5Unified"
add_lib "M5Cardputer"
add_lib "FastLED"
add_lib "IRremote"
add_lib "Adafruit_SSD1306"
add_lib "Adafruit_PN532"
add_lib "ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays"

json_record() {
  local name="$1"
  local slug="$2"
  local binary="$3"
  local status="$4"
  local installable="$5"
  local notes="$6"
  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
    "${name}" "${slug}" "${binary}" "${VERSION}" "${status}" "${installable}" "${notes}" >> "${CATALOG_TSV}"
}

prepare_source() {
  local src="$1"
  local slug="$2"
  local base
  base="$(basename "${src}")"
  local prepared="${SOURCE_ROOT}/${base}"
  rm -rf "${prepared}"
  mkdir -p "${prepared}"
  rsync -a \
    --exclude ".git" \
    --exclude ".pio" \
    --exclude "build" \
    --exclude "dist" \
    --exclude ".DS_Store" \
    "${src}/" "${prepared}/"
  cp -f "${ROOT}/partitions.csv" "${prepared}/partitions.csv"
  echo "${prepared}"
}

patch_prepared_source() {
  local slug="$1"
  local prepared="$2"
  if [[ "${slug}" == "cypher-drive" || "${slug}" == "cypher-chat" ]]; then
    local touch_file
    for touch_file in "${prepared}/src/TouchInput.cpp" "${prepared}/TouchInput.cpp"; do
      if [[ ! -f "${touch_file}" ]]; then
        continue
      fi
      python3 - "${touch_file}" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
old = """void remapTouchForRotation(int32_t rawX, int32_t rawY, uint16_t &outX, uint16_t &outY) {
  int32_t x = rawY;
  int32_t y = (LCD_HEIGHT - 1) - rawX;
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  if (x >= LCD_WIDTH) {
    x = LCD_WIDTH - 1;
  }
  if (y >= LCD_HEIGHT) {
    y = LCD_HEIGHT - 1;
  }
  outX = static_cast<uint16_t>(x);
  outY = static_cast<uint16_t>(y);
}"""
new = """void remapTouchForRotation(int32_t rawX, int32_t rawY, uint16_t &outX, uint16_t &outY) {
  int32_t x = rawX;
  int32_t y = rawY;
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  if (x >= LCD_WIDTH) {
    x = LCD_WIDTH - 1;
  }
  if (y >= LCD_HEIGHT) {
    y = LCD_HEIGHT - 1;
  }
  outX = static_cast<uint16_t>(x);
  outY = static_cast<uint16_t>(y);
}"""
if old in text:
    text = text.replace(old, new)
elif "remapTouchForRotation" in text and "int32_t x = rawY;" in text:
    raise SystemExit(f"unrecognized FT3168 touch remap in {path}")
path.write_text(text, encoding="utf-8")
PY
    done
  fi
  if [[ "${slug}" == "flock-you" && -f "${prepared}/src/FlockYouCore.h" ]]; then
    python3 - "${prepared}/src/FlockYouCore.h" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
old_touch = """static void amoledRemapTouch(int32_t rawX, int32_t rawY, int16_t& outX, int16_t& outY) {
  int32_t x = rawY;
  int32_t y = (AMOLED_H - 1) - rawX;
  outX = (int16_t)constrain(x, 0, AMOLED_W - 1);
  outY = (int16_t)constrain(y, 0, AMOLED_H - 1);
}"""
new_touch = """static void amoledRemapTouch(int32_t rawX, int32_t rawY, int16_t& outX, int16_t& outY) {
  int32_t x = rawX;
  int32_t y = rawY;
  outX = (int16_t)constrain(x, 0, AMOLED_W - 1);
  outY = (int16_t)constrain(y, 0, AMOLED_H - 1);
}"""
if old_touch in text:
    text = text.replace(old_touch, new_touch)
elif "static void amoledRemapTouch" in text and "int32_t x = rawY;" in text:
    raise SystemExit(f"unrecognized FT3168 touch remap in {path}")
text = text.replace(
    "#if USE_CARDPUTER_DISPLAY\n#include <CypherPuterReturn.h>\n#endif",
    "#if USE_CARDPUTER_DISPLAY || defined(CYPHER_OS_LAUNCHER_RETURN)\n#include <CypherPuterReturn.h>\n#endif",
)
text = text.replace(
    "#if USE_CARDPUTER_DISPLAY\n  cypherPuterReturnToLauncher(delayMs);\n#else",
    "#if USE_CARDPUTER_DISPLAY || defined(CYPHER_OS_LAUNCHER_RETURN)\n  cypherPuterReturnToLauncher(delayMs);\n#else",
)
path.write_text(text, encoding="utf-8")
PY
  fi
}

copy_app_bin() {
  local build_dir="$1"
  local dest="$2"
  local found

  found="$(find "${build_dir}" -maxdepth 1 -type f -name "*.ino.bin" | head -n 1 || true)"
  if [[ -z "${found}" ]]; then
    found="$(find "${build_dir}" -maxdepth 1 -type f -name "*.bin" \
      ! -name "*.merged.bin" ! -name "*.bootloader.bin" ! -name "*.partitions.bin" | head -n 1 || true)"
  fi
  if [[ -z "${found}" ]]; then
    echo "[apps] no sketch app .bin found in ${build_dir}"
    return 1
  fi
  cp -f "${found}" "${DIST}/${dest}"
  echo "[apps] packaged ${dest}"
}

build_sketch() {
  local name="$1"
  local slug="$2"
  local src="$3"
  local binary="$4"
  local status="$5"
  local installable="$6"
  local notes="$7"
  local extra_flags="${8:-}"
  local required="${9:-1}"
  local prepared out rc

  if [[ ! -d "${src}" ]]; then
    echo "[apps] missing source for ${slug}: ${src}"
    json_record "${name}" "${slug}" "${binary}" "build_failed" "false" "Source not found: ${src}"
    if [[ "${required}" == "1" ]]; then FAILED_REQUIRED=1; fi
    return 0
  fi

  prepared="$(prepare_source "${src}" "${slug}")"
  patch_prepared_source "${slug}" "${prepared}"
  out="${OUT_ROOT}/${slug}"
  rm -rf "${out}"
  mkdir -p "${out}"

  echo "[apps] building ${slug}"
  set +e
  if [[ -n "${extra_flags}" ]]; then
    "${ARDUINO_CLI}" compile \
      --fqbn "${FQBN}" \
      "${LIB_FLAGS[@]}" \
      --build-property "build.extra_flags=${extra_flags}" \
      --output-dir "${out}" \
      "${prepared}"
    rc=$?
  else
    "${ARDUINO_CLI}" compile \
      --fqbn "${FQBN}" \
      "${LIB_FLAGS[@]}" \
      --output-dir "${out}" \
      "${prepared}"
    rc=$?
  fi
  set -e

  if [[ "${rc}" -ne 0 ]]; then
    echo "[apps] build failed: ${slug}"
    json_record "${name}" "${slug}" "${binary}" "build_failed" "false" "Build failed; see terminal output."
    if [[ "${required}" == "1" ]]; then FAILED_REQUIRED=1; fi
    return 0
  fi

  if ! copy_app_bin "${out}" "${binary}"; then
    json_record "${name}" "${slug}" "${binary}" "build_failed" "false" "Compiled but no sketch app binary was produced."
    if [[ "${required}" == "1" ]]; then FAILED_REQUIRED=1; fi
    return 0
  fi

  json_record "${name}" "${slug}" "${binary}" "${status}" "${installable}" "${notes}"
}

FAILED_REQUIRED=0

build_local() {
  build_sketch "$1" "$2" "${ROOT}/apps/$2" "$3" "$4" "$5" "$6" "" "${7:-1}"
}

build_local "Touch Diagnostics" "touch-diagnostics" "touch-diagnostics.bin" "ready" "true" "AMOLED and FT3168 touch coordinate test with BOOT return."
build_local "SD Status" "sd-status" "sd-status.bin" "ready" "true" "Shows SD mount, capacity, usage, and catalog presence."

CYPHER_DRIVE_DIR="${WAVESHARE_OS_CYPHER_DRIVE_DIR:-${WORKSPACE_ROOT}/cypher-drive}"
CYPHER_CHAT_ROOT="${WAVESHARE_OS_CYPHER_CHAT_DIR:-${WORKSPACE_ROOT}/cypher-chat}"
if [[ -d "${CYPHER_CHAT_ROOT}/cypher-chat-firmware" ]]; then
  CYPHER_CHAT_DIR="${CYPHER_CHAT_ROOT}/cypher-chat-firmware"
else
  CYPHER_CHAT_DIR="${CYPHER_CHAT_ROOT}"
fi
FLOCK_YOU_DIR="${WAVESHARE_OS_FLOCK_YOU_DIR:-${WORKSPACE_ROOT}/flock-you}"

build_sketch "Cypher Drive" "cypher-drive" "${CYPHER_DRIVE_DIR}" "cypher-drive.bin" "ready" "true" \
  "Direct AMOLED profile build from sibling cypher-drive with Cypher launcher return helper." \
  "-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" 1

build_sketch "Cypher Chat" "cypher-chat" "${CYPHER_CHAT_DIR}" "cypher-chat.bin" "ready" "true" \
  "Direct Waveshare AMOLED mesh chat build from sibling cypher-chat firmware." \
  "-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" 1

build_sketch "Flock You" "flock-you" "${FLOCK_YOU_DIR}" "flock-you.bin" "ready" "true" \
  "Direct Waveshare AMOLED profile build from sibling flock-you detector." \
  "-DESP32 -DBOARD_PROFILE=ESP32_WAVESHARE_AMOLED_18 -DCYPHER_OS_LAUNCHER_RETURN" 1

build_local "Cardputer Games" "cardputer-games-lite" "cardputer-games-lite.bin" "lite" "true" \
  "Touch arcade shelf with high scores, replayable modes, and SD score history."
build_local "Cardputer Tarot" "cardputer-tarot-lite" "cardputer-tarot-lite.bin" "lite" "true" \
  "Touch one-card and three-card Tarot draw with SD reading history."

GAME_OS_APPS=(
  "Cryptid Ranger|gameos-cryptid-park-ranger|Cryptid Ranger campaign with stats, lore, pack, quest log, and SD saves."
  "Cyber Ranger|gameos-cyber-ranger|Cyber Ranger campaign with stats, lore, pack, quest log, and SD saves."
  "Cyberdeck RPG|gameos-cyberdeck-hacker-rpg|Cyberdeck Hacker RPG campaign with stats, lore, pack, quest log, and SD saves."
  "Dungeon Courier|gameos-dungeon-courier|Dungeon Courier campaign with stats, lore, pack, quest log, and SD saves."
  "Guildmaster|gameos-guildmaster-pocket|Guildmaster Pocket campaign with stats, lore, pack, quest log, and SD saves."
  "Haunted Radio|gameos-haunted-radio-operator|Haunted Radio Operator campaign with stats, lore, pack, quest log, and SD saves."
  "Monster Ranch|gameos-monster-ranch-trail|Monster Ranch Trail campaign with stats, lore, pack, quest log, and SD saves."
  "Pocket Detective|gameos-pocket-detective-agency|Pocket Detective Agency campaign with stats, lore, pack, quest log, and SD saves."
  "Pocket Kingdom|gameos-pocket-kingdom-manager|Pocket Kingdom Manager campaign with stats, lore, pack, quest log, and SD saves."
  "Signal Rat|gameos-signal-rat-cyberdeck-rpg|Signal Rat Cyberdeck RPG campaign with stats, lore, pack, quest log, and SD saves."
  "Star Trader|gameos-star-trader-pocket-frontier|Star Trader Pocket Frontier campaign with stats, lore, pack, quest log, and SD saves."
  "Star Trail|gameos-star-trail-rancher|Star Trail Rancher campaign with stats, lore, pack, quest log, and SD saves."
  "Tiny Wasteland|gameos-tiny-wasteland|Tiny Wasteland campaign with stats, lore, pack, quest log, and SD saves."
  "Wasteland Guild|gameos-wasteland-guildmaster|Wasteland Guildmaster campaign with stats, lore, pack, quest log, and SD saves."
)

for entry in "${GAME_OS_APPS[@]}"; do
  IFS="|" read -r name slug notes <<< "${entry}"
  build_local "${name}" "${slug}" "${slug}.bin" "campaign_lite" "true" "${notes}"
done

python3 - "${CATALOG_TSV}" "${DIST}/apps.json" <<'PY'
import csv
import json
import sys
from pathlib import Path

tsv = Path(sys.argv[1])
out = Path(sys.argv[2])
apps = []
with tsv.open(newline="", encoding="utf-8") as handle:
    for row in csv.reader(handle, delimiter="\t"):
        if not row:
            continue
        name, slug, binary, version, status, installable, notes = row
        apps.append({
            "name": name,
            "slug": slug,
            "binary": binary,
            "version": version,
            "status": status,
            "installable": installable == "true" and status != "build_failed",
            "notes": notes,
        })

manifest = {
    "schema": 1,
    "target": "waveshare-esp32-s3-touch-amoled-1.8",
    "launcher_slot": "app1",
    "generated_by": "tools/build-apps.sh",
    "apps": apps,
}
out.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
PY

echo "[apps] manifest: ${DIST}/apps.json"

if [[ "${FAILED_REQUIRED}" -ne 0 ]]; then
  echo "[apps] one or more required catalog apps failed"
  exit 1
fi
