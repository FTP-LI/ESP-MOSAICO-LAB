"""Collect unchanged upstream license notices for the locked build inputs."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sources = [
    root / 'build-deps/esp-mosaico-utils/LICENSES.md',
    root / 'build-deps/esp-mosaico-utils/ESP-Iris/components/esp_iris/LICENSE',
    root / 'build-deps/esp-mosaico-bsp/LICENSE',
    root / 'docs/licenses/SourceHanSansSC-OFL.txt',
]
for component in sorted((root / 'managed_components').iterdir()):
    if component.is_dir():
        sources += sorted(p for p in component.iterdir() if p.is_file() and p.name.upper().startswith(('LICENSE','LICENCE','COPYING','NOTICE')))
sections = ['Third-party license notices for Desk-Deck 0.1.0-dev.\nExact components are locked in dependencies.lock and project.json.\nNo license grant is added by collecting these notices.\n']
for path in sources:
    sections.append('\n' + '=' * 72 + '\n' + path.relative_to(root).as_posix() + '\n' + '=' * 72 + '\n' + path.read_text(encoding='utf-8', errors='replace'))
(root / 'docs/licenses/THIRD-PARTY-NOTICES.txt').write_text('\n'.join(sections), encoding='utf-8')
print(len(sources), 'upstream notices collected')
