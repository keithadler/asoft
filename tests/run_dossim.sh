#!/bin/sh
# Runs the DOS front end on the host -- console_dos.c and display_dos.c
# against the BIOS and keyboard stand-ins in tests/dosshim -- through the
# scenario in tests/dossim.txt, and diffs what landed in text video memory
# and which modes were set against tests/dossim.expected. The graphics
# frames are written to build/dossim-*.ppm for looking at; only the text
# screens and the mode sequence are pinned here.
set -e
cd "$(dirname "$0")/.."

rm -f printer.txt
./build/dossim tests/dossim.txt build > build/dossim.out 2>&1
# PR#1 on the host appends to printer.txt where the run happens; the
# scenario prints one line to it, which is checked and then tidied away.
if grep -q "TO PRINTER" printer.txt 2>/dev/null; then
    echo "--- printer.txt: TO PRINTER" >> build/dossim.out
else
    echo "--- printer.txt: MISSING" >> build/dossim.out
fi
rm -f printer.txt
if diff -u tests/dossim.expected build/dossim.out > build/dossim.diff; then
    echo "run_dossim: the DOS screen matches"
    exit 0
fi
echo "run_dossim: FAILED"
cat build/dossim.diff
exit 1
