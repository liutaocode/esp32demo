#!/usr/bin/env python3
"""Require the actual audio resource bytes in the verified merged delivery file."""
import hashlib,json,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1];b=Path(sys.argv[1]);merged=(b/'FoloToy-AI-Passport-full.bin').read_bytes()
audio=(R/'assets/music/listening/listening_b.bin').read_bytes()
assert 0<len(audio)<=0x3A0000
assert merged[0x360000:0x360000+len(audio)]==audio
assert all(x==255 for x in merged[0x356000:0x35A000])
print('Listening audio resource: PASS (included at 0x360000; protected identity region remains padding)')
report=dict(firmware_bytes=len(merged),firmware_sha256=hashlib.sha256(merged).hexdigest(),application_bytes=(b/'FoloToy-AI-Passport.bin').stat().st_size,resource_bytes=len(audio),resource_sha256=hashlib.sha256(audio).hexdigest())
(b/'listening-artifact.json').write_text(json.dumps(report,indent=2)+'\n')
