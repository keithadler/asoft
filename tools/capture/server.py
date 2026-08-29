#!/usr/bin/env python3
"""Static server for the capture page that also accepts the transcript back.

The page POSTs OUT.TXT to /_capture once DONE.TXT appears inside DOSBox; we
write it to captured/<name>.txt so the build loop can diff against it without
round-tripping the text through the browser console.
"""
import http.server, os, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parent
OUT = ROOT / "captured"
OUT.mkdir(exist_ok=True)


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(ROOT), **kw)

    def do_POST(self):
        if not self.path.startswith("/_capture"):
            self.send_error(404)
            return
        name = self.headers.get("X-Capture-Name", "capture")
        name = "".join(c for c in name if c.isalnum() or c in "-_") or "capture"
        n = int(self.headers.get("Content-Length", 0))
        (OUT / (name + ".txt")).write_bytes(self.rfile.read(n))
        self.send_response(204)
        self.end_headers()

    def end_headers(self):
        # the bundle changes on every iteration; never let the browser cache it
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8130
    http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
