#!/bin/sh
# Drives the windowed front end through a pty and diffs the screen it drew
# against a stored snapshot. This covers more than the layout:
#
#   - the menu reaches the interpreter: toggling the HTAB bug off has to
#     change POS(0) from 11 to 9 in the transcript
#   - the editor reaches the program: line 20 is edited in place in the
#     listing, and RUN then has to print the edited text, not the typed one
#   - the mouse works: the second bug is switched back on by clicking the
#     menu rather than walking it with the keyboard
set -e
cd "$(dirname "$0")/.."

out=build/ide.out
python3 tools/idegrab.py \
    '10 PRINT "HI"' ENTER \
    '20 PRINT "BETA"' ENTER \
    F5 \
    'HTAB 10: PRINT POS(0)' ENTER \
    F10 RIGHT RIGHT DOWN ENTER ESC \
    'HTAB 10: PRINT POS(0)' ENTER \
    TAB DOWN END BS BS BS BS BS 'EDITED"' DOWN TAB \
    F5 \
    CLICK:16,0 CLICK:18,3 ESC \
    'HTAB 10: PRINT POS(0)' ENTER \
    > "$out"

if diff -u tests/ide_expected.txt "$out" > build/ide.diff; then
    echo "run_ide: exact match"
    exit 0
fi
echo "run_ide: FAILED"
cat build/ide.diff
exit 1
