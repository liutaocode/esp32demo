#!/usr/bin/env python3
"""Generate or check Chinese glyph subsets for the full-screen counter."""
from pathlib import Path
import argparse, os, re, subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--check',action='store_true'); a=p.parse_args()
source=''.join(f.read_text() for f in (ROOT/'main/tally').glob('*.c'))
symbols=''.join(sorted({c for c in source if ord(c)>127}))
base=Path(os.environ.get('TALLY_FONT_DIR', ROOT/'managed_components/lvgl__lvgl/scripts/built_in_font'))
for size in (12,14,20):
    name=f'tally_digits_{size}' if size==64 else f'tally_zh_{size}'
    output=ROOT/f'assets/fonts/{name}.c'
    wanted='0123456789' if size==64 else symbols+''.join(map(chr,range(32,127)))
    if a.check:
        existing={int(n,16) for n in re.findall(r'U\+([0-9A-Fa-f]+)',output.read_text())}
        missing={ord(c) for c in wanted}-existing
        assert not missing, (name,sorted(missing))
        print(f'{name}: {len(set(wanted))} glyphs verified')
    else:
        font=base/('Montserrat-Medium.ttf' if size==64 else 'SourceHanSansSC-Normal.otf')
        subprocess.run(['npx','--yes','lv_font_conv@1.5.3','--size',str(size),'--bpp','2','--format','lvgl','--font',str(font),'--symbols',wanted,'--no-compress','--no-kerning','--lv-include','lvgl.h','--lv-font-name',name,'-o',str(output)],check=True)
        output.write_text(output.read_text().replace(str(ROOT),'<repo>').replace(str(base),'<font-source>'))
