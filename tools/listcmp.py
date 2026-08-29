#!/usr/bin/env python3
"""Compare our LIST output against the reference build's captured transcript.

The capture comes off a 40-column screen, so any line the interpreter printed
that was longer than 40 characters arrives split across several records. A
record of exactly 40 characters is a continuation, so glue it to the next one
before trying to read line numbers off the front.
"""
import re
import subprocess
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
CAP = ROOT / "tools/capture/captured/tests.txt"


def unwrap(records, width=40):
    out, buf = [], ""
    for r in records:
        buf += r
        if len(r) != width:
            out.append(buf)
            buf = ""
    if buf:
        out.append(buf)
    return out


def reference_listing():
    raw = CAP.read_bytes().decode("latin1")
    records = raw.split("\r\n")
    # The listing runs from the record holding "10  REM" to the one before the
    # RUN output, which starts at the prompt followed by FLOATS:.
    start = next(i for i, r in enumerate(records) if "10  REM" in r)
    records = records[start:]
    records[0] = records[0][records[0].index("10  REM"):]
    end = next(i for i, r in enumerate(records) if r.startswith("]"))
    records = records[:end]
    return unwrap(records)


def ours():
    exe = subprocess.run([str(ROOT / "build/listdump")], capture_output=True, text=True)
    if exe.returncode != 0:
        sys.exit("listdump failed: " + exe.stderr)
    return exe.stdout.split("\n")[:-1]


ref = reference_listing()
mine = ours()
bad = 0
for i in range(max(len(ref), len(mine))):
    r = ref[i] if i < len(ref) else "<missing>"
    m = mine[i] if i < len(mine) else "<missing>"
    if r != m:
        bad += 1
        print("line %d differs:\n  ref [%s]\n  got [%s]" % (i, r, m))
print("listcmp: %d of %d lines differ" % (bad, len(ref)))
sys.exit(1 if bad else 0)
