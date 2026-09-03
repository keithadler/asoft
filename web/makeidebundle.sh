#!/bin/sh
# Zip the windowed front end into web/idedemo.jsdos, the bundle idedemo.html
# loads: the same .jsdos/ config the bundle already carries, the freshly
# built ASOFTIDE.EXE, and the sample programs. Stamps the content hash into
# the host page so the browser fetches the new bundle rather than its cache.
set -e
cd "$(dirname "$0")"
test -f bundle/ASOFTIDE.EXE || { echo "bundle/ASOFTIDE.EXE missing - build it first" >&2; exit 1; }
rm -rf idebundle && mkdir idebundle
( cd idebundle && unzip -q ../idedemo.jsdos '.jsdos/*' )
cp bundle/ASOFTIDE.EXE bundle/*.BAS idebundle/
# Same cycle budget as the console bundle.
perl -pi -e 's/^cycles=.*/cycles=200000/; s/^nosound=.*/nosound=false/' idebundle/.jsdos/dosbox.conf
rm -f idedemo.jsdos
( cd idebundle && zip -q -r -X ../idedemo.jsdos .jsdos ./*.EXE ./*.BAS )
rm -rf idebundle
V=$(shasum idedemo.jsdos | cut -c1-12)
perl -pi -e "s|url: \"idedemo.jsdos(\\?v=[a-f0-9]+)?\"|url: \"idedemo.jsdos?v=$V\"|" idedemo.html
echo "idedemo.html now loads idedemo.jsdos?v=$V"
