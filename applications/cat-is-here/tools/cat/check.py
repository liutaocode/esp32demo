#!/usr/bin/env python3
"""Run cat logic, persistence, sound, Chinese typography and production UI checks."""
from pathlib import Path
import os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
os.chdir(ROOT)
subprocess.run(['python3','tools/cat/font.py','--check'],check=True)
subprocess.run(['python3','tools/cat/audio.py','--check'],check=True)
with tempfile.TemporaryDirectory(prefix='cat-state-') as tmp:
    binary=str(Path(tmp)/'test-cat')
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Imain','-Imain/cat','tests/test_cat.c','main/cat/cat_state.c','main/cat/cat_control.c','main/cat/cat_sound.c','assets/music/cat/cat_clips.c','main/minecraft_adpcm.c','-lm','-o',binary],check=True)
    subprocess.run([binary],check=True)
    worker=str(Path(tmp)/'test-worker')
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror','-pthread','-Itests/cat_audio_stubs','-Imain','-Imain/cat','tests/test_cat_audio_runtime.c','main/cat/cat_audio_runtime.c','main/cat/cat_sound.c','assets/music/cat/cat_clips.c','main/minecraft_adpcm.c','-o',worker],check=True)
    for failure in range(5): subprocess.run([worker,str(failure)],check=True)
source=Path(os.environ.get('LVGL_SOURCE',str(ROOT/'managed_components/lvgl__lvgl')))
if not (source/'CMakeLists.txt').exists():
    raise SystemExit('LVGL missing: run the firmware gate to resolve locked dependencies, or set LVGL_SOURCE to the locked LVGL checkout.')
build=ROOT/'build/cat-host'; build.mkdir(parents=True,exist_ok=True)
with (build/'build.log').open('w') as log:
    for command in [['cmake','-S','tests/cat_ui','-B',str(build),'-DCMAKE_BUILD_TYPE=Debug',f'-DLVGL_SOURCE={source}'],['cmake','--build',str(build),'-j','8']]:
        result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
        if result.returncode: raise SystemExit(f'Host renderer build failed; see {build / "build.log"}')
subprocess.run([str(build/'preview')],check=True)
