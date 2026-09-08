#!/usr/bin/env python3
"""Record the exact verified merged artifact; never opens a serial port."""
import hashlib,json,subprocess,sys
from datetime import datetime,timezone
from pathlib import Path
root=Path(__file__).resolve().parents[2]; image=Path(sys.argv[1]).resolve()
source_files=['main/main.c','main/fap_screenshot.c','main/CMakeLists.txt','sdkconfig.defaults','partitions.csv','dependencies.lock']
source_files += [str(p.relative_to(root)) for p in sorted((root/'main/cat').glob('*.[ch]'))]
source_files += ['assets/music/cat/cat_clips.c','main/minecraft_adpcm.c','main/minecraft_adpcm.h','assets/music/cat/audio-manifest.json']
source_files += [str(p.relative_to(root)) for p in sorted((root/'assets/fonts').glob('cat_zh_*.c'))]
files={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in source_files}
profile={'application':'cat-is-here','version':'0.1.2','verified_at':datetime.now(timezone.utc).isoformat(),'base_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'firmware':str(image.relative_to(root)),'bytes':image.stat().st_size,'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'firmware_gate':'PASS','device_tests':'NOT RUN','installation':'NOT INSTALLED (this build)','serial_capture':None,'cover':None,'submission':'NOT SUBMITTED','source_sha256':files}
(root/'build/firmware-manifest.json').write_text(json.dumps(profile,indent=2)+'\n')
print(f'Artifact: {profile["bytes"]} bytes; SHA256 {profile["sha256"]}')
