#!/usr/bin/env python3
"""Convert production LVGL PPM output to review PNGs (requires Pillow)."""
import argparse
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("render_dir", type=Path, help="Directory where tests/jelly_squeeze_ui/preview ran")
args = parser.parse_args()
output = ROOT / "assets/images/jelly_squeeze"
output.mkdir(parents=True, exist_ok=True)
for path in args.render_dir.glob("*.ppm"):
    with Image.open(path) as frame:
        assert frame.size == (240, 320)
        frame.save(output / (path.stem + ".png"))
# Host-rendered contact sheet; not a device capture or store cover.
canvas = Image.new("RGB", (784, 688), (219, 230, 239))
for index, name in enumerate(("home", "play", "pass", "fail", "paused", "result")):
    with Image.open(output / (name + ".png")) as frame:
        canvas.paste(frame, (16 + index % 3 * 256, 16 + index // 3 * 336))
canvas.save(output / "preview.png")
print(output / "preview.png")
