#!/usr/bin/env bash
# Builds the firmware and copies it to the Nucleo board's USB drive.
#
# Usage (from anywhere inside the repo):  tools/flash.sh [Release|Debug]
#
# The ST-LINK on the Nucleo shows up as a USB drive; copying a .bin file onto
# it programs the STM32. This script finds that drive by looking for the
# DETAILS.TXT file every ST-LINK drive contains.
set -euo pipefail   # stop at the first error, unset variable, or failed pipe

preset="${1:-Release}"
user="${USER:-$(id -un)}"                  # your username (for /media/<user>)
repo="$(cd "$(dirname "$0")/.." && pwd)"   # the repo root, wherever we're called from
fw="$repo/firmware"

echo "== building firmware ($preset)"
(cd "$fw" && cmake --preset "$preset" >/dev/null && cmake --build --preset "$preset")

elf="$fw/build/$preset/firmware.elf"
bin="$fw/build/$preset/firmware.bin"
arm-none-eabi-objcopy -O binary "$elf" "$bin"

drive=""
for d in /media/"$user"/* /run/media/"$user"/*; do
    if [ -f "$d/DETAILS.TXT" ]; then drive="$d"; break; fi
done
if [ -z "$drive" ]; then
    echo "error: no ST-LINK drive found under /media/$user. Is the board plugged in?" >&2
    exit 1
fi

echo "== copying $(basename "$bin") ($(stat -c %s "$bin") bytes) to $drive"
cp "$bin" "$drive/"
sync
echo "== flashed. Open the terminal and press RESET:"
echo "   python3 -m serial.tools.miniterm /dev/ttyACM0 115200"
