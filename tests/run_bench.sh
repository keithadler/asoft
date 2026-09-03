#!/bin/sh
# The benchmark: BENCH.BAS through the host build with -b, which runs it
# flat out and reports statements a second. The number is printed for the
# record; the check is a floor, low enough that machine-to-machine variation
# never trips it and high enough that a ten-fold slowdown would. The same
# program under DOSBox is how the DOS requirements in the README were
# measured; see web/bench.html.
set -e
cd "$(dirname "$0")/.."
line=$(./build/asoft -b web/bundle/BENCH.BAS < /dev/null | grep 'A SECOND')
rate=$(echo "$line" | sed 's/.*S: \([0-9]*\) A SECOND/\1/')
echo "run_bench: $line"
if [ "$rate" -lt 100000 ]; then
    echo "run_bench: FAILED - under 100000 statements a second on the host"
    exit 1
fi
