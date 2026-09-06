#!/usr/bin/env bash
# Flash the merged attract-player image (bootloader + app + video SPIFFS) in one shot.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="${ROOT}/dist/attract_player_flash.bin"
PORT="${1:-/dev/cu.usbmodem1101}"

if [[ ! -f "$BIN" ]]; then
  echo "Missing $BIN — run scripts/make_flash_image.sh first (or rebuild the project)." >&2
  exit 1
fi

if ! command -v esptool.py >/dev/null 2>&1 && ! command -v esptool >/dev/null 2>&1; then
  echo "esptool.py not found. Install with: pip install esptool" >&2
  exit 1
fi

ESPTOOL="$(command -v esptool.py || command -v esptool)"

echo "Flashing attract player to $PORT ..."
"$ESPTOOL" --chip esp32s3 -p "$PORT" -b 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 "$BIN"

echo "Done. Badge should boot straight into the attract loop."
