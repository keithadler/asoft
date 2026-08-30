#!/usr/bin/env python3
"""Differential harness: run the corpus through asoft and through the real
Applesoft ROM (bobbin, an Apple ][+ emulator), and diff the output.

    BOBBIN=/path/to/bobbin python3 tools/diff/run.py

bobbin's "simple" interface taps the character stream before the 40-column
screen model, so it never wraps and never pads comma zones; asoft's console
goes through its screen model and does both. The normalizer therefore
collapses runs of spaces and strips trailing blanks, and the corpus keeps
every output line under 40 columns and probes layout through POS(0) values
rather than through visual spacing.
"""
import os, pathlib, re, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CORPUS = HERE / "corpus"
OUT = HERE / "out"

ASOFT = os.environ.get("ASOFT", str(ROOT / "build" / "asoft"))
BOBBIN = os.environ.get("BOBBIN", "bobbin")


def normalize(text):
    lines = []
    for ln in text.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        ln = re.sub(r" +", " ", ln.rstrip())
        if ln == "]":            # a bare prompt at EOF is not program output
            continue
        if ln == "":             # blank lines are screen positioning, which
            continue             # the serial tap reports inconsistently
        lines.append(ln)
    while lines and lines[-1] == "":
        lines.pop()
    return "\n".join(lines) + "\n" if lines else ""


def run_asoft(bas):
    p = subprocess.run([ASOFT, "-r", "-f", str(bas)], stdin=subprocess.DEVNULL,
                       capture_output=True, text=True, timeout=120)
    return p.stdout


def run_rom(bas):
    feed = bas.read_text() + "\nRUN\n"
    p = subprocess.run([BOBBIN, "-m", "plus", "--simple", "--turbo"],
                       input=feed, capture_output=True, text=True, timeout=120)
    return p.stdout


def main():
    OUT.mkdir(exist_ok=True)
    names = sorted(CORPUS.glob("*.bas"))
    if not names:
        sys.exit("no corpus")
    passed, failed = [], []
    for bas in names:
        name = bas.stem
        try:
            a = normalize(run_asoft(bas))
            r = normalize(run_rom(bas))
        except subprocess.TimeoutExpired as e:
            failed.append((name, "TIMEOUT: " + str(e.cmd[0])))
            continue
        (OUT / (name + ".asoft.txt")).write_text(a)
        (OUT / (name + ".rom.txt")).write_text(r)
        if a == r:
            passed.append(name)
        else:
            d = subprocess.run(["diff", "-u", str(OUT / (name + ".rom.txt")),
                                str(OUT / (name + ".asoft.txt"))],
                               capture_output=True, text=True)
            (OUT / (name + ".diff")).write_text(d.stdout)
            failed.append((name, d.stdout))

    print("== %d/%d programs identical to the ROM ==" % (len(passed), len(names)))
    for n in passed:
        print("  ok   " + n)
    for n, d in failed:
        print("  DIFF " + n)
    if failed:
        print("\nfirst differing program in full:\n")
        print(failed[0][1][:3000])
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
