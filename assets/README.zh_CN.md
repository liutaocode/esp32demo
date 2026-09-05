<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

### 我的世界图鉴中文子集

- `fonts/minecraft_zh_16.c` 是从 Noto Sans CJK SC Regular 生成的 16 px、
  2 bpp LVGL 中文子集，只包含图鉴界面所需字符；拉丁字母与数字回退到 LVGL
  Montserrat 14，由 `main/CMakeLists.txt` 编译进固件。
- 来源：[Noto Sans CJK](https://github.com/notofonts/noto-cjk)，采用 SIL Open
  Font License 1.1；许可全文保存在 `fonts/OFL-NotoSansCJK.txt`。
- 重新生成时使用 `lv_font_conv`，把 `main/minecraft_guide.c` 中可见中文作为
  `--symbols`，并设置 `--size 16`、`--bpp 2` 和
  `--lv-fallback lv_font_montserrat_14`。必须保留 `--no-compress`：当前固件关闭
  `LV_USE_FONT_COMPRESSED`，压缩字模无法正常显示。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

### 我的世界图鉴条目介绍

- `music/minecraft_guide_entries/` 包含 20 段由项目自行撰写的中文 TTS，
  每个图鉴条目一段；均由 macOS Tingting 系统语音生成，为 16 kHz、16 位、
  单声道 PCM，不含第三方录音。
- `tools/generate_minecraft_guide_audio.py` 按固定顺序把 WAV 打包为
  `main/minecraft_guide_audio.c` 中的 IMA ADPCM。固件每批解码 512 个采样；
  用户切换到其他条目时，会中断当前语音并播放最新条目的介绍。

## 已送审应用资源

`fonts/` 中的应用字体源文件是 Noto Sans CJK 或 Source Han Sans 的 LVGL 子集，遵循本目录的 SIL OFL 1.1 许可。相应 `tools/generate_*` 脚本记录生成方法；固件直接编译生成的 C 文件。

`music/vibe_check/`、`music/idiom_pet/`、`music/word_sprite/`、`music/pvz_almanac/` 和 `music/laoluo_quotes/` 保存离线语音资源。系统语音为 macOS Tingting（中文）与 Samantha（英文）；清单记录文案及素材哈希。WAV 文件用于重建和检查，ADPCM 数据用于固件。老罗语录的引用来源列于应用的 `quotes.csv`。
