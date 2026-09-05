#!/usr/bin/env python3
"""Regenerate the Daily Memory Chinese font subset from visible UI text."""

import argparse
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--font",
        type=Path,
        default=ROOT
        / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf",
    )
    args = parser.parse_args()
    sources = [
        ROOT / "main/apps/memory_garden/memory_garden.c",
        ROOT / "main/apps/memory_garden/memory_garden_state.c",
    ]
    text = "".join(path.read_text() for path in sources)
    symbols = "".join(
        sorted(set(re.findall(r"[\u00b7\u3000-\u303f\u3400-\u9fff\uff00-\uffef]", text)))
    )
    subprocess.run(
        [
            "npx",
            "--yes",
            "lv_font_conv@1.5.3",
            "--size",
            "18",
            "--bpp",
            "2",
            "--format",
            "lvgl",
            "--font",
            str(args.font),
            "--symbols",
            symbols,
            "--range",
            "0x20,0x23,0x25,0x2B,0x2D,0x2F,0x30-0x39,0x41-0x46",
            "--no-compress",
            "--no-kerning",
            "--lv-fallback",
            "lv_font_montserrat_14",
            "--lv-include",
            "lvgl.h",
            "--lv-font-name",
            "memory_garden_zh_18",
            "-o",
            str(ROOT / "assets/fonts/memory_garden_zh_18.c"),
        ],
        check=True,
    )


if __name__ == "__main__":
    main()
