#!/usr/bin/env python3
"""Convert host LVGL PPM frames to clearly identified local UI previews."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('input', type=Path)
args = parser.parse_args()
out = Path(__file__).resolve().parents[1] / 'projects/math-rail/assets'
out.mkdir(parents=True, exist_ok=True)
for p in args.input.glob('*.ppm'):
    with Image.open(p) as frame:
        frame.save(out / (p.stem + '.png'))
canvas = Image.new('RGB', (1008, 368), '#e8eff2')
for i, name in enumerate(['home', 'question', 'correction', 'result']):
    with Image.open(out / (name + '.png')) as frame:
        canvas.paste(frame, (12 + i * 252, 24))
canvas.save(out / 'interface-preview.png')
print('Host UI previews exported; these are not device captures or publication covers.')
