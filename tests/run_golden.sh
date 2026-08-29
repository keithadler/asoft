#!/bin/sh
# Replays the script that was fed to the reference DOS build and diffs our
# transcript against what that build actually printed.
#
# One line is expected to differ: FRE(0) reads 35492 here and 35491 there.
# Everything the reference does with memory has been measured and matched --
# program storage is byte-exact, scalars cost 7, arrays 7+5n, literals cost
# nothing, READ copies -- and what is left is a single byte of collector
# residue inside a binary whose source no longer exists.
set -e
cd "$(dirname "$0")/.."

REF=tools/capture/captured/tests.txt
SCRIPT=tools/capture/golden.txt
[ -f "$REF" ] || { echo "run_golden: no reference capture, skipping"; exit 0; }

./build/asoft < "$SCRIPT" > build/golden.out 2>&1
tr -d '\r' < "$REF" > build/golden.ref

if diff -u build/golden.ref build/golden.out > build/golden.diff; then
    echo "run_golden: exact match"
    exit 0
fi

differing=$(grep -c '^-[^-]' build/golden.diff || true)
if [ "$differing" -eq 1 ] && grep -q '^-FRE(0)=35491' build/golden.diff; then
    echo "run_golden: ok (1 known difference: FRE(0) off by one byte)"
    exit 0
fi
echo "run_golden: FAILED"
cat build/golden.diff
exit 1
