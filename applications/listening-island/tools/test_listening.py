#!/usr/bin/env python3
"""Compile state/codec tests and inspect/render the production LVGL UI."""
from pathlib import Path
import subprocess,tempfile,os
R=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='listening-tests-') as d:
 t=Path(d)
 for name,sources in [('state',['tests/test_listening_state.c','main/apps/listening/listening_state.c']),('catalog',['tests/test_listening_catalog.c','main/apps/listening/listening_catalog_check.c','main/apps/listening/listening_catalog.c','main/minecraft_adpcm.c'])]:
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Imain','-Imain/apps/listening',*sources,'-o',str(t/name)],cwd=R,check=True)
  subprocess.run([str(t/name)],cwd=R,check=True)
 # macOS supports the .incbin labels used by this native concurrency harness.
 if __import__('sys').platform=='darwin':
  exe=t/'runtime'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-pthread','-Itests/listening_runtime_stubs','-Imain','-Imain/apps/listening','tests/test_listening_runtime.c','main/apps/listening/listening_runtime.c','main/apps/listening/listening_catalog.c','main/apps/listening/listening_catalog_check.c','main/minecraft_adpcm.c','-o',str(exe)],cwd=R,check=True)
  for scenario in range(7):subprocess.run([str(exe),str(scenario)],cwd=R,check=True)
 b=t/'ui'
 subprocess.run(['cmake','-S',str(R/'tests/listening_ui'),'-B',str(b)],check=True,stdout=subprocess.DEVNULL)
 subprocess.run(['cmake','--build',str(b),'-j','8'],check=True,stdout=subprocess.DEVNULL)
 subprocess.run([str(b/'preview')],cwd=t,check=True)
