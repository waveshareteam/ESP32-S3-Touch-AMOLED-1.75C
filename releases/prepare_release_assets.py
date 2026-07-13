#!/usr/bin/env python3
"""Validate CI firmware archives and stage assets for a GitHub release."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath


PRODUCT = "ESP32-S3-Touch-AMOLED-1.75C"
DEFAULT_EXPECTED_COUNT = 17


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_member(archive: zipfile.ZipFile, member: str) -> str:
    digest = hashlib.sha256()
    with archive.open(member) as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_member(names: list[str], suffix: str) -> str:
    matches = [name for name in names if name == suffix or name.endswith("/" + suffix)]
    if len(matches) != 1:
        raise ValueError(f"expected one {suffix} entry, found {len(matches)}")
    return matches[0]


def inspect_archive(path: Path) -> dict[str, object]:
    if not path.name.endswith("-combined.zip"):
        raise ValueError(f"archive does not use the combined naming convention: {path.name}")

    with zipfile.ZipFile(path) as archive:
        names = [info.filename for info in archive.infolist() if not info.is_dir()]
        manifest_member = find_member(names, "manifest.json")
        manifest = json.loads(archive.read(manifest_member).decode("utf-8"))
        combined = manifest.get("combined_bin")
        if not isinstance(combined, dict):
            raise ValueError(f"missing combined_bin metadata in {path.name}")

        root = PurePosixPath(manifest_member).parent
        combined_member = (root / str(combined.get("file", ""))).as_posix()
        if combined_member not in names:
            raise ValueError(f"missing combined firmware {combined_member} in {path.name}")
        if str(combined.get("offset")) != "0x0":
            raise ValueError(f"combined firmware in {path.name} must start at 0x0")

        info = archive.getinfo(combined_member)
        if info.file_size != combined.get("size"):
            raise ValueError(f"combined firmware size mismatch in {path.name}")
        combined_sha = sha256_member(archive, combined_member)
        if combined_sha != combined.get("sha256"):
            raise ValueError(f"combined firmware checksum mismatch in {path.name}")

        for helper in (
            "flash_combined.sh",
            "flash_combined.bat",
            "flash_combined_args.txt",
        ):
            find_member(names, helper)

    return {
        "filename": path.name,
        "size": path.stat().st_size,
        "sha256": sha256_file(path),
        "name": manifest.get("name"),
        "framework": manifest.get("framework"),
        "framework_version": manifest.get("framework_version"),
        "target": manifest.get("target"),
        "project": manifest.get("project"),
        "git_sha": manifest.get("git_sha"),
        "combined_bin": combined,
    }


def prepare(args: argparse.Namespace) -> Path:
    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)
    archives = sorted(input_dir.rglob("*-combined.zip"), key=lambda item: item.name.lower())
    if len(archives) != args.expected_count:
        raise ValueError(f"expected {args.expected_count} combined archives, found {len(archives)}")
    if len({path.name for path in archives}) != len(archives):
        raise ValueError("duplicate combined archive filenames found")

    assets = [inspect_archive(path) for path in archives]
    git_shas = {str(asset["git_sha"]) for asset in assets if asset.get("git_sha")}
    if len(git_shas) != 1:
        raise ValueError(f"release assets must share one non-empty git SHA, found {len(git_shas)}")
    git_sha = next(iter(git_shas))
    if args.git_sha and git_sha != args.git_sha:
        raise ValueError(f"release asset git SHA {git_sha} does not match expected {args.git_sha}")

    if args.clean and output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    for source in archives:
        shutil.copy2(source, output_dir / source.name)

    summary = {
        "product": PRODUCT,
        "release": args.version,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "git_sha": git_sha,
        "asset_count": len(assets),
        "assets": assets,
    }
    manifest_path = output_dir / "manifest-combined-assets.json"
    manifest_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(manifest_path.as_posix())
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", required=True, help="Directory containing downloaded CI archives.")
    parser.add_argument("--output-dir", required=True, help="Directory used to stage release assets.")
    parser.add_argument("--version", required=True, help="Release version, for example v1.0.1.")
    parser.add_argument("--git-sha", help="Expected full Git SHA for every firmware archive.")
    parser.add_argument("--expected-count", type=int, default=DEFAULT_EXPECTED_COUNT)
    parser.add_argument("--clean", action="store_true", help="Replace the output directory before staging.")
    args = parser.parse_args()
    try:
        prepare(args)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
