#!/usr/bin/env python3
"""Regenerate the Social Battery Chinese font subsets from its UI source."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font', type=Path, default=ROOT / 'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf')
    args = parser.parse_args()
    source = (ROOT / 'main/apps/social_battery/social_battery.c').read_text()
    symbols = ''.join(sorted(set(re.findall(r'[\u3000-\u303f\u3400-\u9fff\uff00-\uffef]', source))))
    for size in (16, 28):
        output = ROOT / f'assets/fonts/social_battery_zh_{size}.c'
        subprocess.run([
            'npx', '--yes', 'lv_font_conv@1.5.3', '--size', str(size),
            '--bpp', '2', '--format', 'lvgl', '--font', str(args.font),
            '--symbols', symbols, '--range', '0x20,0x30-0x39', '--no-compress',
            '--no-kerning', '--lv-fallback', 'lv_font_montserrat_14', '--lv-include', 'lvgl.h', '--lv-font-name',
            f'social_battery_zh_{size}', '-o',
            str(output)
        ], check=True)
        # Keep generated comments portable and free of local account paths.
        generated = output.read_text().replace(str(ROOT) + '/', '')
        generated = generated.replace(str(args.font), args.font.name)
        output.write_text(generated)


if __name__ == '__main__':
    main()
