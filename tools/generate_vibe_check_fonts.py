#!/usr/bin/env python3
"""Regenerate Vibe Check's Simplified Chinese font subsets from its UI source."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main/apps/vibe_check/vibe_check.c"
FONT = ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"

symbols = "".join(sorted(set(re.findall(
    r"[\u3000-\u303f\u3400-\u9fff\uff00-\uffef]", SOURCE.read_text()
))))

subprocess.run([
    "npx", "--yes", "lv_font_conv@1.5.3",
    "--size", "16", "--bpp", "2", "--format", "lvgl",
    "--font", str(FONT), "--symbols", symbols,
    "--no-compress", "--no-kerning", "--lv-include", "lvgl.h",
    "--lv-font-name", "vibe_check_zh_16",
    "--lv-fallback", "lv_font_montserrat_14",
    "-o", str(ROOT / "assets/fonts/vibe_check_zh_16.c"),
], check=True)
