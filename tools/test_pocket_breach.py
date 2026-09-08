#!/usr/bin/env python3
"""Run pure gameplay checks and production LVGL layout/input regression checks."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
def run(args, cwd=ROOT):
    p = subprocess.run(args, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.returncode:
        raise SystemExit(p.stdout)
    return p.stdout
with tempfile.TemporaryDirectory(prefix='pocket-breach-check-') as tmp:
    work = Path(tmp)
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-Imain/apps/pocket_breach', 'tests/test_pocket_breach.c',
         'main/apps/pocket_breach/pb_game.c', 'main/apps/pocket_breach/pb_render.c',
         'main/apps/pocket_breach/pb_audio.c', '-lm', '-o', str(work / 'game')])
    print(run([str(work / 'game')]).strip())
    run(['cmake', '-S', 'tests/pocket_breach_ui', '-B', str(work / 'ui')])
    run(['cmake', '--build', str(work / 'ui'), '-j', '8'])
    print(run([str(work / 'ui/preview')], cwd=work).strip())
    print(run([str(work / 'ui/capture_test')], cwd=work).strip())
