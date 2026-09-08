#!/usr/bin/env python3
"""Run portable state and threaded audio tests using the actual embedded pack."""
from pathlib import Path
import os
import platform
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'main/apps/night_call'
def run(args): subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
with tempfile.TemporaryDirectory(prefix='night-call-tests-') as tmp:
    tmp = Path(tmp)
    cc = [os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror']
    run(cc + ['-I'+str(APP), ROOT/'tests/test_night_call_state.c', APP/'night_call_state.c', APP/'night_call_story.c', APP/'night_call_audio.c', APP/'night_call_audio_index.c', '-o', tmp/'state'])
    run([tmp/'state'])
    section = '.section __TEXT,__const' if platform.system() == 'Darwin' else '.section .rodata'
    blob = ROOT/'assets/music/night_call/night_call_adpcm.bin'
    assembly = tmp/'audio.S'
    assembly.write_text(section+'\n.globl _binary_night_call_adpcm_bin_start\n.globl _binary_night_call_adpcm_bin_end\n_binary_night_call_adpcm_bin_start:\n.incbin "'+str(blob)+'"\n_binary_night_call_adpcm_bin_end:\n')
    run(cc + ['-pthread', '-I'+str(ROOT/'tests/night_call_audio_stubs'), '-I'+str(APP), '-I'+str(ROOT/'main'), ROOT/'tests/test_night_call_audio_runtime.c', APP/'night_call_voice.c', APP/'night_call_audio.c', APP/'night_call_audio_index.c', ROOT/'main/minecraft_adpcm.c', assembly, '-o', tmp/'audio'])
    for case in range(5): run([tmp/'audio', case])

    run(cc + ['-pthread', '-I'+str(ROOT/'tests/night_call_audio_stubs'), '-I'+str(APP), ROOT/'tests/test_night_call_storage.c', APP/'night_call_storage.c', APP/'night_call_state.c', APP/'night_call_story.c', '-o', tmp/'storage'])
    for case in range(6): run([tmp/'storage', case])
