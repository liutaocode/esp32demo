#!/usr/bin/env python3
"""Exercise every voice frame with the same fixed-point decoder configuration."""
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sources = re.findall(r'"(libspeex/[^" ]+\.c)"', (ROOT / 'components/speex/CMakeLists.txt').read_text())
with tempfile.TemporaryDirectory(prefix='tts-decoder-') as tmp:
    binary = str(Path(tmp) / 'test_decoder')
    command = [os.environ.get('CC', 'cc'), '-O2', '-DFIXED_POINT',
               '-DDISABLE_FLOAT_API', '-DDISABLE_ENCODER', '-DDISABLE_VBR', '-DEXPORT=',
               '-DSPEEX_MAJOR_VERSION=1', '-DSPEEX_MINOR_VERSION=2', '-DSPEEX_MICRO_VERSION=1',
               '-DSPEEX_EXTRA_VERSION=""', '-DSPEEX_VERSION="1.2.1"',
               '-Imain', '-Icomponents/speex/include', '-Icomponents/speex/libspeex',
               'tests/test_tts_decode.c', 'main/tts_bank.c']
    command += ['components/speex/' + source for source in sources]
    subprocess.run(command + ['-lm', '-o', binary], cwd=ROOT, check=True)
    subprocess.run([binary, 'assets/music/chinese_tts/xiaole-compact.dat'], cwd=ROOT, check=True)
