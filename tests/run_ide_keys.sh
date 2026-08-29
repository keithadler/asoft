#!/bin/sh
# The keyboard as a soft switch, in the windowed front end.
#
# GET stops and waits, which is no use to a game. Programs poll $C000 instead,
# and that only works if the front end can answer a poll without blocking its
# own event loop. This runs a program that spins on PEEK(-16384), sends it a
# key, and requires the program to see it: the transcript has to say GOT Z,
# not NO KEY SEEN. The FOR bounds the wait so a failure ends the test rather
# than hanging it -- long enough that the key arrives while the program is
# still spinning, short enough that a failure gives up in a few seconds. The
# WAIT at the end reads without sending anything, so the program's output is
# captured rather than the screen as it stood before RUN.
set -e
cd "$(dirname "$0")/.."

out=build/ide-keys.out
python3 tools/idegrab.py \
    '10 FOR T = 1 TO 1500000' ENTER \
    '20 K = PEEK(-16384)' ENTER \
    '30 IF K > 127 THEN 60' ENTER \
    '40 NEXT T' ENTER \
    '50 PRINT "NO KEY SEEN": END' ENTER \
    '60 POKE -16368,0: PRINT "GOT ";CHR$(K-128)' ENTER \
    '70 END' ENTER \
    F5 'Z' WAIT:15 \
    > "$out"

if ! grep -aq "GOT Z" "$out"; then
    echo "run_ide_keys: FAILED - the program never saw the key"
    grep -aE "NO KEY|RUN" "$out" | head -4
    exit 1
fi
echo "run_ide_keys: the keyboard strobe reaches a running program"
