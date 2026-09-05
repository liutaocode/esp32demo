#!/usr/bin/env python3
"""Regenerate Down 100's Chinese subset from its UI source."""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/apps/down_100/down_100.c").read_text()
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
output = ROOT / "assets/fonts/down_100_zh_16.c"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
if args.check:
    existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text())}
    missing = {ord(ch) for ch in symbols} - existing
    if missing:
        raise SystemExit("Missing glyphs: " + "".join(chr(n) for n in sorted(missing)))
    print(f"Down 100 font: {len(symbols)} UI characters covered")
    raise SystemExit(0)
subprocess.run([
    "npx", "--yes", "lv_font_conv@1.5.3",
    "--size", "16", "--bpp", "2", "--format", "lvgl",
    "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
    "--symbols", symbols, "--no-compress", "--no-kerning",
    "--lv-include", "lvgl.h", "--lv-font-name", "down_100_zh_16",
    "--lv-fallback", "lv_font_montserrat_14",
    "-o", str(ROOT / "assets/fonts/down_100_zh_16.c"),
], check=True)

output.write_text(output.read_text().replace(str(ROOT), "<repo>"))
