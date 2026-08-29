#!/bin/sh
# Diffs the screen the 16-bit DOS build drew under DOSBox against the one the
# native build draws from the same program. They should be identical: the
# layout is the same code, and only the backend below tui.h differs.
#
# The capture is produced by tools/capture/mkide.sh plus capture.html?name=idedos.
# A missing capture skips rather than fails, like the other capture tests.
set -e
cd "$(dirname "$0")/.."

ref=tools/capture/captured/idedos.txt
[ -f "$ref" ] || { echo "run_ide_dos: no capture, skipping"; exit 0; }

./build/asoft-ide -dump build/ide-native.txt web/bundle/TESTS.BAS >/dev/null 2>&1
tr -d '\r' < "$ref" > build/ide-dos.txt

if diff -u build/ide-native.txt build/ide-dos.txt > build/ide-dos.diff; then
    echo "run_ide_dos: DOS and native draw the same screen"
    exit 0
fi
echo "run_ide_dos: FAILED"
cat build/ide-dos.diff
exit 1
