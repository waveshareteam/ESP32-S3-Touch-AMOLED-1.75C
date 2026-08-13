#!/usr/bin/env python3
"""Discover first-party examples for CI."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ESP_IDF_ROOT = Path("examples/esp-idf")
ARDUINO_ROOT = Path("examples/arduino/examples")
ARDUINO_LIBRARY_ROOT = Path("examples/arduino/libraries")
ARDUINO_FQBN = (
    "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,"
    "PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc"
)
LEGACY_SELECTOR_PREFIXES = (
    ("examples/ESP-IDF-v5.5/", "examples/esp-idf/"),
    ("examples/ESP-IDF/", "examples/esp-idf/"),
    ("examples/Arduino-v3.3.5/examples/", "examples/arduino/examples/"),
    ("examples/Arduino-v3.3.5/libraries/", "examples/arduino/libraries/"),
    ("examples/Arduino-v3.3.5/", "examples/arduino/"),
    ("examples/Arduino/", "examples/arduino/"),
)


def normalize(value: str) -> str:
    value = value.replace("\\", "/").strip().strip("/")
    for old, new in LEGACY_SELECTOR_PREFIXES:
        old = old.strip("/")
        new = new.strip("/")
        if value == old:
            return new
        if value.startswith(old + "/"):
            return new + value[len(old) :]
    return value


def selector_matches(entry: dict[str, str], selector: str) -> bool:
    if not selector or selector == "all":
        return True
    selectors = [normalize(item) for item in selector.split(",") if item]
    return any(_one_selector_matches(entry, item) for item in selectors)


def _one_selector_matches(entry: dict[str, str], selector: str) -> bool:
    path = normalize(entry["path"])
    name = entry["name"]
    ino = entry.get("ino", "")
    return (
        selector == name
        or selector == ino
        or selector == Path(ino).stem
        or selector == path
        or path.startswith(selector + "/")
        or selector in path.split("/")
        or any(part.startswith(selector + ".") for part in path.split("/"))
    )


def is_project(path: Path) -> bool:
    return path.is_dir() and (path / "CMakeLists.txt").exists() and (path / "main").is_dir()


def discover_esp_idf(repo: Path) -> list[dict[str, str]]:
    root = repo / ESP_IDF_ROOT
    if not root.exists():
        return []
    entries: list[dict[str, str]] = []
    if is_project(root):
        entries.append({"name": root.name, "path": root.relative_to(repo).as_posix()})
    for project in sorted(root.iterdir(), key=lambda item: item.name.lower()):
        if is_project(project):
            entries.append({"name": project.name, "path": project.relative_to(repo).as_posix()})
    return entries


def is_under(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
    except ValueError:
        return False
    return True


def discover_arduino(repo: Path) -> list[dict[str, str]]:
    root = repo / ARDUINO_ROOT
    if not root.exists():
        return []
    library_roots = [repo / ARDUINO_LIBRARY_ROOT] if (repo / ARDUINO_LIBRARY_ROOT).exists() else []
    entries: list[dict[str, str]] = []
    seen: set[str] = set()
    for ino in sorted(root.rglob("*.ino"), key=lambda item: item.as_posix().lower()):
        if any(is_under(ino, lib_root) for lib_root in library_roots):
            continue
        sketch_dir = ino.parent.relative_to(repo).as_posix()
        if sketch_dir in seen:
            continue
        seen.add(sketch_dir)
        entries.append({"name": ino.parent.name, "path": sketch_dir, "ino": ino.name})
    return entries


def build_matrix(args: argparse.Namespace) -> dict[str, list[dict[str, str]]]:
    repo = Path(args.repo).resolve()
    selector = normalize(args.selector)
    if args.surface == "esp-idf":
        projects = [entry for entry in discover_esp_idf(repo) if selector_matches(entry, selector)]
        versions = [item.strip() for item in args.idf_versions.split(",") if item.strip()]
        include = [entry | {"idf": version} for entry in projects for version in versions]
    else:
        sketches = [entry for entry in discover_arduino(repo) if selector_matches(entry, selector)]
        include = [entry | {"core": args.arduino_core, "fqbn": args.fqbn} for entry in sketches]
    return {"include": include}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    parser.add_argument("--surface", choices=("esp-idf", "arduino"), required=True)
    parser.add_argument("--selector", default="all")
    parser.add_argument("--idf-versions", default="v5.5.5,v6.0.2")
    parser.add_argument("--arduino-core", default="3.3.11")
    parser.add_argument("--fqbn", default=ARDUINO_FQBN)
    parser.add_argument("--github-output")
    args = parser.parse_args()

    matrix = build_matrix(args)
    output = json.dumps(matrix, separators=(",", ":"))
    count = len(matrix["include"])
    if args.github_output:
        with open(args.github_output, "a", encoding="utf-8") as fh:
            fh.write(f"matrix={output}\n")
            fh.write(f"count={count}\n")
    else:
        print(output)


if __name__ == "__main__":
    main()
