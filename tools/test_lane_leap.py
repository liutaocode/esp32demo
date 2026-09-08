#!/usr/bin/env python3
"""Run hardware-independent Lane Leap rules and Chinese font coverage checks."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="lane-leap-state-") as directory:
    binary = str(Path(directory) / "test")
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-Imain/apps/lane_leap", "tests/test_lane_leap_state.c",
        "main/apps/lane_leap/lane_leap_state.c", "-o", binary,
    ], cwd=ROOT, check=True)
    subprocess.run([binary], check=True)
subprocess.run([os.sys.executable, "tools/generate_lane_leap_font.py", "--check"], cwd=ROOT, check=True)
