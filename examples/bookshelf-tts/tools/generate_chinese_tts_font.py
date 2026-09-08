#!/usr/bin/env python3
"""Regenerate Chinese TTS's Chinese subset from its UI source."""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = "".join((ROOT / "main" / name).read_text() for name in ("demo_chinese_tts.c", "tts_model.c"))
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
output = ROOT / "assets/fonts/chinese_tts_zh_16.c"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
parser.add_argument("--font", type=Path, default=ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf")
args = parser.parse_args()
if args.check:
    existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text())}
    missing = {ord(ch) for ch in symbols} - existing
    if missing:
        raise SystemExit("Missing glyphs: " + "".join(chr(n) for n in sorted(missing)))
    print(f"Chinese TTS font: {len(symbols)} UI characters covered")
    raise SystemExit(0)
subprocess.run([
    "npx", "--yes", "lv_font_conv@1.5.3",
    "--size", "16", "--bpp", "2", "--format", "lvgl",
    "--font", str(args.font),
    "--symbols", symbols, "--range", "0x20-0x40", "--no-compress", "--no-kerning",
    "--lv-include", "lvgl.h", "--lv-font-name", "chinese_tts_zh_16",
    "--lv-fallback", "lv_font_montserrat_14",
    "-o", str(ROOT / "assets/fonts/chinese_tts_zh_16.c"),
], check=True)

output.write_text(output.read_text().replace(str(args.font), args.font.name).replace(str(ROOT), "<repo>"))
