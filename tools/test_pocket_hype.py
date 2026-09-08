#!/usr/bin/env python3
"""Run portable state and threaded audio tests using the actual embedded pack."""
from pathlib import Path
import os
import platform
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'main/apps/pocket_hype'
def run(args): subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
with tempfile.TemporaryDirectory(prefix='pocket-hype-tests-') as tmp:
    tmp = Path(tmp)
    cc = [os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror']
    run(cc + ['-I'+str(APP), ROOT/'tests/test_pocket_hype_state.c', APP/'pocket_hype_state.c', APP/'pocket_hype_audio.c', APP/'pocket_hype_audio_index.c', '-o', tmp/'state'])
    run([tmp/'state'])
    section = '.section __TEXT,__const' if platform.system() == 'Darwin' else '.section .rodata'
    blob = ROOT/'assets/music/pocket_hype/pocket_hype_adpcm.bin'
    assembly = tmp/'audio.S'
    assembly.write_text(section+'\n.globl _binary_pocket_hype_adpcm_bin_start\n.globl _binary_pocket_hype_adpcm_bin_end\n_binary_pocket_hype_adpcm_bin_start:\n.incbin "'+str(blob)+'"\n_binary_pocket_hype_adpcm_bin_end:\n')
    run(cc + ['-pthread', '-I'+str(ROOT/'tests/down_100_audio_stubs'), '-I'+str(APP), '-I'+str(ROOT/'main'), ROOT/'tests/test_pocket_hype_audio_runtime.c', APP/'pocket_hype_voice.c', APP/'pocket_hype_audio.c', APP/'pocket_hype_audio_index.c', ROOT/'main/minecraft_adpcm.c', assembly, '-o', tmp/'audio'])
    for case in range(5): run([tmp/'audio', case])
