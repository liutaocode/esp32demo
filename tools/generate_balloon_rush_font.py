#!/usr/bin/env python3
"""Regenerate Balloon Rush's Chinese subset from its UI source."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/apps/balloon_rush/balloon_rush.c").read_text()
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
subprocess.run([
    "npx", "--yes", "lv_font_conv@1.5.3",
    "--size", "16", "--bpp", "2", "--format", "lvgl",
    "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
    "--symbols", symbols, "--range", "0x20-0x7e", "--no-compress", "--no-kerning",
    "--lv-include", "lvgl.h", "--lv-font-name", "balloon_rush_zh_16",
    "--lv-fallback", "lv_font_montserrat_14",
    "-o", str(ROOT / "assets/fonts/balloon_rush_zh_16.c"),
], check=True)
