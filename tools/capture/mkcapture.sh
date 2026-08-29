#!/bin/sh
# usage: ./mkcapture.sh script.txt [asoft.exe]
#
# Packs a script into capture.jsdos so capture.html can run it under DOSBox.
# The binary defaults to the original reference build, which is the point of
# the rig; pass ../../web/bundle/ASOFT.EXE to run the same script through our
# own DOS build instead and check the two against each other.
set -e
cd "$(dirname "$0")"
test -n "$1" || { echo "usage: $0 <script.txt> [asoft.exe]" >&2; exit 1; }

EXE=${2:-../../reference/ASOFT-watcom-reference.EXE}
test -f "$EXE" || { echo "no such binary: $EXE" >&2; exit 1; }
cp "$EXE" bundle/ASOFT.EXE

# DOS wants CRLF and a newline on the last command.
awk '{ sub(/\r$/,""); printf "%s\r\n", $0 }' "$1" > bundle/SCRIPT.TXT
rm -f capture.jsdos bundle/OUT.TXT bundle/DONE.TXT
( cd bundle && zip -q -r -X ../capture.jsdos .jsdos ./*.EXE ./*.BAS ./*.TXT )
echo "capture.jsdos built from $(basename "$EXE")"
