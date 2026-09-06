#!/usr/bin/env bash
# Build (if needed) and merge bootloader + partition + app + SPIFFS into one flashable image.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"
DIST="${ROOT}/dist"
OUT="${DIST}/attract_player_flash.bin"

export IDF_PATH="${IDF_PATH:-/Users/klipper/esp/esp-idf}"
if [[ -z "${IDF_PYTHON_ENV_PATH:-}" ]]; then
  export IDF_PYTHON_ENV_PATH=/Users/klipper/.espressif/python_env/idf5.3_py3.12_env
fi

# shellcheck disable=SC1091
source "${IDF_PATH}/export.sh" >/dev/null

cd "$ROOT"
if [[ ! -f "${BUILD}/attract_player.bin" || ! -f "${BUILD}/storage.bin" ]]; then
  echo "Building attract_player..."
  idf.py build
fi

mkdir -p "$DIST"
esptool.py --chip esp32s3 merge_bin \
  -o "$OUT" \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0    "${BUILD}/bootloader/bootloader.bin" \
  0x8000 "${BUILD}/partition_table/partition-table.bin" \
  0x10000 "${BUILD}/attract_player.bin" \
  0x190000 "${BUILD}/storage.bin"

ls -lh "$OUT"
echo "Merged image ready: $OUT"
echo "Flash with: ./flash.sh [/dev/cu.usbmodemXXXX]"
