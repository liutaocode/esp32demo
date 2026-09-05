[English](README.md) | 简体中文

# 草坪研究所——植物大战僵尸图鉴



面向 FoloToy AI Passport 的口袋同人图鉴：全中文界面、24 张原创像素插画、
48 段离线普通话 TTS。首批收录经典一代的 16 种植物和 8 种僵尸，属于精选入门版，
并非完整百科，也不是可玩的塔防游戏。

翻看角色能力与实用提示，听语音讲解，或把设备传给朋友玩五题线索挑战。
结果卡展示趣味称号和题组编号；同题再战会保留题目和选项，方便公平比成绩。
退出结果页后再次开启挑战，会生成新题组。

界面采用奶油纸色与森林绿的收藏手册配色。三级中文字体区分标题、正文和辅助信息；角色展台、简洁选中行与固定按键栏让 240 × 320 小屏更易阅读。语音状态集中显示在角色或线索旁边。

## 按键

| 页面 | 单击上／下 | 单击确定 | 长按 |
| --- | --- | --- | --- |
| 首页 | 选择植物、僵尸或挑战 | 进入 | 无 |
| 图鉴 | 上一张／下一张，在分类内循环 | 朗读名称、能力、提示 | 确定：回首页；上：重听 |
| 挑战 | 选择三个答案之一 | 提交，查看解析后继续 | 上：重听线索；确定：回首页 |
| 结果 | 无 | 同题再战 | 确定：回首页 |

语音线索也以文字展示，没有声音仍可答题。揭晓答案后自动朗读讲解。
翻页或回首页会停止旧语音，新请求会替换等待中的语音。本次认识的角色和成绩只在
应用运行期间保留，重启清空。运行时无需联网、账号、存储或云端 TTS。
闲置 60 秒会调暗屏幕，第一次按键只唤醒、不跳页。电量读取失败显示 `--%`。

## 构建与素材

激活 ESP-IDF 5.5.3 后，在仓库根目录运行：

```bash
FAP_APP=pvz_almanac ./tools/validate.sh
```

统一检查生成 `build/FoloToy-AI-Passport-full.bin`。构建其他应用前，请为本应用
保留独立命名的副本。默认应用不变。继续保留 3 MB 应用上限、设备身份与 Recovery
固定地址，以及上键五秒进入 Recovery 的启动钩子，详见
[BLE 安装契约](../../../docs/development/engineering/ble-recovery-compatibility.zh_CN.md)。

`catalog.csv` 是可编辑的内容源。素材脚本生成 C 图鉴、语音索引、嵌入式 ADPCM
语音包、语音清单和中文字体：

```bash
python3 tools/generate_pvz_assets.py --synthesize --font
python3 tools/generate_pvz_assets.py --check
```

重新生成需要 macOS 的 `say`（中性 Tingting 声音）、FFmpeg、
`lv_font_conv@1.5.3` 和已捆绑的思源黑体。正常构建使用现成资源，无需这些工具。
每个角色有一段完整讲解和一段不泄露名字的独立线索语音。

## 架构与检查

- `pvz_state.c`：确定性导航、分类边界、不重复题目、同分类干扰项、计分和本次收集位图。
- `pvz_view.c` / `pvz_art.c`：真实 LVGL 界面与通过代码绘制的原创插画。
- `pvz_almanac.c`：静态非阻塞输入队列、LVGL 定时器、电量和闲置调暗。
- `pvz_audio_runtime.c`：可中断语音工作任务，使用 512 字节 PCM 缓冲、带请求编号的
  状态和退出握手，不访问 LVGL。`enter`、`exit` 需在按键回调之外持有 LVGL 锁调用。
- 主机测试覆盖 12,000 局挑战、导航、计分边界、同题重玩、目录完整性和语音边界。
  素材检查将语音清单、压缩字节和索引与保存的 WAV 源文件逐项比对。

仍需在实机验收普通话发音、扬声器音量、播放中快速翻页、长按、闲置唤醒，以及
安装和 Recovery。固件构建或主机渲染通过不代表设备测试通过。

主机 LVGL 渲染与模拟按键／生命周期检查（需要 CMake 和 C 编译器）：

```bash
cmake -S tests/pvz_ui -B /tmp/pvz-ui-build
cmake --build /tmp/pvz-ui-build -j 4
mkdir -p /tmp/pvz-preview
/tmp/pvz-ui-build/pvz_ui_test /tmp/pvz-preview
/tmp/pvz-ui-build/pvz_input_test
```

## 内容与来源

角色名称及游戏标识属于各自权利人，本应用为非官方同人项目。简介与线索采用
独立撰写的中文摘要，插画使用 LVGL 图形绘制，不包含原版游戏贴图、音乐、
角色录音或照搬的图鉴趣味文案。经典一代玩法事实参考
[图鉴资料](https://gamefaqs.gamespot.com/pc/959255-plants-vs-zombies/faqs/61070)
和[白天关卡指南](https://strategywiki.org/wiki/Plants_vs._Zombies/Day)。
素材及来源记录在[素材索引](../../../assets/README.zh_CN.md)。
