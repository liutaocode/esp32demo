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

### 海昏侯国遗址博物馆中文子集

- `fonts/haihunhou_zh_16.c` 是为博物馆云游界面可见中文生成的 16 px、2 bpp LVGL
  子集，来源为 Noto Sans CJK SC Regular；拉丁字母和数字回退到 LVGL
  Montserrat 14，并由 `main/CMakeLists.txt` 编译。
- 字体来源：[Noto Sans CJK](https://github.com/notofonts/noto-cjk)，采用 SIL Open
  Font License 1.1；共用许可文本位于 `fonts/OFL-NotoSansCJK.txt`。
- 使用 `main/haihunhou_museum.c` 中的可见中文重新生成时，运行
  `python3 tools/generate_haihunhou_font.py /path/to/NotoSansCJKsc-Regular.otf`；
  脚本固定使用 `lv_font_conv` 1.5.3、16 px、2 bpp、无压缩和
  `lv_font_montserrat_14` 回退。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

### 海昏侯国遗址博物馆 README 图片

- `images/haihunhou-museum-cover.png` 是为本项目制作的 1086 × 1448 社区封面，
  取材于项目原创的展品构思与运行画面风格，不含博物馆照片、标志或第三方画作。
- `images/haihunhou-museum-runtime.png` 是 240 × 320 的云游序厅画面抓取，只包含
  应用界面，不含设备或账户标识。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

### 海昏侯国遗址博物馆离线云导游

- `music/haihunhou_tts/` 保存 22 段 WAV 源文件，按页面顺序采用
  `01_site_story.wav` 至 `22_location_detail.wav` 的命名方式。
- 全部语音来自项目自编讲解文案，由 `tools/generate_haihunhou_tts.py` 使用 macOS
  婷婷系统音色生成，不包含博物馆或其他第三方录音。
- WAV 统一为 16 kHz、16 位、单声道 PCM；同一脚本随后将它们压缩打包成
  `main/haihunhou_tts_audio.c` 的 4-bit IMA-ADPCM 数据。
- 固件从 Flash 分块解码到小型 PCM 缓冲区，再交给 `bsp_audio_write()` 播放，
  不会把整段音频载入内部 RAM。
