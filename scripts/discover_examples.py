#!/usr/bin/env python3
"""Discover first-party examples for CI."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ESP_IDF_ROOTS = (
    Path("examples/esp-idf"),
    Path("examples/ESP-IDF"),
    Path("examples/ESP-IDF-v5.5"),
)
ARDUINO_ROOTS = (
    Path("examples/arduino"),
    Path("examples/Arduino"),
    Path("examples/Arduino-v3.3.5/examples"),
)
ARDUINO_LIBRARY_ROOTS = (
    Path("examples/arduino/libraries"),
    Path("examples/Arduino/libraries"),
    Path("examples/Arduino-v3.3.5/libraries"),
)


def normalize(value: str) -> str:
    return value.replace("\\", "/").strip("/")


def selector_matches(entry: dict[str, str], selector: str) -> bool:
    if not selector or selector == "all":
        return True
    selector = normalize(selector)
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


def first_existing_root(repo: Path, roots: tuple[Path, ...]) -> Path | None:
    for root in roots:
        candidate = repo / root
        if candidate.exists():
            return candidate
    return None


def is_project(path: Path) -> bool:
    return path.is_dir() and (path / "CMakeLists.txt").exists() and (path / "main").is_dir()


def discover_esp_idf(repo: Path) -> list[dict[str, str]]:
    root = first_existing_root(repo, ESP_IDF_ROOTS)
    if root is None:
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
    root = first_existing_root(repo, ARDUINO_ROOTS)
    if root is None:
        return []
    library_roots = [repo / item for item in ARDUINO_LIBRARY_ROOTS if (repo / item).exists()]
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
    parser.add_argument("--idf-versions", default="v5.5.4,v6.0.2")
    parser.add_argument("--arduino-core", default="3.3.10")
    parser.add_argument("--fqbn", default="esp32:esp32:esp32s3")
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