#!/bin/sh
# Replays a script through our interpreter and diffs the transcript against
# what the reference DOS build printed for the same script.
#
# The captures live in tools/capture/captured/ and are produced by running
# reference/ASOFT-watcom-reference.EXE under js-dos; see tools/capture/README.
# Missing captures skip rather than fail, so the suite still runs on a machine
# that has never opened the browser rig.
#
# tests: exactly one line is expected to differ, FRE(0) reading 35492 against
# the reference's 35491 -- one byte of collector residue, documented in
# README.md. Everything else must match byte for byte.
set -e
cd "$(dirname "$0")/.."

name=$1
allow=$2
ref="tools/capture/captured/$name.txt"
script="tools/capture/$name.txt"

[ -f "$ref" ] || { echo "run_capture $name: no capture, skipping"; exit 0; }

./build/asoft < "$script" > "build/$name.out" 2>&1
tr -d '\r' < "$ref" > "build/$name.ref"

if diff -u "build/$name.ref" "build/$name.out" > "build/$name.diff"; then
    echo "run_capture $name: exact match"
    exit 0
fi

n=$(grep -c '^-[^-]' "build/$name.diff" || true)
if [ -n "$allow" ] && [ "$n" -eq 1 ] && grep -q "^-$allow" "build/$name.diff"; then
    echo "run_capture $name: ok (1 known difference: $allow)"
    exit 0
fi
echo "run_capture $name: FAILED ($n lines differ)"
cat "build/$name.diff"
exit 1
