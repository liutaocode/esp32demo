#!/usr/bin/env python3
"""Portable state, waveform and threaded production audio tests."""
from pathlib import Path
import os, platform, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1];APP=ROOT/'main/apps/rhythm_agent'
def run(args):subprocess.run([str(x) for x in args],cwd=ROOT,check=True)
with tempfile.TemporaryDirectory(prefix='rhythm-tests-') as tmp:
    tmp=Path(tmp);cc=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror']
    run(cc+['-I'+str(APP),ROOT/'tests/test_rhythm_state.c',APP/'rhythm_state.c',APP/'rhythm_tone.c','-o',tmp/'state']);run([tmp/'state'])
    section='.section __TEXT,__const' if platform.system()=='Darwin' else '.section .rodata'
    blob=ROOT/'assets/music/rhythm_agent/rhythm_voice.bin';asm=tmp/'audio.S'
    asm.write_text(section+'\n.globl _binary_rhythm_voice_bin_start\n.globl _binary_rhythm_voice_bin_end\n_binary_rhythm_voice_bin_start:\n.incbin "'+str(blob)+'"\n_binary_rhythm_voice_bin_end:\n')
    run(cc+['-pthread','-I'+str(ROOT/'tests/rhythm_audio_stubs'),'-I'+str(APP),'-I'+str(ROOT/'main'),ROOT/'tests/test_rhythm_audio.c',APP/'rhythm_audio.c',APP/'rhythm_tone.c',APP/'rhythm_voice_index.c',ROOT/'main/minecraft_adpcm.c',asm,'-o',tmp/'audio'])
    for case in range(5):run([tmp/'audio',case])
    run(cc+['-pthread','-I'+str(ROOT/'tests/rhythm_audio_stubs'),'-I'+str(APP),ROOT/'tests/test_rhythm_service.c',APP/'rhythm_service.c','-o',tmp/'service'])
    for case in range(4):run([tmp/'service',case])
