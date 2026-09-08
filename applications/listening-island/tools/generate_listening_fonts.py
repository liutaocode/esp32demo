#!/usr/bin/env python3
from pathlib import Path
import argparse,re,subprocess,json
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args()
src=''.join(f.read_text() for f in (R/'main/apps/listening').glob('*.c'))+(R/'main/apps/listening/catalog.json').read_text()
symbols=''.join(sorted({c for c in src if ord(c)>127}))
font=R/'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'
if not font.exists():font=R.parents[2]/'managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'
for size in [16,22]:
 out=R/f'assets/fonts/listening_zh_{size}.c'
 if a.check:
  have={int(x,16) for x in re.findall(r'U\+([0-9a-fA-F]+)',out.read_text())};assert not ({ord(c) for c in symbols}|set(range(32,127)))-have
 else:
  subprocess.run(['npx','--yes','lv_font_conv@1.5.3','--size',str(size),'--bpp','2','--format','lvgl','--font',str(font),'--symbols',symbols,'--range','0x20-0x7e','--no-compress','--no-kerning','--lv-include','lvgl.h','--lv-font-name',f'listening_zh_{size}','-o',str(out)],check=True)
  out.write_text(out.read_text().replace(str(R),'<project>'))
print('Chinese glyph coverage PASS:',len(symbols),'characters in both sizes')
