#!/usr/bin/env python3
"""Generate the bilingual directory cards with the pinned official renderer.

Requires shared/ideas/requirements.txt and system libraqm (Linux/WSL supported).
Normal packaging uses the checked-in prebuilt cards and needs no font renderer.
"""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build-deps/esp-mosaico-utils"))
from shared.ideas import generate_resources

font = ROOT / "build-font/NotoSansCJKsc-Regular.otf"
expected = json.loads((ROOT / "publish/sources.json").read_text(encoding="utf-8"))["fonts"]["sha256"]
if hashlib.sha256(font.read_bytes()).hexdigest() != expected:
    raise SystemExit("Noto source font checksum mismatch")
content = json.loads((ROOT / "publish/device-text.json").read_text(encoding="utf-8"))
for locale, text in content.items():
    resources = generate_resources(locale=locale, font=font, **text)
    directory = ROOT / "publish/device" / locale
    directory.mkdir(parents=True, exist_ok=True)
    for name, data in resources.items():
        (directory / name).write_bytes(data)
    print(locale, sum(map(len, resources.values())), "bytes")
