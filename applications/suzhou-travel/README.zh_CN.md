<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 苏州旅游指南

这是一个独立的 ESP-IDF 应用，把 FoloToy AI Passport 变成离线苏州旅行小指南。
240 × 320 界面以原创园林水墨横幅为主视觉，搭配宣纸色、青瓷绿、朱砂红和窗棂纹样，
呈现古色古香的苏州园林气质。

## 收录景点

拙政园、留园、虎丘、寒山寺、平江路、山塘街、狮子林、网师园、沧浪亭和金鸡湖，
共 10 个经典景点。

## 按键操作

- `UP`：上一个景点
- `DOWN`：下一个景点
- `OK`：在景点简介和游览看点之间切换

应用启动以及每次按下 UP、DOWN 或 OK 后，都会朗读当前页面。每个景点的简介与看点
各有一段离线 TTS；新操作会中断上一段语音并立即播放新内容，不会阻塞按键。

电量始终显示在右上角；可选电量计不可用时显示 `--`。应用完全离线，不保存位置或
个人数据。

## 构建

激活 ESP-IDF 5.5.3 后，在本目录执行：

```bash
idf.py set-target esp32c3
idf.py build
```

本文件夹完全自包含，带有独立的 BSP、默认配置、受保护分区表、依赖锁和 bootloader
Recovery 钩子，不依赖 Minecraft 固件仓库。之后把本文件夹新建为独立仓库时，也应保留
这些硬件契约。烧录时不得覆盖 `cardid` 与永久 Recovery 区域。

已经配置过的设备通过 USB 开发时，请使用上面的分段命令 `idf.py flash`。它会把
bootloader 写到 `0x0`、分区表写到 `0x8000`、应用写到 `0x10000`，同时保留
`cardid` 与永久 Recovery。合并文件 `FoloToy-Suzhou-Travel-full.bin` 只能作为
`0x0` 起始的完整镜像使用；不要把整个文件写到 `0x8000` 或 `0x10000`，否则设备会
无法解析分区表并在黑屏状态下循环重启。

黑屏时可通过 USB 查看日志：

```bash
idf.py -p <PORT> monitor
```

如果出现 `partition 0 invalid magic number 0x3e9`，说明完整镜像被写到了分区表地址。
按 `Ctrl+]` 退出监视器，再运行 `idf.py -p <PORT> flash` 修复三个正常分段。

无需硬件的状态机测试：

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_suzhou_travel_state.c main/suzhou_travel_state.c \
  -o /tmp/test_suzhou_travel_state
/tmp/test_suzhou_travel_state
```

激活 ESP-IDF 5.5.3 后，也可以运行完整的主机与固件检查：

```bash
./tools/validate.sh
```

检查会生成 `build/FoloToy-Suzhou-Travel-full.bin` 合并镜像，并验证 8 MB Flash
头、3 MB factory-app 上限、受保护的 `cardid` 与 Recovery 区域、分区表校验和以及
Recovery bootloader 钩子。

## 素材来源

园林横幅是使用 OpenAI 内置图像工具生成的项目原创素材。最终提示词要求：无文字的
苏州园林场景，包含月洞门、石桥、粉墙黛瓦、竹影、红枫和晨雾，以暖色宣纸上的克制
水墨设色风格呈现。生成原图与 240 × 96 预览图保存在 `assets/images/`，应用编译
的是 46,080 字节 RGB565 Flash 资源。

`tools/generate_assets.py` 使用 FFmpeg 重新生成 RGB565 C 源文件，并用
`lv_font_conv` 1.5.3 生成中文字库子集。字库来源是采用 SIL Open Font License 1.1 的
Noto Sans CJK SC Regular，字体原文件不会复制进应用。

`tools/generate_suzhou_tts.py` 使用 macOS 婷婷系统语音生成 20 段项目自编文案，
源文件保存在 `assets/music/suzhou_tts/`，格式为 16 kHz、16 位、单声道 WAV；随后
打包为 IMA-ADPCM，在设备上从 Flash 分块解码播放，不包含第三方录音。
