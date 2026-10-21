#!/usr/bin/env python3
"""Serve an Emscripten build of MREmu for local testing.

The emulator is compiled with -pthread, so it needs SharedArrayBuffer, which
browsers only expose to cross-origin isolated documents. Opening index.html
straight from disk (file://) sends no headers at all, so the wasm build fails
with "SharedArrayBuffer is not defined". This server adds the COOP/COEP pair
that makes the page cross-origin isolated.

Usage:
    python tools/web_serve.py [--dir build/em] [--port 8099]
"""

from __future__ import annotations

import argparse
import functools
import http.server
import os
import sys

# Python's mimetypes does not know these on every platform; without them the
# browser refuses to stream-compile the module. .data is kept for builds that
# add a preload and emit index.data.
EXTRA_TYPES = {
    ".wasm": "application/wasm",
    ".data": "application/octet-stream",
    ".js": "text/javascript",
    ".mjs": "text/javascript",
}


class WebHandler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        **EXTRA_TYPES,
    }

    def end_headers(self) -> None:
        # Cross-origin isolation: required for SharedArrayBuffer / pthreads.
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dir",
        default=os.path.join("build", "em"),
        help="directory holding index.html/.js/.wasm/.data (default: build/em)",
    )
    parser.add_argument("--port", type=int, default=8099)
    parser.add_argument("--bind", default="127.0.0.1")
    args = parser.parse_args()

    directory = os.path.abspath(args.dir)
    if not os.path.exists(os.path.join(directory, "index.html")):
        print(f"No index.html in {directory}. Build the web target first.", file=sys.stderr)
        return 1

    handler = functools.partial(WebHandler, directory=directory)
    with http.server.ThreadingHTTPServer((args.bind, args.port), handler) as httpd:
        print(f"Serving {directory} at http://{args.bind}:{args.port}/ (Ctrl+C to stop)")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
