#!/usr/bin/env python3
"""Run a pile of third-party Applesoft programs and report what breaks.

This is not a pass/fail suite. These are other people's programs: many are
interactive, many expect a disk, a printer or a machine that is not here, and
stopping early is often the correct behaviour. What it is good for is finding
places where this interpreter refuses something real Applesoft accepted -- a
syntax error in a program that ran on the hardware is a bug here, not there.

The corpus is not part of this repository; it is other people's code under
their own licences. Clone it somewhere and point this at it.

usage: tools/corpus.py <directory> [--seconds N] [--show ERROR]
"""
import argparse, collections, os, re, subprocess, sys

ANSI = re.compile(rb'\x1b\[[0-9;?]*[A-Za-z]')
# Only errors raised inside a running program count. Once a program ends, the
# prompt reads whatever is left of the fed input as commands and rejects it,
# which says nothing about the interpreter.
ERR = re.compile(rb'\?([A-Z][A-Z0-9 \'?]*?(?:ERROR|MEMORY|DATA|SUBSCRIPT|OVERFLOW)) IN (\d+)')
NUMBERED = re.compile(rb'^\s*\d+\s')

def looks_like_basic(path):
    try:
        with open(path, 'rb') as f:
            head = f.read(4096).splitlines()[:40]
    except OSError:
        return False
    return sum(1 for l in head if NUMBERED.match(l)) >= 3

# Something to say at an INPUT prompt. A program asking for a name, a number
# and a yes/no in turn should get something plausible for each rather than end
# of file, which is what stops most of them on the first question.
ANSWERS = [b'1', b'Y', b'A', b'2', b'N', b'5', b'', b'3', b'TEST', b'10']
FEED = b'\n'.join(ANSWERS * 200) + b'\n'

def run(binary, path, seconds, extra=(), feed=False):
    try:
        p = subprocess.run([binary] + list(extra) + ['-r', path],
                           input=FEED if feed else None,
                           stdin=None if feed else subprocess.DEVNULL,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=seconds)
        return p.stdout, False
    except subprocess.TimeoutExpired as e:
        return (e.stdout or b''), True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('directory')
    ap.add_argument('--seconds', type=int, default=8)
    ap.add_argument('--binary', default='build/asoft')
    ap.add_argument('--show', help='list the programs raising this error')
    ap.add_argument('--no-bugs', action='store_true',
                    help='pass -n, turning off the deliberate ROM bugs')
    ap.add_argument('--feed', action='store_true',
                    help='answer INPUT and GET instead of closing stdin, so '
                         'programs get past their prompts')
    args = ap.parse_args()

    files = []
    for root, _, names in os.walk(args.directory):
        for n in sorted(names):
            if n.lower().endswith(('.bas', '.txt', '.a2b')):
                p = os.path.join(root, n)
                if looks_like_basic(p):
                    files.append(p)

    tally = collections.Counter()
    by_error = collections.defaultdict(list)
    clean = timeouts = 0

    for p in sorted(files):
        out, timed_out = run(args.binary, p, args.seconds,
                             ('-n',) if args.no_bugs else (), args.feed)
        out = ANSI.sub(b'', out)
        errs = sorted({m.group(1).decode() for m in ERR.finditer(out)})
        name = os.path.relpath(p, args.directory)
        if errs:
            for e in errs:
                tally[e] += 1
                by_error[e].append(name)
        elif timed_out:
            timeouts += 1
        else:
            clean += 1

    print("%d programs: %d ran clean, %d raised an error, %d still running at %ds"
          % (len(files), clean, sum(1 for e in by_error for _ in [0]) and
             len({n for v in by_error.values() for n in v}) or 0,
             timeouts, args.seconds))
    print()
    print("errors by kind:")
    for err, n in tally.most_common():
        print("  %-28s %3d" % (err, n))
    if args.show:
        print()
        print("programs raising %s:" % args.show)
        for n in by_error.get(args.show, []):
            print("  " + n)

if __name__ == '__main__':
    main()
