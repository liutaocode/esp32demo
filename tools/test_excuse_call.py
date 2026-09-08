#!/usr/bin/env python3
"""Host checks for timing, generated PCM, worker failures and the production UI."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
def run(args, **kw):
    subprocess.run(args, check=True, **kw)
with tempfile.TemporaryDirectory(prefix='excuse-call-test-') as tmp:
    base=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Imain/apps/excuse_call']
    run(base+['tests/test_excuse_call_state.c','main/apps/excuse_call/excuse_call_state.c','main/apps/excuse_call/excuse_call_audio.c','assets/music/excuse_call/excuse_call_pcm.c','-lm','-o',tmp+'/state'])
    run([tmp+'/state'])
    run(base+['-pthread','-Itests/excuse_call_ui/stubs','-Itests/down_100_audio_stubs','-Itests/word_sprite_ui/stubs','tests/test_excuse_call_runtime.c','main/apps/excuse_call/excuse_call_runtime.c','main/apps/excuse_call/excuse_call_audio.c','assets/music/excuse_call/excuse_call_pcm.c','-lm','-o',tmp+'/runtime'])
    for case in range(5): run([tmp+'/runtime',str(case)])
    run(['python3','tools/generate_excuse_call_font.py','--check'])
    run(['python3','tools/generate_excuse_call_audio.py','--check'])
    with open(tmp+'/build.log','w') as log:
        try:
            run(['cmake','-S','tests/excuse_call_ui','-B',tmp+'/ui'],stdout=log,stderr=log)
            run(['cmake','--build',tmp+'/ui','-j','8'],stdout=log,stderr=log)
        except subprocess.CalledProcessError:
            print(Path(tmp+'/build.log').read_text()[-6000:]); raise
    run([tmp+'/ui/preview'],cwd=tmp)
print('Excuse Call host gate: PASS')
