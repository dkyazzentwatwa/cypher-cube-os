#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIST="${ROOT}/dist/apps"
SD_ROOT="${ROOT}/dist/sd-card"
SD_OS="${SD_ROOT}/waveshare-os"
SD_APPS="${SD_OS}/apps"
WORKSPACE_ROOT="${WAVESHARE_OS_WORKSPACE_ROOT:-$(cd "${ROOT}/.." && pwd)}"

if [[ ! -f "${APP_DIST}/apps.json" ]]; then
  echo "[sd] run ./tools/build-apps.sh before packaging the SD card"
  exit 1
fi

python3 - "${APP_DIST}" <<'PY'
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
manifest = json.loads((root / "apps.json").read_text(encoding="utf-8"))
missing = []
for app in manifest.get("apps", []):
    if app.get("installable") and app.get("status") != "build_failed":
        binary = app.get("binary", "")
        if not binary or not (root / binary).is_file():
            missing.append(f"{app.get('slug')}: {binary}")
if missing:
    print("[sd] manifest references missing binaries:")
    for item in missing:
        print(f"  - {item}")
    sys.exit(1)
PY

rm -rf "${SD_ROOT}"
mkdir -p "${SD_APPS}"
cp -f "${APP_DIST}/apps.json" "${SD_APPS}/apps.json"
find "${APP_DIST}" -maxdepth 1 -type f -name "*.bin" -exec cp -f {} "${SD_APPS}/" \;

mkdir -p \
  "${SD_OS}/cardputer-games" \
  "${SD_OS}/cardputer-game-os/saves" \
  "${SD_OS}/cardputer-game-os/states" \
  "${SD_OS}/tarot"

cat > "${SD_OS}/README.txt" <<'TXT'
Waveshare AMOLED Cypher OS SD card

Copy this waveshare-os folder to the root of the FAT32 SD card.
The launcher reads waveshare-os/apps/apps.json and installs app .bin files
into the app1 partition.

BOOT short press backs out inside most apps.
BOOT long press returns to the launcher.
Serial fallback is 115200 baud.
TXT

echo "[sd] prepared ${SD_ROOT}"
echo "[sd] copy the contents of ${SD_ROOT} to the root of a FAT32 SD card"
