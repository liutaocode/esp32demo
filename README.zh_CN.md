[English](README.md)

# ESP32 Demo — 已送审应用源码

本仓库包含 21 个已送审应用的源码、离线资源和主机测试，每个固件运行一个应用。默认应用为气场测试。

## 构建

启用 ESP-IDF 5.5.3 后，在仓库根目录运行：

```bash
./tools/validate.sh --static
FAP_APP=vibe_check ./tools/validate.sh
FAP_APP=minecraft_guide ./tools/validate.sh --firmware
```

将 `FAP_APP` 替换为下表中的应用标识。输出为 `build/FoloToy-AI-Passport-full.bin`。更换应用后请重新构建。

安装前阅读 [BLE 安装与 Recovery 兼容性说明](docs/development/engineering/ble-recovery-compatibility.zh_CN.md)。

## 应用目录

| 应用标识 | 名称 | 源码 |
| --- | --- | --- |
| `balloon_rush` | 见好就收 | [source](main/apps/balloon_rush/balloon_rush.c) |
| `cloud_hop` | 再跳一步 | [source](main/apps/cloud_hop/cloud_hop.c) |
| `down_100` | 勇闯地下100层 | [source](main/apps/down_100/down_100.c) |
| `focus_post` | 注意力小邮差 | [source](main/apps/focus_post/focus_post.c) |
| `fruit_merge` | 再合一颗 | [source](main/apps/fruit_merge/fruit_merge.c) |
| `idiom_pet` | 成语萌兽：读故事，孵伙伴 | [source](main/apps/idiom_pet/idiom_pet.c) |
| `laoluo_quotes` | 老罗语录：口袋金句电台 | [source](main/apps/laoluo_quotes/laoluo_quotes.c) |
| `math_rail` | 口算旅行号 | [source](main/apps/math_rail/math_rail.c) |
| `math_train` | 口算小火车 | [source](main/apps/math_train/math_train.c) |
| `memory_garden` | 天天记一记 | [source](main/apps/memory_garden/memory_garden.c) |
| `needle_rush` | 再插一针 | [source](main/apps/needle_rush/needle_rush.c) |
| `perfect_slice` | 一刀刚好 | [source](main/apps/perfect_slice/perfect_slice.c) |
| `pocket_pond` | 口袋捞鱼 | [source](main/apps/pocket_pond/pocket_pond.c) |
| `pvz_almanac` | 草坪研究所：植物大战僵尸图鉴 | [source](main/apps/pvz_almanac/pvz_almanac.c) |
| `ricochet_rush` | 再弹一轮 | [source](main/apps/ricochet_rush/ricochet_rush.c) |
| `social_battery` | 社交电量牌｜你不必随时在线 | [source](main/apps/social_battery/social_battery.c) |
| `stack_rush` | 再叠一层 | [source](main/apps/stack_rush/stack_rush.c) |
| `tomato_bloom` | 番茄花园 | [source](main/apps/tomato_bloom/tomato_bloom.c) |
| `vibe_check` | 气场测试｜今天你是哪一种？ | [source](main/apps/vibe_check/vibe_check.c) |
| `word_sprite` | 单词精灵｜听单词，养出小精灵 | [source](main/apps/word_sprite/word_sprite.c) |
| `minecraft_guide` | 我的世界语音图鉴 | [source](main/minecraft_guide.c) |

## 资源与验证

字体和音频来源见 [资源说明](assets/README.zh_CN.md)，开发资料见 [文档索引](docs/README.zh_CN.md)。编译通过不代表完成实机验证。

基于 [FoloToy AI Passport](https://github.com/FoloToy/ai-passport)，保留其许可与署名。
