#!/usr/bin/env python3
"""Verify that the generated LVGL font covers every non-ASCII UI character."""

from __future__ import annotations

import re
from pathlib import Path


APP_ROOT = Path(__file__).resolve().parents[1]
UI_SOURCE = APP_ROOT / "main/suzhou_travel.c"
FONT_SOURCE = APP_ROOT / "main/suzhou_zh_16.c"


def ui_codepoints() -> set[int]:
    source = UI_SOURCE.read_text(encoding="utf-8")
    literals = re.findall(r'"(?:\\.|[^"\\])*"', source, re.DOTALL)
    return {
        ord(char)
        for literal in literals
        for char in literal
        if ord(char) >= 0x80
    }


def font_codepoints() -> set[int]:
    source = FONT_SOURCE.read_text(encoding="utf-8")
    lists: dict[str, list[int]] = {}
    for name, body in re.findall(
        r"static const uint16_t (unicode_list_\d+)\[\] = \{(.*?)\};",
        source,
        re.DOTALL,
    ):
        lists[name] = [int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", body)]

    covered: set[int] = set()
    cmap_pattern = re.compile(
        r"\.range_start\s*=\s*(\d+).*?"
        r"\.range_length\s*=\s*(\d+).*?"
        r"\.unicode_list\s*=\s*(unicode_list_\d+|NULL)",
        re.DOTALL,
    )
    for start_text, length_text, list_name in cmap_pattern.findall(source):
        start = int(start_text)
        if list_name == "NULL":
            covered.update(range(start, start + int(length_text)))
        else:
            covered.update(start + offset for offset in lists[list_name])
    return covered


def main() -> None:
    expected = ui_codepoints()
    covered = font_codepoints()
    missing = sorted(expected - covered)
    if missing:
        formatted = ", ".join(f"U+{value:04X} {chr(value)}" for value in missing)
        raise SystemExit(f"UI font is missing: {formatted}")
    print(f"UI font coverage: PASS ({len(expected)} non-ASCII characters)")


if __name__ == "__main__":
    main()
