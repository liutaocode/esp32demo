#!/usr/bin/env python3
"""Generate the tiny RGB565 header and LVGL Chinese font for the Suzhou app."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path


APP_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = APP_ROOT
SOURCE_IMAGE = REPO_ROOT / "assets/images/suzhou-garden-header-source.png"
PREVIEW_IMAGE = REPO_ROOT / "assets/images/suzhou-garden-header-240x96.png"
IMAGE_C = APP_ROOT / "main/suzhou_garden_header.c"
UI_SOURCE = APP_ROOT / "main/suzhou_travel.c"
FONT_SOURCE = Path("/tmp/NotoSansCJKsc-Regular.otf")
FONT_C = APP_ROOT / "main/suzhou_zh_16.c"


def generate_image() -> None:
    raw_path = APP_ROOT / "main/suzhou_garden_header.rgb565"
    filters = "scale=240:96:force_original_aspect_ratio=increase,crop=240:96"
    subprocess.run(
        [
            "ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
            "-i", str(SOURCE_IMAGE), "-vf", filters, "-frames:v", "1",
            str(PREVIEW_IMAGE),
        ],
        check=True,
    )
    subprocess.run(
        [
            "ffmpeg", "-y", "-hide_banner", "-loglevel", "error",
            "-i", str(PREVIEW_IMAGE), "-pix_fmt", "rgb565le", "-f", "rawvideo",
            str(raw_path),
        ],
        check=True,
    )
    data = raw_path.read_bytes()
    raw_path.unlink()
    if len(data) != 240 * 96 * 2:
        raise RuntimeError(f"unexpected RGB565 byte count: {len(data)}")

    rows = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        rows.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    body = "\n".join(rows)
    IMAGE_C.write_text(
        "#include \"suzhou_garden_header.h\"\n\n"
        "/* Generated from assets/images/suzhou-garden-header-source.png. */\n"
        "static const LV_ATTRIBUTE_MEM_ALIGN uint8_t suzhou_garden_header_data[] = {\n"
        f"{body}\n"
        "};\n\n"
        "const lv_image_dsc_t suzhou_garden_header = {\n"
        "    .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
        "    .header.cf = LV_COLOR_FORMAT_RGB565,\n"
        "    .header.flags = 0,\n"
        "    .header.w = 240,\n"
        "    .header.h = 96,\n"
        "    .header.stride = 480,\n"
        "    .data_size = sizeof(suzhou_garden_header_data),\n"
        "    .data = suzhou_garden_header_data,\n"
        "};\n",
        encoding="utf-8",
    )


def generate_font() -> None:
    if not FONT_SOURCE.exists():
        raise RuntimeError(
            "Place NotoSansCJKsc-Regular.otf at /tmp before regenerating the font"
        )
    source = UI_SOURCE.read_text(encoding="utf-8")
    # Collect every non-ASCII character from C string literals instead of relying
    # on hand-maintained Unicode ranges. This includes punctuation such as the
    # ellipsis (U+2026), CJK punctuation, and full-width punctuation.
    string_literals = re.findall(r'"(?:\\.|[^"\\])*"', source, re.DOTALL)
    symbols = "".join(
        sorted({char for literal in string_literals for char in literal
                if ord(char) >= 0x80})
    )
    subprocess.run(
        [
            "npx", "--yes", "lv_font_conv@1.5.3", "--size", "16", "--bpp", "2",
            "--format", "lvgl", "--font", str(FONT_SOURCE), "--symbols", symbols,
            "--no-compress", "--no-kerning", "--lv-include", "lvgl.h",
            "--lv-font-name", "suzhou_zh_16", "--lv-fallback", "lv_font_montserrat_14",
            "-o", str(FONT_C),
        ],
        check=True,
    )


if __name__ == "__main__":
    generate_image()
    generate_font()
    print(f"Generated {IMAGE_C.relative_to(APP_ROOT)}")
    print(f"Generated {FONT_C.relative_to(APP_ROOT)}")
