#!/usr/bin/env python3
"""Generate and validate the exact Chinese UI font subsets."""
from pathlib import Path
import argparse, os, re, subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__); p.add_argument('--check',action='store_true'); a=p.parse_args()
text=''.join(f.read_text() for f in (ROOT/'main/cat').glob('*.c'))
symbols=''.join(sorted({c for c in text if ord(c)>127}))
source=Path(os.environ.get('CAT_FONT_SOURCE',str(ROOT/'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf')))
for size in (12,14,16,20):
    target=ROOT/f'assets/fonts/cat_zh_{size}.c'
    if a.check:
        chars={int(s,16) for s in re.findall(r'U\+([0-9A-Fa-f]+)',target.read_text())}
        assert not {ord(c) for c in symbols}-chars, f'{size}: missing glyphs'
        print(f'Font {size}: {len(symbols)} Chinese/UI characters verified')
    else:
        conv=['node',os.environ['LV_FONT_CONV']] if os.environ.get('LV_FONT_CONV') else ['npx','--yes','lv_font_conv@1.5.3']
        subprocess.run(conv+['--size',str(size),'--bpp','2','--format','lvgl','--font',str(source),'--symbols',symbols,'--range','0x20-0x7e','--no-compress','--no-kerning','--lv-include','lvgl.h','--lv-font-name',f'cat_zh_{size}','-o',str(target)],check=True)
        contents=target.read_text(); contents=contents.replace(str(ROOT),'<repo>').replace(str(source),'<SourceHanSansSC-Normal.otf>')
        target.write_text(contents)
