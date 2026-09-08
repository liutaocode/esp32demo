#!/usr/bin/env python3
"""Verify the pinned upstream libraries and voice data without network access."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    component = ROOT / "components/esp_tts"
    manifest = json.loads((component / "upstream.json").read_text())
    for item in manifest["files"]:
        data = (component / item["local"]).read_bytes()
        assert len(data) == item["size"], item["local"]
        assert hashlib.sha256(data).hexdigest() == item["sha256"], item["local"]
    component = ROOT / "components/speex"
    manifest = json.loads((component / "upstream.json").read_text())
    for path, digest in manifest["files"].items():
        assert hashlib.sha256((component / path).read_bytes()).hexdigest() == digest, path
    voice = (ROOT / "assets/music/chinese_tts/xiaole-compact.dat").read_bytes()
    assert len(voice) == 929972
    assert hashlib.sha256(voice).hexdigest() == "089be5781ea08ac94a3f2d227ca66c52fb57c6b14a906de1c39ceaf3097fcb92"
    original = (ROOT / "assets/music/chinese_tts/xiaole.dat").read_bytes()
    assert voice[20:40] == original[20:40]
    assert voice[7040:171640] == original[7040:171640], "pronunciation tables changed"
    print("Pinned ESP-TTS / Speex assets and compact voice: PASS")

if __name__ == "__main__":
    main()
