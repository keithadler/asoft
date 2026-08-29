#!/bin/sh
# Diffs a script's output against an expectation written here rather than
# captured from the reference binary. Used only where this interpreter
# deliberately follows real Applesoft instead of the reference -- currently
# just NEXT with a list of variables, which real machines accept and the
# reference does not. See README.
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
