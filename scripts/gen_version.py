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
    cmake = (root / "CMakeLists.txt").read_text()
    match = re.search(
        r"project\(\s*robohero\s+VERSION\s+(\d+)\.(\d+)\.(\d+)", cmake
    )
    if not match:
        sys.exit("could not parse project VERSION from CMakeLists.txt")

    major, minor, patch = match.groups()
    version = f"{major}.{minor}.{patch}"
    whoami = subprocess.check_output(["whoami"], text=True).strip()
    hostname = subprocess.check_output(["hostname"], text=True).strip()
    date = subprocess.check_output(
        ["date", "+%Y-%m-%d %H:%M:%S"], text=True
    ).strip()

    text = (root / "version.h.in").read_text()
    text = (
        text.replace("@PROJECT_VERSION_MAJOR@", major)
        .replace("@PROJECT_VERSION_MINOR@", minor)
        .replace("@PROJECT_VERSION_PATCH@", patch)
        .replace("@PROJECT_VERSION@", version)
        .replace("@WHOAMI_OUTPUT@", whoami)
        .replace("@HOSTNAME_OUTPUT@", hostname)
        .replace("@DATE_OUTPUT@", date)
    )

    dest = root / "build" / "include" / "version.h"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text)
    print(version)


if __name__ == "__main__":
    main()
