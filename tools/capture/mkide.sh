#!/bin/sh
# Packs the DOS IDE into capture.jsdos so capture.html can draw its screen
# under DOSBox and post the result back. There is no pty inside the emulator
# to read an escape stream from, so the front end is asked to write what it
# drew to a file instead, with -dump.
#
# usage: ./mkide.sh [program.bas]      then open capture.html?name=idedos
set -e
cd "$(dirname "$0")"

PROG=${1:-TESTS.BAS}
test -f ../../web/bundle/ASOFTIDE.EXE || {
    echo "build the DOS IDE first: WATCOM=\$PWD/ow ./build-dos.sh" >&2; exit 1; }
cp ../../web/bundle/ASOFTIDE.EXE bundle/
cp ../../web/bundle/*.BAS bundle/ 2>/dev/null || true

sed 's/^\[autoexec\].*$/[autoexec]/' bundle/.jsdos/dosbox.conf > /dev/null   # sanity
awk '/^\[autoexec\]/ { print; print "mount c ."; print "c:";
                       print "ASOFTIDE.EXE -dump OUT.TXT '"$PROG"'";
                       print "echo done > DONE.TXT"; skip = 1; next }
     skip && /^\[/ { skip = 0 }
     !skip { print }' bundle/.jsdos/dosbox.conf > bundle/.jsdos/dosbox-ide.conf

mv bundle/.jsdos/dosbox.conf bundle/.jsdos/dosbox-console.conf
mv bundle/.jsdos/dosbox-ide.conf bundle/.jsdos/dosbox.conf
rm -f capture.jsdos bundle/OUT.TXT bundle/DONE.TXT
( cd bundle && zip -q -r -X ../capture.jsdos .jsdos/jsdos.json .jsdos/dosbox.conf ./*.EXE ./*.BAS )
mv bundle/.jsdos/dosbox-console.conf bundle/.jsdos/dosbox.conf
echo "capture.jsdos built for the IDE (program: $PROG)"
