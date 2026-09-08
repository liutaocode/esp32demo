[简体中文](README.zh_CN.md) | English

# English Listening Practice speech assets

384 original English sentences and Chinese translations are authored for this application in `main/apps/listening/catalog.json`; no textbook recordings or branded characters are used. Source WAVs, generated ADPCM banks and per-file hashes in `manifest.json` are retained for reproducibility and review.

Speech was synthesized using [Kokoro-82M v1.0](https://huggingface.co/hexgrad/Kokoro-82M), voice `af_heart`, speed 0.87. Its official model card lists Apache-2.0 weights and supports production use. Model SHA-256: `496dba118d1a58f5f3db2efc88dbdc216e0483fc89fe6e47ee1f2c53f18ad1e4`. This repository redistributes generated speech rather than model weights. Voice is synthetic, with no claim of human narration or a copied public figure.

FFmpeg converts to 16 kHz mono PCM, trims boundary silence while preserving internal pauses, and adds 180 ms trailing silence. Before encoding, each clip is peak-normalized to -1.5 dBFS from the immutable source WAV. The existing repository IMA ADPCM encoder creates deterministic banks. Original PCM is not linked into the application. Fonts derive from Source Han Sans (SIL Open Font License) using Chinese subsets; pixel artwork is original LVGL geometry.

Regeneration and integrity checks are in `tools/generate_listening_audio.py`. A physical listening review remains required before publication.
