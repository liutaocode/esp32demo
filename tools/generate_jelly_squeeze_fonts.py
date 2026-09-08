#!/usr/bin/env python3
"""Regenerate Jelly Squeeze's Chinese subsets from its UI source."""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 24)
source = (ROOT / "main/apps/jelly_squeeze/jelly_squeeze.c").read_text()
# 只收字符串字面量里的字符：注释里的中文不该占用字库空间。
literals = re.findall(r'"((?:[^"\\\n]|\\.)*)"', source)
symbols = "".join(sorted({ch for text in literals for ch in text if ord(ch) > 127}))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
for size in SIZES:
    output = ROOT / f"assets/fonts/jelly_squeeze_zh_{size}.c"
    if args.check:
        existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text())}
        missing = {ord(ch) for ch in symbols} - existing
        if missing:
            raise SystemExit(f"Missing {size}px glyphs: " + "".join(chr(n) for n in sorted(missing)))
        continue
    subprocess.run([
        "npx", "--yes", "lv_font_conv@1.5.3",
        "--size", str(size), "--bpp", "2", "--format", "lvgl",
        "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
        "--symbols", symbols, "--no-compress", "--no-kerning",
        "--lv-include", "lvgl.h", "--lv-font-name", f"jelly_squeeze_zh_{size}",
        "--lv-fallback", "lv_font_montserrat_14",
        "-o", str(output),
    ], check=True)
    output.write_text(output.read_text().replace(str(ROOT), "<repo>"))
if args.check:
    print(f"Jelly Squeeze fonts: {len(symbols)} UI characters covered at {SIZES}")
