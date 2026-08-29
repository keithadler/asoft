#!/usr/bin/env python3
"""Encode vectors into an Applesoft shape table.

Each byte holds up to three moves: two that can plot (sections A and B) and a
third that only moves (section C). The packing rules are awkward because a
zero byte ends the shape, so a section cannot always hold what you want:

  - section C cannot hold direction 0, because bits 6-7 of zero means
    "there is no section C"
  - section B cannot hold a non-plotting direction 0 unless section C is
    occupied, for the same reason one level down
  - section A cannot hold a non-plotting direction 0 unless something above
    it is set, or the whole byte would read as the terminator

So this packs greedily and drops back to fewer moves per byte when the next
one will not fit. Emits BASIC DATA statements.
"""
import sys

UP, RIGHT, DOWN, LEFT = 0, 1, 2, 3

def encode(vectors):
    """vectors: list of (plot, direction). Returns a list of bytes."""
    out, i = [], 0
    while i < len(vectors):
        p1, d1 = vectors[i]
        b = (4 if p1 else 0) | d1
        n = 1
        # section B, if the next move fits there
        if i + 1 < len(vectors):
            p2, d2 = vectors[i + 1]
            cand = b | ((4 if p2 else 0) | d2) << 3
            # a non-plotting up-move in B is only safe if C gets filled
            if not (not p2 and d2 == UP):
                b, n = cand, 2
                # section C: movement only, and never direction 0
                if i + 2 < len(vectors):
                    p3, d3 = vectors[i + 2]
                    if not p3 and d3 != UP:
                        b, n = cand | (d3 << 6), 3
        if b == 0:
            raise ValueError("cannot encode a bare non-plotting up-move")
        out.append(b)
        i += n
    out.append(0)          # end of shape
    return out

def table(shapes):
    """Build a full table: count, pad, offsets, then the shape bodies."""
    head = 2 + 2 * len(shapes)
    body, offsets = [], []
    for s in shapes:
        offsets.append(head + len(body))
        body += s
    out = [len(shapes), 0]
    for o in offsets:
        out += [o & 0xFF, (o >> 8) & 0xFF]
    return out + body

def run(v, n, plot=True):
    return [(plot, v)] * n

if __name__ == "__main__":
    # A flag on a pole: asymmetric, so rotation is obvious.
    flag = (run(UP, 14) + run(RIGHT, 9) + run(DOWN, 5) + run(LEFT, 9))
    # A small solid-ish blob, drawn as a zigzag, for XDRAW and collision.
    box = (run(RIGHT, 8) + run(DOWN, 8) + run(LEFT, 8) + run(UP, 8))
    t = table([encode(flag), encode(box)])
    print("shape table: %d bytes, %d shapes" % (len(t), t[0]), file=sys.stderr)
    line = 1000
    for i in range(0, len(t), 16):
        chunk = ",".join(str(b) for b in t[i:i+16])
        print("%d DATA %s" % (line, chunk))
        line += 1
    print("REM total %d" % len(t), file=sys.stderr)
