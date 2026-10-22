#!/usr/bin/env python3
"""Fetch Time-Tick's pinned 0.2 tooling without modifying the legacy checkout."""
import subprocess
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
REVISION = "be151ae047014e79a9c7ae0092dd203844499003"
REPOSITORY = "https://github.com/esp-mosaico/esp-mosaico-utils.git"
CHECKOUT = PROJECT / "build-deps/esp-mosaico-utils"


def main():
    if not CHECKOUT.exists():
        CHECKOUT.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "clone", "--no-checkout", REPOSITORY, str(CHECKOUT)], check=True)
        subprocess.run(["git", "-C", str(CHECKOUT), "checkout", "--detach", REVISION], check=True)
    actual = subprocess.check_output(["git", "-C", str(CHECKOUT), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(CHECKOUT), "status", "--porcelain"], text=True).strip()
    if actual != REVISION or dirty:
        raise SystemExit("Dependency checkout differs from the pinned revision; preserve it and resolve manually.")
    for required in ("mosaico-tools/mosaico.py", "ESP-Iris/components/esp_iris/CMakeLists.txt",
                     "esp-mosaico-vibe-mode/product_contract.json"):
        if not (CHECKOUT / required).is_file():
            raise SystemExit("Dependency checkout is incomplete: " + required)
    print("Pinned Mosaico 0.2 tools ready: " + REVISION)


if __name__ == "__main__":
    main()
