#!/bin/sh
# Cross-compile the console build to a 16-bit DOS executable with Open Watcom.
# Verified working: Open Watcom 2.0 beta, target DOS, large model.
#
# Get the toolchain (no install needed, just unpack):
#   curl -LO https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz
#   mkdir -p ow && tar -xJf ow-snapshot.tar.xz -C ow
#
# Then:  WATCOM=$PWD/ow ./build-dos.sh

set -e
: "${WATCOM:?set WATCOM to the unpacked Open Watcom directory}"
export WATCOM
export INCLUDE="$WATCOM/h"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"

OUT=web/bundle/ASOFT.EXE
mkdir -p web/bundle

# -ml       large model: all pointers far, needed for the 64K memory image
# -k16384   16K stack; the recursive-descent evaluator wants more than the 4K default
wcl -bcl=dos -ml -q -k16384 -fe=$OUT \
    src/mbf.c src/token.c src/a2mem.c src/interp.c src/main_stdio.c

rm -f *.obj src/*.obj 2>/dev/null || true
ls -l $OUT
