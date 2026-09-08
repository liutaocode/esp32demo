#!/usr/bin/env python3
"""Build and run the actual LVGL page with hardware stubs, no serial access."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
if not (ROOT/'managed_components/lvgl__lvgl/CMakeLists.txt').exists():
    raise SystemExit('LVGL is missing: resolve the pinned ESP-IDF dependencies before the UI gate.')
with tempfile.TemporaryDirectory(prefix='rhythm-ui-') as tmp:
    tmp=Path(tmp);build=tmp/'build';output=Path(os.environ.get('RHYTHM_PREVIEW_DIR',str(tmp/'frames')));output.mkdir(parents=True,exist_ok=True)
    def run(args,cwd=ROOT):
        r=subprocess.run([str(x) for x in args],cwd=cwd,capture_output=True,text=True)
        if r.returncode: print(r.stdout);print(r.stderr);raise SystemExit(r.returncode)
        return r.stdout
    run(['cmake','-S',ROOT/'tests/rhythm_agent_ui','-B',build]);run(['cmake','--build',build,'-j','8'])
    print(run([build/'preview'],output).strip())
