#!/usr/bin/env python3
"""Drive the windowed front end in a pty and render what it drew.

The IDE only ever emits a small subset of ANSI -- absolute cursor moves, an
SGR pair for colour, and the alternate-screen and cursor-visibility toggles --
so parsing it back into a grid is straightforward, and gives a deterministic
picture of the layout instead of a screenshot to squint at.

usage: idegrab.py [keys ...]
       keys are literal text, or \\e \\r \\t, or names like F10 UP DOWN ENTER ESC
"""
import os, pty, re, select, sys, time

W, H = 80, 43

NAMED = {
    "F1": "\x1bOP", "F2": "\x1bOQ", "F3": "\x1bOR", "F4": "\x1bOS",
    "F5": "\x1b[15~", "F9": "\x1b[20~", "F10": "\x1b[21~",
    "UP": "\x1b[A", "DOWN": "\x1b[B", "RIGHT": "\x1b[C", "LEFT": "\x1b[D",
    "PGUP": "\x1b[5~", "PGDN": "\x1b[6~",
    "ENTER": "\r", "ESC": "\x1b", "BS": "\x7f", "CTRLQ": "\x11", "CTRLC": "\x03",
    "TAB": "\t", "F8": "\x1b[19~", "DEL": "\x1b[3~", "HOME": "\x1b[H", "END": "\x1b[F",
}

def keys_from(args):
    """CLICK:x,y sends a left button press at that cell, in the SGR encoding
    the front end asks for. Coordinates are zero-based here and one-based on
    the wire, like the terminal itself.

    WAIT:n sends nothing and just reads for n seconds, which is how a test
    waits for a program that is still running rather than for a keystroke to
    be answered."""
    out = []
    for a in args:
        if a.startswith("WAIT:"):
            out.append(("wait", float(a[5:])))
        elif a.startswith("CLICK:"):
            x, y = (int(v) for v in a[6:].split(","))
            out.append("\x1b[<0;%d;%dM" % (x + 1, y + 1))
        elif a in NAMED:
            out.append(NAMED[a])
        else:
            out.append(a.replace("\\e", "\x1b").replace("\\r", "\r").replace("\\t", "\t"))
    return out

def run(argv, keys, settle=None):
    # How long to wait after each key before sending the next. Escape
    # sequences arrive in pieces, so a front end that is busy can still be
    # mid-sequence when the next key lands; tests that drive a running
    # program raise this.
    if settle is None:
        settle = float(os.environ.get("IDEGRAB_SETTLE", "0.35"))
    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "xterm-256color"
        os.execv(argv[0], argv)
    buf = b""
    def drain(t):
        nonlocal buf
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.05)
            if r:
                try:
                    d = os.read(fd, 65536)
                except OSError:
                    return False
                if not d:
                    return False
                buf += d
        return True
    drain(settle)
    for k in keys:
        if isinstance(k, tuple):        # ("wait", seconds)
            if not drain(k[1]):
                break
            continue
        os.write(fd, k.encode())
        if not drain(settle):
            break
    try:
        os.write(fd, b"\x11")      # Ctrl-Q, so it exits cleanly
        drain(0.3)
    except OSError:
        pass
    try:
        os.close(fd)
    except OSError:
        pass
    os.waitpid(pid, os.WNOHANG)
    return buf.decode("utf-8", "replace")

def render(s):
    grid = [[" "] * W for _ in range(H)]
    attr = [[0] * W for _ in range(H)]
    x = y = 0
    cur = 0
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\x1b":
            m = re.match(r"\x1b\[([0-9;?]*)([A-Za-z])", s[i:])
            if not m:
                i += 1
                continue
            body, fin = m.group(1), m.group(2)
            if fin == "H":
                parts = body.split(";")
                y = (int(parts[0]) - 1) if parts[0] else 0
                x = (int(parts[1]) - 1) if len(parts) > 1 and parts[1] else 0
            elif fin == "m":
                nums = [int(n) for n in body.split(";") if n.isdigit()]
                if nums:
                    cur = nums[0] * 1000 + (nums[1] if len(nums) > 1 else 0)
            i += m.end()
            continue
        if c == "\n":
            y += 1; x = 0; i += 1; continue
        if c == "\r":
            x = 0; i += 1; continue
        if 0 <= y < H and 0 <= x < W:
            grid[y][x] = c
            attr[y][x] = cur
        x += 1
        i += 1
    return grid, attr

if __name__ == "__main__":
    out = run(["build/asoft-ide"], keys_from(sys.argv[1:]))
    grid, _ = render(out)
    print("+" + "-" * W + "+")
    for row in grid:
        print("|" + "".join(row) + "|")
    print("+" + "-" * W + "+")
