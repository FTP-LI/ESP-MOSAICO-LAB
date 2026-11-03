"""Obtain independent, pinned official dependencies without touching other apps."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    "esp-mosaico-utils": "be151ae047014e79a9c7ae0092dd203844499003",
    "esp-mosaico-bsp": "5f88b91e1be055d1d3674364123b2779a1758377",
}

for name, revision in PINS.items():
    path = ROOT / "build-deps" / name
    url = f"https://github.com/esp-mosaico/{name}.git"
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "clone", "--no-checkout", url, str(path)], check=True)
        subprocess.run(["git", "-C", str(path), "checkout", "--detach", revision], check=True)
    actual = subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(path), "status", "--porcelain"], text=True).strip()
    if actual != revision or dirty:
        raise SystemExit(f"Preserve and inspect modified dependency: {path}")
    print(f"{name}: {actual}")
