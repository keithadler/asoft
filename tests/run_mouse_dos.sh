#!/bin/sh
# Checks that the mouse works in the DOS front end, on the 16-bit binary.
#
# The rig clicks the Bugs menu, clicks a toggle, and clicks away to close it,
# then posts the screen the program drew. That covers the whole path: int 33h,
# the edge-detected press, menu hit-testing, and the toggle reaching the
# interpreter -- the Machine pane has to change from ON to off.
#
# Produced by tools/capture/mkmouse.sh plus tools/capture/mouse.html. A
# missing capture skips rather than fails, like the other browser-driven ones.
set -e
cd "$(dirname "$0")/.."

ref=tools/capture/captured/mousedos.txt
want=tests/local/mousedos.expected
[ -f "$ref" ] || { echo "run_mouse_dos: no capture, skipping"; exit 0; }

tr -d '\r' < "$ref" > build/mousedos.out
if diff -u "$want" build/mousedos.out > build/mousedos.diff; then
    echo "run_mouse_dos: the DOS mouse reaches the interpreter"
    exit 0
fi
echo "run_mouse_dos: FAILED"
cat build/mousedos.diff
exit 1
