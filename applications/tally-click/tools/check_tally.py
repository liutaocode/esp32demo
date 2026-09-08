#!/usr/bin/env python3
"""Run the app state, production-worker and real-LVGL layout checks."""
from pathlib import Path
import os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
def run(args,**kw): subprocess.run([str(x) for x in args],check=True,**kw)
run(['python3','tools/generate_tally_fonts.py','--check'])
run(['python3','tools/generate_tally_audio.py','--check'])
with tempfile.TemporaryDirectory(prefix='tally-check-') as work:
    work=Path(work)
    flags=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror']
    run(flags+['-Imain/tally','tests/test_tally_state.c','main/tally/tally_state.c','-o',work/'state'])
    run([work/'state'])
    run(flags+['-Itests/tally_runtime_stubs','-Imain/tally','tests/test_tally_runtime.c','main/tally/tally_state.c','-o',work/'runtime'])
    for scenario in range(8): run([work/'runtime',scenario])
    run(flags+['-Imain/tally','tests/test_tally_sound.c','main/tally/tally_sound.c','-lm','-o',work/'sound'])
    run([work/'sound'])
    run(flags+['-Itests/tally_runtime_stubs','-Imain/tally','tests/test_tally_audio.c','main/tally/tally_sound.c','-lm','-o',work/'audio'])
    for scenario in range(3): run([work/'audio',scenario])
    # Cached generated files live in the ignored build directory.
    build=ROOT/'build/host-tally'
    with (work/'cmake.log').open('w') as log:
        try:
            run(['cmake','-S','tests/tally_ui','-B',build,'-DCMAKE_BUILD_TYPE=Debug'],stdout=log,stderr=subprocess.STDOUT)
            run(['cmake','--build',build,'-j8'],stdout=log,stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError:
            print((work/'cmake.log').read_text()); raise
    run([build/'preview'],cwd=work)
print('Tally app gate: PASS')
