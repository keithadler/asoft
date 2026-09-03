#!/bin/sh
# Diffs a script's output against an expectation written here rather than
# captured from the reference binary. Used where this interpreter follows
# real Applesoft instead of the reference (NEXT with a list of variables;
# the ROM's FOR, LOMEM:, HIMEM: and array rules in rom.txt, checked against
# the genuine ROM by tools/diff), and for things the reference never did at
# all (LOAD and SAVE in apple.txt, the file formats LOAD accepts in
# formats.txt). See README.
set -e
cd "$(dirname "$0")/.."

name=$1
script=tests/local/$name.txt
want=tests/local/$name.expected

./build/asoft < "$script" 2>&1 \
    | LC_ALL=C sed 's/\x1b\[[0-9;]*m//g' > "build/$name.local"

if diff -u "$want" "build/$name.local" > "build/$name.localdiff"; then
    echo "run_local $name: ok"
    exit 0
fi
echo "run_local $name: FAILED"
cat "build/$name.localdiff"
exit 1
