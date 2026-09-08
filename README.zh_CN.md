[English](README.md)

# 已发布应用源码

收录 40 个社区已发布应用。31 个使用根目录构建选择器，9 个保留独立工程。源码是整理后的开发快照，不承诺与社区固件逐字节一致。口袋游戏厅已有发布版本，同时存在新的草稿修订。

## 构建

```bash
# ESP-IDF 5.5.3; resolve pinned dependencies on a fresh clone.
idf.py reconfigure
FAP_APP=vibe_check ./tools/validate.sh
# Standalone example
cd applications/suzhou-travel
idf.py reconfigure
./tools/validate.sh
```

## 应用

| 社区 ID | 应用 | 源码 / 构建选择器 |
| --- | --- | --- |
| 213 | 随身书架 | [ebook](main/apps/ebook) |
| 204 | 口袋游戏厅 | [pocket_arcade](main/apps/pocket_arcade) |
| 203 | 一挤就过 | [jelly_squeeze](main/apps/jelly_squeeze) |
| 201 | 俄罗斯方块｜三个键玩的方块游戏 | [clean_sweep](main/apps/clean_sweep) |
| 195 | 点点有数 | [tally-click](applications/tally-click) |
| 194 | 借过一下 | [excuse_call](main/apps/excuse_call) |
| 193 | 英语磨耳朵 | [listening-island](applications/listening-island) |
| 189 | 口袋突围 | [pocket_breach](main/apps/pocket_breach) |
| 188 | 小猫在呢 | [cat-is-here](applications/cat-is-here) |
| 187 | 飞跃车道 | [lane_leap](main/apps/lane_leap) |
| 186 | 码上装忙 | [code_theater](main/apps/code_theater) |
| 183 | 海昏侯博物馆口袋云游 | [haihunhou-museum](applications/haihunhou-museum) |
| 182 | 三星堆博物馆口袋云游 | [sanxingdui-museum](applications/sanxingdui-museum) |
| 178 | 节奏特工｜听一遍，按回来 | [rhythm-agent](applications/rhythm-agent) |
| 177 | 深夜来电｜今夜，请别挂断 | [night-call](applications/night-call) |
| 176 | 截稿小站 | [deadline_station](main/apps/deadline_station) |
| 173 | 口袋捧场王｜这一刻，就差你捧个场 | [pocket_hype](main/apps/pocket_hype) |
| 172 | 再弹一轮 | [ricochet_rush](main/apps/ricochet_rush) |
| 171 | 天天记一记 | [memory_garden](main/apps/memory_garden) |
| 170 | 再合一颗 | [fruit_merge](main/apps/fruit_merge) |
| 169 | 草坪研究所：植物大战僵尸图鉴 | [pvz_almanac](main/apps/pvz_almanac) |
| 168 | 老罗语录：口袋金句电台 | [laoluo_quotes](main/apps/laoluo_quotes) |
| 167 | 注意力小邮差 | [focus_post](main/apps/focus_post) |
| 166 | 勇闯地下100层 | [down_100](main/apps/down_100) |
| 164 | 再插一针 | [needle_rush](main/apps/needle_rush) |
| 162 | 单词精灵｜听单词，养出小精灵 | [word_sprite](main/apps/word_sprite) |
| 161 | 见好就收 | [just_seen](main/apps/just_seen) |
| 160 | 口算旅行号 | [math_rail](main/apps/math_rail) |
| 159 | 口袋捞鱼 | [pocket_pond](main/apps/pocket_pond) |
| 158 | 口算小火车 | [math_train](main/apps/math_train) |
| 157 | 成语萌兽：读故事，孵伙伴 | [idiom_pet](main/apps/idiom_pet) |
| 156 | 气场测试｜今天你是哪一种？ | [vibe_check](main/apps/vibe_check) |
| 155 | 再跳一步 | [cloud_hop](main/apps/cloud_hop) |
| 154 | 一刀刚好 | [perfect_slice](main/apps/perfect_slice) |
| 153 | 社交电量牌｜你不必随时在线 | [social_battery](main/apps/social_battery) |
| 152 | 再叠一层 | [stack_rush](main/apps/stack_rush) |
| 151 | 番茄花园 | [tomato_bloom](main/apps/tomato_bloom) |
| 150 | 姑苏十景·随身语音导览 | [suzhou-travel](applications/suzhou-travel) |
| 149 | 六悦博物馆口袋云游 | [six-arts-museum](applications/six-arts-museum) |
| 148 | 我的世界语音图鉴 | [minecraft_guide](main/minecraft_guide.c) |

验证区分构建、主机测试与实机测试。参见[文档索引](docs/README.zh_CN.md)和[归档技能](skills/plays-archive/SKILL.zh_CN.md)。
