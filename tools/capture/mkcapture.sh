#!/bin/sh
# usage: ./mkcapture.sh script.txt   -> builds capture.jsdos with that script
set -e
cd "$(dirname "$0")"
test -n "$1" || { echo "usage: $0 <script.txt>" >&2; exit 1; }
# DOS wants CRLF and a trailing newline on the last command.
awk '{ sub(/\r$/,""); printf "%s\r\n", $0 }' "$1" > bundle/SCRIPT.TXT
rm -f capture.jsdos bundle/OUT.TXT bundle/DONE.TXT
( cd bundle && zip -q -r -X ../capture.jsdos .jsdos ./*.EXE ./*.BAS ./*.TXT )
echo "capture.jsdos: $(unzip -l capture.jsdos | tail -1)"
