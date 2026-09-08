#!/usr/bin/env python3
"""Regenerate the Sanxingdui Museum LVGL Chinese glyph subset."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main/sanxingdui_museum.c"
OUTPUT = ROOT / "assets/fonts/sanxingdui_zh_16.c"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("font", type=Path, help="path to NotoSansCJKsc-Regular.otf")
    args = parser.parse_args()
    if not args.font.is_file():
        raise SystemExit(f"font file not found: {args.font}")

    symbols = "".join(sorted({character for character in SOURCE.read_text()
                              if ord(character) > 127}))
    subprocess.run(
        [
            "npx", "--yes", "lv_font_conv@1.5.3",
            "--font", str(args.font),
            "--symbols", symbols,
            "--size", "16",
            "--bpp", "2",
            "--format", "lvgl",
            "--no-compress",
            "--lv-include", "lvgl.h",
            "--lv-font-name", "sanxingdui_zh_16",
            "--lv-fallback", "lv_font_montserrat_14",
            "--output", str(OUTPUT),
        ],
        check=True,
    )
    generated = OUTPUT.read_text()
    generated = generated.replace(str(args.font), "NotoSansCJKsc-Regular.otf")
    generated = generated.replace(str(OUTPUT), "assets/fonts/sanxingdui_zh_16.c")
    OUTPUT.write_text(generated.rstrip() + "\n")
    print(f"Wrote {len(symbols)} glyphs to {OUTPUT}")


if __name__ == "__main__":
    main()
