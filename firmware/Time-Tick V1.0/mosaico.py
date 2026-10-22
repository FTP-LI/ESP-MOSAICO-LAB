#!/usr/bin/env python3
"""Consuming-workspace entry point for the pinned official Mosaico CLI."""
from pathlib import Path
import runpy
import sys

project = Path(__file__).resolve().parent
launcher = project / "build-deps/esp-mosaico-utils/mosaico-tools/mosaico.py"
if not launcher.is_file():
    raise SystemExit("Run python tools/prepare_dependencies.py first.")
if "--workspace" not in sys.argv:
    sys.argv[1:1] = ["--workspace", str(project)]
runpy.run_path(str(launcher), run_name="__main__")
