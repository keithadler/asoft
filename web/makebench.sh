#!/bin/sh
# Bundles for bench.html: ASOFT.EXE -b BENCH.BAS at a fixed DOSBox cycle
# count each, so the same workload can be timed at the speed of a 286, a
# 386 and a 486. The result line stays on the DOS screen.
set -e
cd "$(dirname "$0")"
for c in ${*:-3000 20000 200000}; do
    rm -rf benchb && mkdir -p benchb/.jsdos
    cp bundle/.jsdos/jsdos.json benchb/.jsdos/
    sed "s/^cycles=.*/cycles=$c/; s/^ASOFT.EXE.*/ASOFT.EXE -b BENCH.BAS/" bundle/.jsdos/dosbox.conf > benchb/.jsdos/dosbox.conf
    cp bundle/ASOFT.EXE bundle/BENCH.BAS benchb/
    rm -f bench-$c.jsdos
    ( cd benchb && zip -q -r -X ../bench-$c.jsdos .jsdos ./*.EXE ./*.BAS )
    rm -rf benchb
    echo "bench-$c.jsdos"
done
