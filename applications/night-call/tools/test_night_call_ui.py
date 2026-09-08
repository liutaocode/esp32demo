#!/usr/bin/env python3
"""Compile the actual production UI with LVGL and execute every screen state."""
from pathlib import Path
import subprocess,tempfile
R=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='night-call-ui-') as d:
    b=Path(d)/'build'; out=Path(d)/'renders';out.mkdir()
    subprocess.run(['cmake','-S',str(R/'tests/night_call_ui'),'-B',str(b)],check=True,stdout=subprocess.DEVNULL)
    subprocess.run(['cmake','--build',str(b),'-j','6'],check=True,stdout=subprocess.DEVNULL)
    subprocess.run([str(b/'preview')],cwd=out,check=True)
