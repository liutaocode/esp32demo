# 我的世界语音图鉴

一款会说话的《我的世界》随身图鉴。按上、下键循环浏览 20 个生物、方块和装备；
每一页都有原创像素插图、出没地点和生存提示，切换页面时会自动介绍，按确定键可
再次播放当前说明。

[English](README.md)

![项目封面](assets/images/minecraft-guide-cover.png)

## 怎么玩

- 上键：查看上一个条目。
- 下键：查看下一个条目。
- 确定键：重播当前条目的语音介绍。

图鉴包含苦力怕、末影人、美西螈、末影龙、监守者、鞘翅等 20 个条目。

![运行画面](assets/images/minecraft-guide-runtime.png)

## 构建

项目基于 FoloToy AI Passport 官方固件仓库开发。准备好 ESP-IDF 5.5 环境后运行：

```bash
idf.py build
idf.py merge-bin
```

完整检查可运行：

```bash
./tools/validate.sh
```

## 素材与许可

- 中文字库子集来自 Noto Sans CJK，采用 SIL Open Font License 1.1；许可文件位于
  `assets/fonts/OFL-NotoSansCJK.txt`。
- 条目文案、像素插图和中文语音均为本项目制作。
- 基础项目来源：[folotoy/ai-passport](https://github.com/folotoy/ai-passport)。
- 本项目是非官方粉丝作品，与 Mojang Studios 或 Microsoft 无隶属关系。
