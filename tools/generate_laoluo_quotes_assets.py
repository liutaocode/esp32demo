#!/usr/bin/env python3
"""Generate the Lao Luo quote catalog, neutral TTS pack, and LVGL font subset."""

from __future__ import annotations

import argparse
import csv
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from minecraft_adpcm_encode import encode


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "main" / "apps" / "laoluo_quotes"
CATALOG = APP_DIR / "quotes.csv"
AUDIO_DIR = ROOT / "assets" / "music" / "laoluo_quotes"
CATALOG_C = APP_DIR / "laoluo_quotes_catalog.c"
AUDIO_C = APP_DIR / "laoluo_quotes_audio.c"
FONT_C = ROOT / "assets" / "fonts" / "laoluo_quotes_zh_18.c"
UI_TEXT = (
    "老罗语录金句行动理想思考坦荡归来彪悍转型热爱产品选择"
    "合成语音非本人录音确定朗读正在播放朗读完毕语音不可用"
    "按键不可用上一个下一个罗永浩第条共收藏今日失败重播—…："
)


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def load_rows() -> list[dict[str, str]]:
    with CATALOG.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    required = {"id", "category", "quote", "source_title", "source_url"}
    if not rows or set(rows[0]) != required:
        raise SystemExit(f"{CATALOG}: expected columns {sorted(required)}")
    if len({row["id"] for row in rows}) != len(rows):
        raise SystemExit(f"{CATALOG}: quote ids must be unique")
    return rows


def synthesize(rows: list[dict[str, str]], voice: str) -> None:
    AUDIO_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="laoluo-tts-") as temporary:
        temp = Path(temporary)
        for row in rows:
            wav_path = AUDIO_DIR / f'{row["id"]}.wav'
            aiff_path = temp / f'{row["id"]}.aiff'
            subprocess.run(
                ["say", "-v", voice, "-r", "182", "-o", str(aiff_path), row["quote"]],
                check=True,
            )
            subprocess.run(
                [
                    "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                    "-i", str(aiff_path), "-ar", "16000", "-ac", "1",
                    "-c:a", "pcm_s16le", str(wav_path),
                ],
                check=True,
            )


def read_clip(path: Path) -> tuple[bytes, int, int, int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2 or wav.getframerate() != 16000:
            raise SystemExit(f"{path}: expected 16 kHz, 16-bit, mono PCM WAV")
        frames = wav.readframes(wav.getnframes())
    samples = struct.unpack(f"<{len(frames) // 2}h", frames)
    data, predictor, index = encode(samples)
    return data, len(samples), predictor, index


def write_catalog(rows: list[dict[str, str]]) -> None:
    lines = [
        '#include "laoluo_quotes_catalog.h"',
        "",
        "const laoluo_quote_t laoluo_quotes_catalog[] = {",
    ]
    for row in rows:
        lines.append(
            f'    {{ .category = {c_string(row["category"])}, '
            f'.text = {c_string(row["quote"])} }},'
        )
    lines.extend([
        "};",
        "const size_t laoluo_quotes_catalog_count =",
        "    sizeof(laoluo_quotes_catalog) / sizeof(laoluo_quotes_catalog[0]);",
        "",
    ])
    CATALOG_C.write_text("\n".join(lines), encoding="utf-8")


def write_audio(rows: list[dict[str, str]]) -> None:
    clips = [read_clip(AUDIO_DIR / f'{row["id"]}.wav') for row in rows]
    packed = b"".join(clip[0] for clip in clips)
    lines = [
        '#include "laoluo_quotes_audio.h"',
        "",
        "const uint8_t laoluo_audio_data[] = {",
    ]
    for offset in range(0, len(packed), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in packed[offset:offset + 16])
        lines.append(f"    {row},")
    lines.extend([
        "};",
        f"const size_t laoluo_audio_data_size = {len(packed)};",
        "",
        "const laoluo_audio_clip_t laoluo_audio_clips[] = {",
    ])
    offset = 0
    for data, sample_count, predictor, index in clips:
        lines.append(
            "    { .offset = %d, .adpcm_size = %d, .sample_count = %d, "
            ".initial_predictor = %d, .initial_step_index = %d },"
            % (offset, len(data), sample_count, predictor, index)
        )
        offset += len(data)
    lines.extend([
        "};",
        f"const size_t laoluo_audio_clip_count = {len(clips)};",
        "",
    ])
    AUDIO_C.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {len(clips)} clips and {len(packed)} ADPCM bytes to {AUDIO_C}")


def write_font(rows: list[dict[str, str]], font_path: Path) -> None:
    symbols = "".join(row["category"] + row["quote"] for row in rows) + UI_TEXT
    symbols = "".join(dict.fromkeys(symbols))
    subprocess.run(
        [
            "npx", "--yes", "lv_font_conv@1.5.3",
            "--size", "18", "--bpp", "2", "--format", "lvgl",
            "--font", str(font_path), "--symbols", symbols,
            "--no-compress", "--no-kerning", "--lv-include", "lvgl.h",
            "--lv-font-name", "laoluo_quotes_zh_18",
            "--lv-fallback", "lv_font_montserrat_14", "-o", str(FONT_C),
        ],
        check=True,
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--synthesize", action="store_true",
                        help="regenerate WAV files with the macOS say command")
    parser.add_argument("--voice", default="Tingting",
                        help="neutral macOS Chinese voice used with --synthesize")
    parser.add_argument("--font", type=Path,
                        help="Noto Sans CJK SC OTF used to regenerate the LVGL font")
    args = parser.parse_args()

    rows = load_rows()
    write_catalog(rows)
    if args.synthesize:
        synthesize(rows, args.voice)
    write_audio(rows)
    if args.font:
        write_font(rows, args.font)


if __name__ == "__main__":
    main()
