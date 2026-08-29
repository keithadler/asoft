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

# The snapshot ships host binaries for several platforms in parallel
# directories; pick the one that will actually run here. armo64 is Apple
# silicon, bino64 Intel Macs, binl64 Linux.
case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)  HOSTBIN=armo64 ;;
    Darwin-x86_64) HOSTBIN=bino64 ;;
    Linux-*)       HOSTBIN=binl64 ;;
    *)             HOSTBIN=binl64 ;;
esac
test -x "$WATCOM/$HOSTBIN/wcl" || {
    echo "no wcl in $WATCOM/$HOSTBIN - is WATCOM pointing at the unpacked snapshot?" >&2
    exit 1
}
export PATH="$WATCOM/$HOSTBIN:$WATCOM/binl:$PATH"

OUT=web/bundle/ASOFT.EXE
IDE=web/bundle/ASOFTIDE.EXE
mkdir -p web/bundle

# -ml       large model: all pointers far, needed for the 64K memory image
# -k16384   16K stack; the recursive-descent evaluator wants more than the 4K default
CORE="src/mbf.c src/token.c src/a2mem.c src/errs.c src/bugs.c src/gfx.c \
      src/hires.c src/shape.c src/screen.c src/panes.c src/interp.c"

# The console build, with VGA graphics.
wcl -bcl=dos -ml -q -k16384 -fe=$OUT \
    $CORE src/display_dos.c src/main_stdio.c

# The windowed build. Text mode only: it does not wire the graphics display,
# so HGR still writes page memory but nothing switches the screen over.
wcl -bcl=dos -ml -q -k16384 -fe=$IDE \
    $CORE src/tui_dos.c src/ide.c src/main_ide.c

rm -f *.obj src/*.obj 2>/dev/null || true
ls -l $OUT $IDE
