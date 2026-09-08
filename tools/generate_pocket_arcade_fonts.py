#!/usr/bin/env python3
"""Regenerate Pocket Arcade's Chinese subsets from its user interface strings."""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "main/apps/pocket_arcade"
STRING_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
SIZES = (16, 12)


def ui_symbols() -> str:
    """Collect every non-ASCII character that can reach a label.

    Comments are stripped first: Chinese in a comment never renders, and a
    stray quote inside one would otherwise swallow the literal next to it.
    """
    found: set[str] = set()
    for path in sorted(APP.glob("*.[ch]")):
        source = COMMENT_RE.sub(" ", path.read_text(encoding="utf-8"))
        for literal in STRING_RE.findall(source):
            found.update(ch for ch in literal if ord(ch) > 127)
    return "".join(sorted(found))


def output_for(size: int) -> Path:
    return ROOT / f"assets/fonts/pocket_arcade_zh_{size}.c"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    symbols = ui_symbols()
    if args.check:
        for size in SIZES:
            path = output_for(size)
            existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", path.read_text())}
            missing = {ord(ch) for ch in symbols} - existing
            if missing:
                raise SystemExit(
                    f"{path.name}: missing glyphs " + "".join(chr(n) for n in sorted(missing))
                )
        print(f"Pocket Arcade fonts: {len(symbols)} interface characters covered")
        return 0

    for size in SIZES:
        output = output_for(size)
        subprocess.run([
            "npx", "--yes", "lv_font_conv@1.5.3",
            "--size", str(size), "--bpp", "2", "--format", "lvgl",
            "--font", str(ROOT / "managed_components/lvgl__lvgl/scripts/built_in_font/"
                                 "SourceHanSansSC-Normal.otf"),
            "--symbols", symbols, "--no-compress", "--no-kerning",
            "--lv-include", "lvgl.h", "--lv-font-name", f"pocket_arcade_zh_{size}",
            "--lv-fallback", "lv_font_montserrat_14",
            "-o", str(output),
        ], check=True)
        output.write_text(output.read_text().replace(str(ROOT), "<repo>"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
