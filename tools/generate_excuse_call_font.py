#!/usr/bin/env python3
"""Regenerate Excuse Call's Chinese font subsets from its UI source."""
from pathlib import Path
import subprocess
import argparse
import re
import os

ROOT = Path(__file__).resolve().parents[1]
source = "".join(p.read_text() for p in (ROOT / "main/apps/excuse_call").glob("*.c"))
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
for size in (16, 24, 32):
    name = f"excuse_call_zh_{size}"
    output = ROOT / f"assets/fonts/{name}.c"
    if args.check:
        existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text())}
        missing = {ord(ch) for ch in symbols} - existing
        if missing:
            raise SystemExit(f"Missing {size}px glyphs: " + "".join(chr(n) for n in sorted(missing)))
        print(f"Excuse Call {size}px font: {len(symbols)} UI characters covered")
        continue
    converter = ["node", os.environ["LV_FONT_CONV"]] if os.environ.get("LV_FONT_CONV") else ["npx", "--yes", "lv_font_conv@1.5.3"]
    subprocess.run(converter + [
        "--size", str(size), "--bpp", "4", "--format", "lvgl",
        "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
        "--symbols", symbols, "--range", "0x20-0x7e", "--no-compress", "--no-kerning",
        "--lv-include", "lvgl.h", "--lv-font-name", name,
        "--lv-fallback", "lv_font_montserrat_14", "-o", str(output),
    ], check=True)
    output.write_text(output.read_text().replace(str(ROOT), "<repo>"))
