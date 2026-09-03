#!/usr/bin/env python3
#
# gen_version.py
#
# Copyright (C) 2026, Charles Chiou
#
# Expand version.h.in the same way CMake configure_file(@ONLY) does.
#

import pathlib
import re
import subprocess
import sys


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    firmware = root / "firmware" if (root / "firmware" / "CMakeLists.txt").exists() else root
    readme = (root / "README.md").read_text(encoding="utf-8")
    match = re.search(
        r"(?:<!--\s*robohero-version:\s*|\*\*Version:\s*)([0-9]+)\.([0-9]+)\.([0-9]+)",
        readme,
        re.IGNORECASE,
    )
    if not match:
        sys.exit("could not parse project VERSION from README.md")

    major, minor, patch = match.groups()
    version = f"{major}.{minor}.{patch}"
    whoami = subprocess.check_output(["whoami"], text=True).strip()
    hostname = subprocess.check_output(["hostname"], text=True).strip()
    date = subprocess.check_output(
        ["date", "+%Y-%m-%d %H:%M:%S"], text=True
    ).strip()

    text = (firmware / "version.h.in").read_text()
    text = (
        text.replace("@PROJECT_VERSION_MAJOR@", major)
        .replace("@PROJECT_VERSION_MINOR@", minor)
        .replace("@PROJECT_VERSION_PATCH@", patch)
        .replace("@PROJECT_VERSION@", version)
        .replace("@WHOAMI_OUTPUT@", whoami)
        .replace("@HOSTNAME_OUTPUT@", hostname)
        .replace("@DATE_OUTPUT@", date)
    )

    dest = firmware / "build" / "include" / "version.h"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text)
    print(version)


if __name__ == "__main__":
    main()
