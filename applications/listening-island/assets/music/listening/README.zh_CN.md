[English](README.md) | 简体中文

# 英语磨耳朵语音素材

`main/apps/listening/catalog.json` 中的 384 句英语及中文译文为本应用原创，没有使用教材录音或品牌角色。保留源 WAV、生成的 ADPCM 包和 `manifest.json` 中每个文件的哈希，方便复现与审核。

语音使用 [Kokoro-82M v1.0](https://huggingface.co/hexgrad/Kokoro-82M)，声音 `af_heart`，速度 0.87。官方模型卡标注权重采用 Apache-2.0，支持生产用途。模型 SHA-256：`496dba118d1a58f5f3db2efc88dbdc216e0483fc89fe6e47ee1f2c53f18ad1e4`。本工程分发合成语音，不分发模型权重。不宣称真人配音，也没有模仿公众人物。

FFmpeg 转为 16 kHz 单声道 PCM，仅裁剪首尾静音，保留句内停顿并添加 180 毫秒尾部静音。编码前从保留的原始 WAV 将每句峰值归一化至 -1.5 dBFS，再使用仓库已有 IMA ADPCM 编码器生成可复现资源。原始 PCM 不进入固件。字体来自 Source Han Sans（SIL Open Font License）的中文字形子集；像素画由原创 LVGL 图形构成。

资源生成及完整性检查由 `tools/generate_listening_audio.py` 实现。发布前仍需在实体设备上试听。
