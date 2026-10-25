"""Generate bilingual Mosaico cards using the pinned official renderer (WSL)."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'build-deps/esp-mosaico-utils'))
from shared.ideas import generate_resources

font = root / 'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'
text = json.loads((root / 'publish/device-text.json').read_text(encoding='utf-8'))
for locale, content in text.items():
    resources = generate_resources(locale=locale, font=font, **content)
    directory = root / 'publish/device' / locale
    directory.mkdir(parents=True, exist_ok=True)
    for name, data in resources.items():
        (directory / name).write_bytes(data)
    print(locale, sum(map(len, resources.values())), 'bytes')
