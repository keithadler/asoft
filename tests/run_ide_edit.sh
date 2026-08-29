#!/bin/sh
# The editor and the mouse, in the terminal front end.
#
# Covers what one screen snapshot does not: clicking a line to put the cursor
# on it, moving within a line, backspace and delete, deleting a whole line,
# the guard that stops an unnumbered line being run as an immediate statement,
# and clicking back to the prompt. RUN at the end has to print what the
# editing produced, so the edits must have reached the program and not just
# the display.
set -e
cd "$(dirname "$0")/.."

out=build/ide-edit.out
python3 tools/idegrab.py \
    '10 PRINT "ONE"' ENTER \
    '20 PRINT "TWO"' ENTER \
    '30 PRINT "THREE"' ENTER \
    CLICK:5,31 \
    END BS BS BS BS 'EDIT"' \
    HOME RIGHT RIGHT \
    DOWN \
    F8 \
    CLICK:10,10 \
    F5 \
    TAB DOWN DOWN \
    'NO NUMBER HERE' \
    DOWN \
    > "$out"

if diff -u tests/ide_edit_expected.txt "$out" > build/ide-edit.diff; then
    echo "run_ide_edit: exact match"
    exit 0
fi
echo "run_ide_edit: FAILED"
cat build/ide-edit.diff
exit 1
