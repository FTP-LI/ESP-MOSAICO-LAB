#!/usr/bin/env python3
"""Use the official pinned CLI for every Desk-Deck device operation."""
from pathlib import Path
import os
import runpy
import sys

os.environ.setdefault("PYTHONUTF8", "1")
for stream in (sys.stdout, sys.stderr):
    if hasattr(stream, "reconfigure"):
        stream.reconfigure(encoding="utf-8", errors="replace")

root = Path(__file__).resolve().parent
entry = root / "build-deps/esp-mosaico-utils/mosaico-tools/mosaico.py"
if not entry.is_file():
    raise SystemExit("Run python tools/prepare_dependencies.py first.")
if "--workspace" not in sys.argv:
    sys.argv[1:1] = ["--workspace", str(root)]
runpy.run_path(str(entry), run_name="__main__")
