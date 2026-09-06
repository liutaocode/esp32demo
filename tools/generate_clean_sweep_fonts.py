#!/usr/bin/env python3
"""Regenerate Clean Sweep's Chinese subsets from its UI source."""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main/apps/clean_sweep"
FONT = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"
# 24 像素只画消行横幅和成绩数字,单独列出字表,不跟着注释一起变大。
BANNER = "消掉一行全清四三双！秒分"
source = "".join(p.read_text() for p in sorted(SOURCE.glob("*.c")))
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
plans = {12: symbols, 16: symbols, 24: BANNER}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()

if args.check:
    for size, wanted in plans.items():
        output = ROOT / f"assets/fonts/clean_sweep_zh_{size}.c"
        existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text())}
        missing = {ord(ch) for ch in wanted + "0123456789.%+"} - existing
        if missing:
            raise SystemExit(f"Missing {size}px glyphs: " + "".join(chr(n) for n in sorted(missing)))
    print(f"Clean Sweep fonts: {len(symbols)} UI characters covered, banner subset at 24px")
    raise SystemExit(0)

for size, wanted in plans.items():
    output = ROOT / f"assets/fonts/clean_sweep_zh_{size}.c"
    subprocess.run([
        "npx", "--yes", "lv_font_conv@1.5.3",
        "--size", str(size), "--bpp", "2", "--format", "lvgl",
        "--font", str(FONT),
        "--symbols", wanted, "--range", "0x20-0x40", "--no-compress", "--no-kerning",
        "--lv-include", "lvgl.h", "--lv-font-name", f"clean_sweep_zh_{size}",
        "--lv-fallback", "lv_font_montserrat_14",
        "-o", str(output),
    ], check=True)
    output.write_text(output.read_text().replace(str(ROOT), "<repo>"))
    print(output)
