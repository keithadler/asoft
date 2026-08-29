#!/usr/bin/env python3
"""Run a command under a pty and send it keys, then print what it wrote.

The console build behaves differently when its input is a terminal rather
than a file -- INPUT does not echo, and the keyboard can be polled without
waiting -- so testing that behaviour needs a real pty, which a pipe cannot
provide.

usage: ptyrun.py [--settle S] -- command args...  <<< keys
       keys are read from stdin: literal text, or WAIT:n to just read.
"""
import os, pty, select, sys, time

def main():
    settle = 0.4
    args = sys.argv[1:]
    if args and args[0] == "--settle":
        settle = float(args[1]); args = args[2:]
    if args and args[0] == "--":
        args = args[1:]
    if not args:
        sys.exit("usage: ptyrun.py [--settle S] -- command args... <<< keys")

    steps = [l for l in sys.stdin.read().split("\n")]

    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "dumb"
        os.execvp(args[0], args)

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
    for s in steps:
        if s.startswith("WAIT:"):
            if not drain(float(s[5:])):
                break
            continue
        os.write(fd, (s + "\n").encode())
        if not drain(settle):
            break
    try:
        os.close(fd)
    except OSError:
        pass
    os.waitpid(pid, os.WNOHANG)
    sys.stdout.write(buf.decode("utf-8", "replace"))

if __name__ == "__main__":
    main()
