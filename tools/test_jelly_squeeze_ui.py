#!/usr/bin/env python3
"""Render the production Jelly Squeeze UI with LVGL and verify every page."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--keep", type=Path, help="keep the rendered PPM frames in this directory")
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="jelly-squeeze-ui-") as temp:
    workdir = Path(args.keep) if args.keep else Path(temp)
    workdir.mkdir(parents=True, exist_ok=True)
    build = Path(temp) / "build"
    commands = [
        ["cmake", "-S", str(ROOT / "tests/jelly_squeeze_ui"), "-B", str(build)],
        ["cmake", "--build", str(build), "-j8"],
        [str(build / "preview")],
    ]
    for command in commands:
        result = subprocess.run(command, cwd=workdir, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            print(result.stdout)
            raise SystemExit(result.returncode)
    print(result.stdout.strip())
