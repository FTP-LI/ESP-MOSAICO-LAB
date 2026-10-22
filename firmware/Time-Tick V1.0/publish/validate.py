#!/usr/bin/env python3
"""Offline checks for Time-Tick's Mosaico Ideas framework; never uploads or flashes."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]


def check_layout(layout, contract):
    """Preserve Vibe's protected region and reject overlapping/out-of-range partitions."""
    for name, expected in contract["immutable_layout"].items():
        if name not in layout or list(layout[name]) != expected:
            raise ValueError(f"Protected partition differs from the official contract: {name}")
    end = contract["partition_table_offset"] + contract["partition_table_region_bytes"]
    for name, part in sorted(layout.items(), key=lambda row: row[1].offset):
        alignment = 0x10000 if part.type == "app" else 0x1000
        if part.offset % alignment or part.size <= 0 or part.size % 0x1000:
            raise ValueError(f"Invalid partition alignment or size: {name}")
        if part.offset < end or part.offset + part.size > 16 * 1024 * 1024:
            raise ValueError(f"Partition overlaps or exceeds 16 MiB: {name}")
        end = part.offset + part.size
    app = layout.get("main_app")
    if not app or (app.type, app.subtype, app.flags) != ("app", "ota_0", ""):
        raise ValueError("An unencrypted app/ota_0 application partition is required")
    if app.offset < contract["application_region_start"]:
        raise ValueError("Application overlaps the protected Mosaico region")
    resources = [name for name, p in layout.items()
                 if p.offset >= contract["application_region_start"]
                 and p.type == "data" and p.subtype not in ("nvs", "0x02", "2")]
    if resources:
        raise ValueError("Add and verify data-image build rules before publishing resource partitions: "
                         + ", ".join(resources))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, help="Inspect a real .irisfw and its build configuration")
    parser.add_argument("--build-dir", type=Path, default=PROJECT / "build")
    args = parser.parse_args()
    config = json.loads((PROJECT / ".mosaico.json").read_text(encoding="utf-8"))
    iris = (PROJECT / config["dependencies"]["esp_iris"]).resolve()
    utils = iris.parent
    tools = utils / "mosaico-tools"
    if not (tools / "tools/mosaico_cli/ideas_archive.py").is_file():
        raise ValueError("Initialize the pinned official Vibe checkout; see publish/README.md")
    pins = json.loads((PROJECT / "publish/sources.json").read_text(encoding="utf-8"))
    revision = subprocess.check_output(
        ["git", "-C", str(utils), "rev-parse", "HEAD"], text=True).strip()
    if revision != pins["utils"]["commit"]:
        raise ValueError("Official utils commit differs from publish/sources.json")
    sys.path.insert(0, str(tools / "tools"))
    from jsonschema import Draft202012Validator
    from mosaico_cli.ideas_firmware import validate_firmware
    from mosaico_cli.iris import load_bundle
    from mosaico_cli.product_contract import PRODUCT_CONTRACT, validate_application_config
    from mosaico_cli.workspace import load_workspace
    workspace = load_workspace(tools, start=PROJECT, explicit=str(PROJECT))
    if workspace.default_project != PROJECT:
        raise ValueError("The workspace must select this application")
    metadata = json.loads((PROJECT / "mosaico-ideas.json").read_text(encoding="utf-8"))
    schema = json.loads((tools / "mosaico-ideas.schema.json").read_text(encoding="utf-8"))
    Draft202012Validator(schema).validate(metadata)
    version = (PROJECT / "version.txt").read_text(encoding="utf-8").strip()
    if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is None:
        raise ValueError("version.txt must contain the numeric MAJOR.MINOR.PATCH used by CMake")
    spec = importlib.util.spec_from_file_location(
        "time_tick_official_preparer", tools / "tools/prepare_system_update.py")
    preparer = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = preparer
    spec.loader.exec_module(preparer)
    check_layout(preparer._read_layout(PROJECT / "partitions.csv"), PRODUCT_CONTRACT)
    print(f"PASS: official metadata, workspace, protected layout and version {version}")
    if not metadata["website"].get("covers"):
        print("PENDING: real cover/screenshots; the website may require them before release")
    if args.bundle:
        if args.bundle.suffix != ".irisfw":
            raise ValueError("The firmware must be a .irisfw bundle")
        raw = args.bundle.read_bytes()
        description = json.loads((args.build_dir / "project_description.json").read_text(encoding="utf-8"))
        if description.get("project_name") != "time_tick" or description.get("target") != "esp32s31":
            raise ValueError("Build description is not the Time-Tick ESP32-S31 application")
        validate_application_config(args.build_dir)
        manifest, _ = validate_firmware(raw)
        plan = load_bundle(raw)
        if manifest["release"] != version or description["project_version"] != version:
            raise ValueError("Bundle, build and version.txt versions must match")
        application = next(c for c in plan["components"] if c["kind"] == "application")
        built_image = args.build_dir / "time_tick.bin"
        if application["sha256"] != hashlib.sha256(built_image.read_bytes()).hexdigest():
            raise ValueError("The bundle application differs from the current build/time_tick.bin")
        layout = preparer._read_layout(PROJECT / "partitions.csv")
        if application["target_offset"] != layout["main_app"].offset:
            raise ValueError("The bundle application does not target this project's main_app")
        print(f"PASS: unsigned Iris 0.2 bundle, application contract and matching version; SHA-256 {hashlib.sha256(raw).hexdigest()}")
    else:
        print("PENDING: .irisfw inspection and physical device tests; this mode checks framework files only")
    print("Local checks only; no account, upload or device operation performed.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)

