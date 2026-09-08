#!/usr/bin/env python3
"""Convert production LVGL PPM output to review PNGs (requires Pillow)."""
import argparse
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
COLUMNS = 4
FRAMES = (
    [f"game-{index:02d}" for index in range(1, 33)]
    + ["menu", "menu-page", "brief", "nonogram-solved", "over"]
)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("render_dir", type=Path,
                    help="Directory where tests/pocket_arcade_ui/preview ran")
args = parser.parse_args()
output = ROOT / "assets/images/pocket_arcade"
output.mkdir(parents=True, exist_ok=True)
for path in sorted(args.render_dir.glob("*.ppm")):
    with Image.open(path) as frame:
        assert frame.size == (240, 320)
        frame.save(output / (path.stem + ".png"))
# Host-rendered contact sheet; not a device capture or store cover.
rows = (len(FRAMES) + COLUMNS - 1) // COLUMNS
canvas = Image.new("RGB", (16 + COLUMNS * 256, 16 + rows * 336), (219, 230, 239))
for index, name in enumerate(FRAMES):
    with Image.open(output / (name + ".png")) as frame:
        canvas.paste(frame, (16 + index % COLUMNS * 256, 16 + index // COLUMNS * 336))
canvas.save(output / "preview.png")
print(output / "preview.png")
