#!/usr/bin/env python3
"""Host behavior, Chinese glyph coverage, shipped speech and real LVGL geometry."""
from pathlib import Path
import argparse
import os
import platform
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ui',action='store_true')
args=parser.parse_args()
def run(cmd,**kw):subprocess.run([str(v) for v in cmd],check=True,cwd=ROOT,**kw)
with tempfile.TemporaryDirectory(prefix='bean-test-') as tmp:
    out=Path(tmp)/'state'
    run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Imain','tests/test_bean_state.c','main/bean_state.c','-o',out]);run([out])
with tempfile.TemporaryDirectory(prefix='bean-runtime-') as tmp:
    tmp=Path(tmp); assembly=tmp/'audio.S'
    section='.section __TEXT,__const' if platform.system()=='Darwin' else '.section .rodata'
    blob=ROOT/'assets/music/mouthy_bean/mouthy_bean_adpcm.bin'
    assembly.write_text(section+'\n.globl _binary_mouthy_bean_adpcm_bin_start\n.globl _binary_mouthy_bean_adpcm_bin_end\n_binary_mouthy_bean_adpcm_bin_start:\n.incbin "'+str(blob)+'"\n_binary_mouthy_bean_adpcm_bin_end:\n')
    run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-pthread',
        '-Itests/bean_runtime_stubs','-Imain','-Imain',
        'tests/test_bean_runtime.c','main/bean_runtime.c',
        'main/bean_state.c','main/bean_audio_index.c',
        'main/minecraft_adpcm.c',assembly,'-o',tmp/'runtime'])
    for case in range(7):run([tmp/'runtime',case])
run([sys.executable,'tools/generate_mouthy_bean_font.py','--check'])
run([sys.executable,'tools/generate_mouthy_bean_audio.py','--check'])
if args.ui:
    build=ROOT/'build/ui'
    preview=ROOT/'build/mouthy-bean/preview';preview.mkdir(parents=True,exist_ok=True)
    run(['cmake','-S','tests/mouthy_bean_ui','-B',build],stdout=subprocess.DEVNULL)
    run(['cmake','--build',build,'-j','8'],stdout=subprocess.DEVNULL)
    subprocess.run([str(build/'preview')],cwd=preview,check=True)
