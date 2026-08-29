#!/bin/sh
# Zip web/bundle/ into web/asoft.jsdos, the bundle js-dos loads.
#
# A .jsdos bundle is just a zip whose root holds .jsdos/ (dosbox.conf +
# jsdos.json) alongside the DOS files. Paths must be relative to the bundle
# root, so the zip is built from inside bundle/ rather than above it.

set -e
cd "$(dirname "$0")"

test -f bundle/ASOFT.EXE || {
    echo "bundle/ASOFT.EXE missing - build it first (see README)" >&2
    exit 1
}

rm -f asoft.jsdos
# -X drops the extra file attributes; keeps the bundle byte-stable between runs.
( cd bundle && zip -q -r -X ../asoft.jsdos .jsdos ./*.EXE ./*.BAS )

unzip -l asoft.jsdos
