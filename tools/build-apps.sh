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
AUDIO_LIB="${ROOT}/libraries/WaveshareAmoledAudio"
CYPHERBOX_LIB="${ROOT}/libraries/WaveshareCypherboxPort"
SENSORS_LIB="${ROOT}/libraries/WaveshareAmoledSensors"

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

LIB_FLAGS=(
  --library "${RETURN_LIB}"
  --library "${APPKIT_LIB}"
  --library "${PORTS_LIB}"
  --library "${AUDIO_LIB}"
  --library "${CYPHERBOX_LIB}"
  --library "${SENSORS_LIB}"
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
add_lib "SensorLib"
add_lib "TinyGPSPlus"
add_lib "M5GFX"
add_lib "M5Unified"
add_lib "M5Cardputer"
add_lib "FastLED"
add_lib "IRremote"
add_lib "Adafruit_SSD1306"
add_lib "Adafruit_PN532"
add_lib "ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays"
add_lib "NimBLE-Arduino"

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
  if [[ "${slug}" == "cypher-drive" && -f "${prepared}/src/main.cpp" ]]; then
    python3 - "${prepared}/src/main.cpp" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
include = "#include <WaveshareAmoledIntro.h>\n"
if include not in text:
    marker = "#include <CypherPuterReturn.h>\n"
    if marker not in text:
        raise SystemExit(f"missing CypherPuterReturn include in {path}")
    text = text.replace(marker, marker + include, 1)

needle = "static void drawIntroScreen(DisplayPort &display) {"
start = text.find(needle)
if start < 0:
    raise SystemExit(f"drawIntroScreen not found in {path}")

pos = start + len(needle)
depth = 1
while pos < len(text) and depth:
    ch = text[pos]
    if ch == "{":
        depth += 1
    elif ch == "}":
        depth -= 1
    pos += 1
if depth:
    raise SystemExit(f"drawIntroScreen braces did not balance in {path}")

replacement = """static void drawIntroScreen(DisplayPort &display) {
  WaveshareAmoledIntro::draw(display.gfx(), display.width(), display.height(),
                             "Cypher Drive");
}
"""
text = text[:start] + replacement + text[pos:]
path.write_text(text, encoding="utf-8")
PY
  fi
  if [[ "${slug}" == "cypher-chat" && -f "${prepared}/CardputerUI.cpp" ]]; then
    python3 - "${prepared}/CardputerUI.cpp" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
include = "#include <WaveshareAmoledIntro.h>\n"
if include not in text:
    marker = '#include "PowerPort.h"\n'
    if marker not in text:
        raise SystemExit(f"missing PowerPort include in {path}")
    text = text.replace(marker, marker + "\n" + include, 1)

old = """void CardputerUI::begin() {
  displayPort.begin();
  powerPort.begin();
"""
new = """void CardputerUI::begin() {
  displayPort.begin();
  if (displayPort.available()) {
    WaveshareAmoledIntro::draw(displayPort.gfx(), displayPort.width(),
                               displayPort.height(), "Cypher Chat");
  }
  powerPort.begin();
"""
if old not in text:
    raise SystemExit(f"CardputerUI::begin insertion point not found in {path}")
text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")
PY
  fi
  if [[ "${slug}" == "flock-you" && -f "${prepared}/src/FlockYouCore.h" ]]; then
    python3 - "${prepared}/src/FlockYouCore.h" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
intro_include = "#include <WaveshareAmoledIntro.h>\n"
if intro_include not in text:
    marker = "#if USE_AMOLED_DISPLAY\n"
    if marker not in text:
        raise SystemExit(f"missing AMOLED include block in {path}")
    text = text.replace(marker, marker + intro_include, 1)

old_splash = """  amoled->drawRoundRect(20, 36, AMOLED_W - 40, AMOLED_H - 72, 18, AMOLED_ACCENT);
  amoledText(55, 128, "CYPHER", 4, AMOLED_WHITE);
  amoledText(55, 178, "FLOCK", 4, AMOLED_ACCENT);
  amoledText(55, 250, "passive detector", 2, AMOLED_DIM);
  char pwr[16];
  powerLabel(pwr, sizeof(pwr));
  amoledPrintf(55, 282, 2, AMOLED_OK, "touch %s  power %s", uiTouchReady ? "ok" : "off", pwr);
  delay(5000);
"""
new_splash = """  WaveshareAmoledIntro::draw(*amoled, AMOLED_W, AMOLED_H, "Flock You");
"""
if old_splash in text:
    text = text.replace(old_splash, new_splash, 1)
elif "AMOLED splash drawing" in text and "WaveshareAmoledIntro::draw" not in text:
    raise SystemExit(f"unrecognized AMOLED splash block in {path}")

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

CYPHER_DRIVE_DIR="${WAVESHARE_OS_CYPHER_DRIVE_DIR:-${WORKSPACE_ROOT}/cypher-drive}"
CYPHER_CHAT_ROOT="${WAVESHARE_OS_CYPHER_CHAT_DIR:-${WORKSPACE_ROOT}/cypher-chat}"
if [[ -d "${CYPHER_CHAT_ROOT}/cypher-chat-firmware" ]]; then
  CYPHER_CHAT_DIR="${CYPHER_CHAT_ROOT}/cypher-chat-firmware"
else
  CYPHER_CHAT_DIR="${CYPHER_CHAT_ROOT}"
fi
FLOCK_YOU_DIR="${WAVESHARE_OS_FLOCK_YOU_DIR:-${WORKSPACE_ROOT}/flock-you}"

# Catalog order = launcher menu order. Featured apps first, utilities last.

build_local "Pomodoro" "pomodoro" "pomodoro.bin" "ready" "true" \
  "Focus timer (25/5/15) with theme colors, RTC clock, and tap controls."

build_sketch "Cypher Chat" "cypher-chat" "${CYPHER_CHAT_DIR}" "cypher-chat.bin" "ready" "true" \
  "Direct Waveshare AMOLED mesh chat build from sibling cypher-chat firmware." \
  "-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" 1

build_sketch "Cypher Drive" "cypher-drive" "${CYPHER_DRIVE_DIR}" "cypher-drive.bin" "ready" "true" \
  "Direct AMOLED profile build from sibling cypher-drive with Cypher launcher return helper." \
  "-DESP32 -DBOARD_PROFILE=BOARD_PROFILE_WAVESHARE_TOUCH_AMOLED_18" 1

build_sketch "Flock You" "flock-you" "${FLOCK_YOU_DIR}" "flock-you.bin" "ready" "true" \
  "Direct Waveshare AMOLED profile build from sibling flock-you detector." \
  "-DESP32 -DBOARD_PROFILE=ESP32_WAVESHARE_AMOLED_18 -DCYPHER_OS_LAUNCHER_RETURN" 1

build_local "Cypherbox WiFi Tools" "cypherbox-wifi-tools" "cypherbox-wifi-tools.bin" "cypherbox_port" "true" \
  "Touch and USB serial WiFi lab tools: scan, heatmap, captive portal, attacks, and read-only SD web server."
build_local "Cypherbox BLE Tools" "cypherbox-ble-tools" "cypherbox-ble-tools.bin" "cypherbox_port" "true" \
  "Touch, USB serial, and BLE Serial tools: BLE scan, spam, HID payloads, mouse jiggler, pairing, and SD file listing."

build_local "Haunted Radio Operator" "gameos-haunted-radio-operator" "haunted-radio-operator.bin" "prototype" "true" \
  "Touch-first fictional signal-capture game with optional ES8311 WAV assets stored on SD."

# --- Utilities (bottom of the launcher menu) ---
build_local "Tricorder" "tricorder" "tricorder.bin" "ready" "true" \
  "Battery, RTC, IMU, and strongest WiFi RSSI on one screen."
build_local "SD Status" "sd-status" "sd-status.bin" "ready" "true" \
  "Shows SD mount, capacity, usage, and catalog presence."
build_local "Touch Diagnostics" "touch-diagnostics" "touch-diagnostics.bin" "ready" "true" \
  "AMOLED and FT3168 touch coordinate test with BOOT return."
build_local "Audio Diagnostics" "audio-diagnostics" "audio-diagnostics.bin" "ready" "true" \
  "ES8311 speaker playback test for 16-bit PCM WAV files stored on the SD card."

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
