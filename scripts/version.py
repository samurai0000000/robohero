#!/usr/bin/env python3
#
# version.py
#
# Copyright (C) 2026, Charles Chiou
#
# Single source of truth version management for RoboHero.
# Reads and manages the version encoded in the root README.md.
#

import json
import pathlib
import re
import sys

VERSION_REGEX = re.compile(
    r"(?:<!--\s*robohero-version:\s*|\*\*Version:\s*)([0-9]+)\.([0-9]+)\.([0-9]+)",
    re.IGNORECASE,
)


def get_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent.parent


def read_version(root: pathlib.Path):
    readme_path = root / "README.md"
    if not readme_path.exists():
        sys.exit(f"Error: {readme_path} not found")

    content = readme_path.read_text(encoding="utf-8")
    match = VERSION_REGEX.search(content)
    if not match:
        sys.exit(f"Error: could not find version in {readme_path}")

    major, minor, patch = match.groups()
    return int(major), int(minor), int(patch)


def write_version(root: pathlib.Path, new_version: str):
    readme_path = root / "README.md"
    content = readme_path.read_text(encoding="utf-8")

    # Replace both comment and markdown tag if present
    content = re.sub(
        r"(<!--\s*robohero-version:\s*)[0-9]+\.[0-9]+\.[0-9]+(\s*-->)",
        rf"\g<1>{new_version}\g<2>",
        content,
    )
    content = re.sub(
        r"(\*\*Version:\s*)[0-9]+\.[0-9]+\.[0-9]+(\*\*)",
        rf"\g<1>{new_version}\g<2>",
        content,
    )
    readme_path.write_text(content, encoding="utf-8")
    print(f"Updated {readme_path.name} -> {new_version}")


def main():
    root = get_root()
    args = sys.argv[1:]
    cmd = args[0] if args else "get"

    major, minor, patch = read_version(root)
    version_str = f"{major}.{minor}.{patch}"

    if cmd == "get":
        print(version_str)
    elif cmd == "get-major":
        print(major)
    elif cmd == "get-minor":
        print(minor)
    elif cmd == "get-patch":
        print(patch)
    elif cmd == "sync":
        print(f"README.md is the single source of truth for version: {version_str}")
    elif cmd == "bump":
        part = args[1] if len(args) > 1 else "patch"
        if part == "major":
            major += 1
            minor = 0
            patch = 0
        elif part == "minor":
            minor += 1
            patch = 0
        elif part == "patch":
            patch += 1
        else:
            sys.exit(f"Unknown bump target: {part}. Use major, minor, or patch.")
        new_version = f"{major}.{minor}.{patch}"
        write_version(root, new_version)
    elif cmd == "set":
        if len(args) < 2:
            sys.exit("Usage: version.py set <major.minor.patch>")
        new_version = args[1].strip()
        if not re.match(r"^\d+\.\d+\.\d+$", new_version):
            sys.exit(f"Invalid version format: {new_version}")
        write_version(root, new_version)
    else:
        sys.exit(f"Unknown command: {cmd}")


if __name__ == "__main__":
    main()
