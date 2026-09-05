#!/usr/bin/env python3
"""Regenerate Pocket Pond's Chinese subset from its UI source."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = "".join(p.read_text() for p in (ROOT / "main/apps/pocket_pond").glob("*.c"))
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
subprocess.run([
    "npx", "--yes", "lv_font_conv@1.5.3",
    "--size", "16", "--bpp", "2", "--format", "lvgl",
    "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
    "--symbols", symbols, "--no-compress", "--no-kerning",
    "--lv-include", "lvgl.h", "--lv-font-name", "pocket_pond_zh_16",
    "--lv-fallback", "lv_font_montserrat_14",
    "-o", str(ROOT / "assets/fonts/pocket_pond_zh_16.c"),
], check=True)
