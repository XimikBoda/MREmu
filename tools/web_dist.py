#!/usr/bin/env python3
"""Copy the built web bundle into the GitHub Pages folder.

`launch.bat 8 1` calls this right after copying the Release bundle to
`bin/Release`, so `web-dist/` always matches the last build. Run it by hand to
refresh the folder from somewhere else:

    python tools/web_dist.py
    python tools/web_dist.py --source build/em --dest some/folder
"""

import argparse
import pathlib
import shutil
import sys

# Everything the page asks for at runtime, plus the GitHub Pages markers. Keep
# this in step with the list in launch.bat's :copy_web_to_bin.
ARTIFACTS = (
    "index.html",
    "index.js",
    "index.wasm",
    "index.data",
    "index.worker.js",
    "mremu-web.js",
    "coi-serviceworker.js",
    ".nojekyll",
)


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="Refresh the static web deploy folder.")
    parser.add_argument("--source", type=pathlib.Path, default=root / "bin" / "Release")
    parser.add_argument("--dest", type=pathlib.Path, default=root / "web-dist")
    args = parser.parse_args()

    if not (args.source / "index.html").is_file():
        print(f"No index.html in {args.source}; build the web bundle first.")
        return 1

    args.dest.mkdir(parents=True, exist_ok=True)
    copied, removed = [], []
    for name in ARTIFACTS:
        src = args.source / name
        dst = args.dest / name
        if src.is_file():
            shutil.copy2(src, dst)
            copied.append(name)
        elif dst.exists():
            # A stale artifact (index.data from an older preloaded bundle) must
            # not ship alongside a build that no longer produces it.
            dst.unlink()
            removed.append(name)

    print(f"web-dist updated from {args.source}: {len(copied)} file(s)")
    if removed:
        print("removed stale: " + ", ".join(removed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
