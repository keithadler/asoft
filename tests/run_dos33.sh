#!/bin/sh
# The DOS 3.3 command channel, in a directory of its own: text files
# written and read back through PRINT and INPUT, END OF DATA reaching
# ONERR as code 5, APPEND, GET, POSITION, BSAVE and BLOAD with DOS's
# header, RENAME, DELETE, CATALOG, LOAD and RUN through the channel, the
# commands typed at the prompt, FILE NOT FOUND, and PR#1 to the printer
# (printer.txt on the host). The transcript and the files left behind are
# both diffed.
set -e
cd "$(dirname "$0")/.."
root=$PWD
dir=build/dos33
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

"$root/build/asoft" < "$root/tests/dos33.txt" 2>&1 \
    | LC_ALL=C sed 's/\x1b\[[0-9;]*m//g' > ../dos33-transcript.txt
{ echo "--- transcript"; cat ../dos33-transcript.txt
  echo "--- SCORES.TXT"; cat SCORES.TXT
  echo "--- printer.txt"; cat printer.txt
  echo "--- files"; ls | sort; } > ../dos33-all.txt

if diff -u "$root/tests/dos33.expected" ../dos33-all.txt > ../dos33.diff; then
    echo "run_dos33: the command channel matches"
    exit 0
fi
echo "run_dos33: FAILED"
cat ../dos33.diff
exit 1
