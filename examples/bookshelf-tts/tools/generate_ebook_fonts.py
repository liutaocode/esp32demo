#!/usr/bin/env python3
"""Generate/check the e-book reader's full GB2312 fonts.

Unlike every other application in this repository, the reader displays text the
user uploads at runtime, so a per-app character subset is impossible. The three
sizes below cover the complete GB2312 set at 1 bpp; anything denser does not fit
under the 3 MB application limit.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SIZES = (12, 16, 20)
# ASCII stays available through --range so English inside a book still renders.
ASCII_RANGE = "0x20-0x7e"


# Python's gb2312 codec maps two punctuation bytes to the Japanese variants:
# A1A4 becomes U+30FB (katakana middle dot) rather than U+00B7, and A1AA becomes
# U+2015 rather than U+2014. Chinese text on this device uses the U+00B7 and
# U+2014 forms, so a set derived purely from the codec renders them as tofu.
SUPPLEMENT = "\u00b7\u2014\u2013"


def gb2312_symbols() -> str:
    """Every character encodable in GB2312, plus the punctuation the codec renames."""
    chars = set(SUPPLEMENT)
    for high in range(0xA1, 0xFF):
        for low in range(0xA1, 0xFF):
            try:
                chars.add(bytes([high, low]).decode("gb2312"))
            except UnicodeDecodeError:
                continue
    return "".join(sorted(chars))


def ui_symbols() -> str:
    """Non-ASCII characters written into the reader's own screens."""
    source = "".join(p.read_text() for p in (ROOT / "main/ebook").glob("*.c"))
    return "".join(sorted({ch for ch in source if ord(ch) > 127}))


def check(symbols: str) -> None:
    # The interface strings are checked separately from the book text: a
    # character the code writes but the font lacks is a tofu box every user
    # sees on every screen, and the GB2312 sweep alone does not catch it —
    # Python's codec renames two punctuation marks (see SUPPLEMENT).
    interface = ui_symbols()
    for size in SIZES:
        generated = (ROOT / f"assets/fonts/ebook_zh_{size}.c").read_text()
        match = re.search(r"--symbols (.*?) --range", generated)
        covered = set(match.group(1) if match else "")
        for label, wanted in (("GB2312", set(symbols)), ("interface", set(interface))):
            missing = wanted - covered
            if missing:
                raise SystemExit(
                    f"E-book {size}px font is missing {len(missing)} {label} characters: "
                    + "".join(sorted(missing))[:40]
                )
        print(f"E-book {size}px font: {len(symbols)} GB2312 plus "
              f"{len(interface)} interface characters covered")


def generate(symbols: str) -> None:
    for size in SIZES:
        output = ROOT / f"assets/fonts/ebook_zh_{size}.c"
        subprocess.run([
            "npx", "--yes", "lv_font_conv@1.5.3", "--size", str(size),
            # 1 bpp is not a style choice: 2 bpp over 7,445 glyphs costs about
            # 1 MB per size and blows the application budget.
            "--bpp", "1", "--format", "lvgl",
            "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"),
            "--symbols", symbols, "--range", ASCII_RANGE,
            "--no-compress", "--no-kerning",
            "--lv-include", "lvgl.h", "--lv-font-name", f"ebook_zh_{size}",
            "-o", str(output),
        ], check=True)
        print(f"E-book {size}px font: {output.stat().st_size} bytes of C source")


if __name__ == "__main__":
    syms = gb2312_symbols()
    if "--check" in sys.argv:
        check(syms)
    else:
        generate(syms)
