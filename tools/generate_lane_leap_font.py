#!/usr/bin/env python3
"""Regenerate Lane Leap's Chinese font subsets from its UI source."""
from pathlib import Path
import subprocess
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/apps/lane_leap/lane_leap.c").read_text()
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
for size in (12, 16):
    name = f"lane_leap_zh_{size}"
    output = ROOT / f"assets/fonts/{name}.c"
    if args.check:
        existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text())}
        missing = {ord(ch) for ch in symbols} - existing
        if missing:
            raise SystemExit(f"Missing {size}px glyphs: " + "".join(chr(n) for n in sorted(missing)))
        print(f"Lane Leap {size}px font: {len(symbols)} UI characters covered")
        continue
    subprocess.run([
        "npx", "--yes", "lv_font_conv@1.5.3",
        "--size", str(size), "--bpp", "2", "--format", "lvgl",
        "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
        "--symbols", symbols, "--range", "0x20-0x7e", "--no-compress", "--no-kerning",
        "--lv-include", "lvgl.h", "--lv-font-name", name,
        "--lv-fallback", "lv_font_montserrat_14", "-o", str(output),
    ], check=True)
    output.write_text(output.read_text().replace(str(ROOT), "<repo>"))
