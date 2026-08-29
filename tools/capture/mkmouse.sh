#!/bin/sh
# Packs the DOS IDE so its mouse and menus can be driven from outside.
#
# There is no pty inside DOSBox and no way to read its text memory from the
# host, so the front end is asked to rewrite OUT.TXT after every redraw with
# -screen. Drive the mouse through js-dos, then read the file back out of the
# emulator's disk and see what the click did.
#
# usage: ./mkmouse.sh [program.bas]   then open mouse.html
set -e
cd "$(dirname "$0")"

PROG=${1:-TESTS.BAS}
test -f ../../web/bundle/ASOFTIDE.EXE || {
    echo "build the DOS IDE first: WATCOM=\$PWD/ow ./build-dos.sh" >&2; exit 1; }
cp ../../web/bundle/ASOFTIDE.EXE bundle/
cp ../../web/bundle/*.BAS bundle/ 2>/dev/null || true

awk '/^\[autoexec\]/ { print; print "mount c ."; print "c:";
                       print "ASOFTIDE.EXE -screen OUT.TXT '"$PROG"'"; skip = 1; next }
     skip && /^\[/ { skip = 0 }
     !skip { print }' bundle/.jsdos/dosbox.conf > bundle/.jsdos/dosbox-mouse.conf

mv bundle/.jsdos/dosbox.conf bundle/.jsdos/dosbox-console.conf
mv bundle/.jsdos/dosbox-mouse.conf bundle/.jsdos/dosbox.conf
rm -f mouse.jsdos bundle/OUT.TXT bundle/DONE.TXT
( cd bundle && zip -q -r -X ../mouse.jsdos .jsdos/jsdos.json .jsdos/dosbox.conf ./*.EXE ./*.BAS )
mv bundle/.jsdos/dosbox-console.conf bundle/.jsdos/dosbox.conf
echo "mouse.jsdos built (program: $PROG)"
