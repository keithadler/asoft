#!/bin/sh
# Drives the windowed front end through a pty and diffs the screen it drew
# against a stored snapshot. Catches layout regressions, and that the menu
# actually reaches the interpreter: the script toggles the HTAB bug off, so
# POS(0) has to change from 11 to 9 in the transcript.
set -e
cd "$(dirname "$0")/.."

out=build/ide.out
python3 tools/idegrab.py \
    '10 PRINT "HI"' ENTER \
    F5 \
    'HTAB 10: PRINT POS(0)' ENTER \
    F10 RIGHT RIGHT DOWN ENTER ESC \
    'HTAB 10: PRINT POS(0)' ENTER \
    > "$out"

if diff -u tests/ide_expected.txt "$out" > build/ide.diff; then
    echo "run_ide: exact match"
    exit 0
fi
echo "run_ide: FAILED"
cat build/ide.diff
exit 1
