#!/usr/bin/env python3
"""Render the production Pocket Arcade page with LVGL and verify every screen."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="pocket-arcade-ui-") as temp:
    build = Path(temp) / "build"
    commands = [
        ["cmake", "-S", str(ROOT / "tests/pocket_arcade_ui"), "-B", str(build)],
        ["cmake", "--build", str(build), "-j8"],
        [str(build / "preview")],
    ]
    for command in commands:
        result = subprocess.run(command, cwd=temp, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            print(result.stdout)
            raise SystemExit(result.returncode)
    print(result.stdout.strip())
