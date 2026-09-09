[English](README.en.md)

# 应用列表与源码

应用列表收录已上架应用，并标注源码入口与构建方式。原有 40 个应用中，31 个使用根目录构建选择器，9 个保留独立工程。新增的书架 TTS 版、朗读员和小豆使用各自目录独立构建。源码是整理后的开发快照，不承诺与社区固件逐字节一致。口袋游戏厅已有发布版本，同时存在新的草稿修订。

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

表格每项依次列出源码入口、构建选择方式和主要功能。标注 `FAP_APP=名称` 的应用，在仓库根目录运行 `FAP_APP=名称 ./tools/validate.sh`；标注 `cd 路径` 的独立工程，进入该目录后运行 `idf.py reconfigure` 和 `./tools/validate.sh`。运行前需激活 ESP-IDF 5.5.3。

| 社区 ID | 应用 | 源码 / 构建选择器 |
| --- | --- | --- |
| 233 | Qwen 语音豆 · 已上架 | [qwen-voice-bean](examples/qwen-voice-bean/README.zh_CN.md)<br>独立工程：`cd examples/qwen-voice-bean`<br>联网语音对话、Agent 任务、手机配网、下键打断 |
| 222 | 口袋书架（TTS版） · 已上架 | [bookshelf-tts](examples/bookshelf-tts/README.zh_CN.md)<br>独立工程：`cd examples/bookshelf-tts`<br>离线听书、三本测试书、音量调节 |
| 221 | 口袋朗读员 · 中文 TTS Demo · 已上架 | [chinese-tts](examples/chinese-tts/README.zh_CN.md)<br>独立工程：`cd examples/chinese-tts`<br>十二组示例、六档语速、自定义文本 API，语音库约 **930 KB** |
| 220 | 嘴硬小豆 | [mouthy-bean](examples/mouthy-bean/README.zh_CN.md)<br>独立工程：`cd examples/mouthy-bean`<br>竖耳倾听、随机思考、摸摸逗逗、八种表情；灵感来自「小狮日记」 |
| 213 | 随身书架 | [ebook](main/apps/ebook)<br>根目录构建：`FAP_APP=ebook`<br>纯文本阅读、手机传书、书签与阅读进度 |
| 204 | 口袋游戏厅 | [pocket_arcade](main/apps/pocket_arcade)<br>根目录构建：`FAP_APP=pocket_arcade`<br>32 款小游戏、玩法提示、成绩与星星收藏 |
| 203 | 一挤就过 | [jelly_squeeze](main/apps/jelly_squeeze)<br>根目录构建：`FAP_APP=jelly_squeeze`<br>调整果冻形状穿过闸门，两种难度与口味收藏 |
| 201 | 俄罗斯方块｜三个键玩的方块游戏 | [clean_sweep](main/apps/clean_sweep)<br>根目录构建：`FAP_APP=clean_sweep`<br>三键方块消行、经典无尽与二十行冲刺 |
| 195 | 点点有数 | [tally-click](applications/tally-click)<br>独立工程：`cd applications/tally-click`<br>加减计数、暂停锁定、最近十条记录 |
| 194 | 借过一下 | [excuse_call](main/apps/excuse_call)<br>根目录构建：`FAP_APP=excuse_call`<br>模拟来电、五种铃声、一键开始或停止 |
| 193 | 英语磨耳朵 | [listening-island](applications/listening-island)<br>独立工程：`cd applications/listening-island`<br>英语短句播放、听力选择题、错句回听 |
| 189 | 口袋突围 | [pocket_breach](main/apps/pocket_breach)<br>根目录构建：`FAP_APP=pocket_breach`<br>第一人称射击、三张地图、十二波敌人 |
| 188 | 小猫在呢 | [cat-is-here](applications/cat-is-here)<br>独立工程：`cd applications/cat-is-here`<br>摸猫陪玩、猫叫呼噜、装扮与回忆 |
| 187 | 飞跃车道 | [lane_leap](main/apps/lane_leap)<br>根目录构建：`FAP_APP=lane_leap`<br>三车道换道跳跃、躲障碍与收集金币 |
| 186 | 码上装忙 | [code_theater](main/apps/code_theater)<br>根目录构建：`FAP_APP=code_theater`<br>代码小人陪伴、模拟终端状态与按键互动 |
| 183 | 海昏侯博物馆口袋云游 | [haihunhou-museum](applications/haihunhou-museum)<br>独立工程：`cd applications/haihunhou-museum`<br>海昏侯文物图文浏览、故事与细看语音讲解 |
| 182 | 三星堆博物馆口袋云游 | [sanxingdui-museum](applications/sanxingdui-museum)<br>独立工程：`cd applications/sanxingdui-museum`<br>三星堆文物图鉴、翻页讲解与细节介绍 |
| 178 | 节奏特工｜听一遍，按回来 | [rhythm-agent](applications/rhythm-agent)<br>独立工程：`cd applications/rhythm-agent`<br>听节奏后按键复现、训练与挑战模式 |
| 177 | 深夜来电｜今夜，请别挂断 | [night-call](applications/night-call)<br>独立工程：`cd applications/night-call`<br>语音分支故事、三通来电、线索与多结局 |
| 176 | 截稿小站 | [deadline_station](main/apps/deadline_station)<br>根目录构建：`FAP_APP=deadline_station`<br>会议投稿倒计时、收藏、准备清单与专注计时 |
| 173 | 口袋捧场王｜这一刻，就差你捧个场 | [pocket_hype](main/apps/pocket_hype)<br>根目录构建：`FAP_APP=pocket_hype`<br>场景语音与音效、惊喜盲盒、接梗挑战 |
| 172 | 再弹一轮 | [ricochet_rush](main/apps/ricochet_rush)<br>根目录构建：`FAP_APP=ricochet_rush`<br>瞄准反弹小球、击碎数字砖块、三十轮挑战 |
| 171 | 天天记一记 | [memory_garden](main/apps/memory_garden)<br>根目录构建：`FAP_APP=memory_garden`<br>看图记顺序、三轮练习与花园成果卡 |
| 170 | 再合一颗 | [fruit_merge](main/apps/fruit_merge)<br>根目录构建：`FAP_APP=fruit_merge`<br>相邻同类水果合成、预览下一颗、单次撤回 |
| 169 | 草坪研究所：植物大战僵尸图鉴 | [pvz_almanac](main/apps/pvz_almanac)<br>根目录构建：`FAP_APP=pvz_almanac`<br>植物与僵尸语音图鉴、五题线索挑战 |
| 168 | 老罗语录：口袋金句电台 | [laoluo_quotes](main/apps/laoluo_quotes)<br>根目录构建：`FAP_APP=laoluo_quotes`<br>十二条金句卡片、中性合成语音朗读 |
| 167 | 注意力小邮差 | [focus_post](main/apps/focus_post)<br>根目录构建：`FAP_APP=focus_post`<br>辨认目标动物送信、观察与等待练习 |
| 166 | 勇闯地下100层 | [down_100](main/apps/down_100)<br>根目录构建：`FAP_APP=down_100`<br>左右移动下楼、躲尖刺、宝石与连击 |
| 164 | 再插一针 | [needle_rush](main/apps/needle_rush)<br>根目录构建：`FAP_APP=needle_rush`<br>向旋转圆盘插针、避障、十关挑战 |
| 162 | 单词精灵｜听单词，养出小精灵 | [word_sprite](main/apps/word_sprite)<br>根目录构建：`FAP_APP=word_sprite`<br>听英文选中文、错词复习与精灵成长 |
| 161 | 见好就收 | [just_seen](main/apps/just_seen)<br>根目录构建：`FAP_APP=just_seen`<br>气球充气时机挑战、继续冒险或收下积分 |
| 160 | 口算旅行号 | [math_rail](main/apps/math_rail)<br>根目录构建：`FAP_APP=math_rail`<br>随机口算、两键作答、错题练习与车站票 |
| 159 | 口袋捞鱼 | [pocket_pond](main/apps/pocket_pond)<br>根目录构建：`FAP_APP=pocket_pond`<br>捞鱼或收手、概率提示、鱼册与奖章 |
| 158 | 口算小火车 | [math_train](main/apps/math_train)<br>根目录构建：`FAP_APP=math_train`<br>四档随机口算、答题反馈与错题再练 |
| 157 | 成语萌兽：读故事，孵伙伴 | [idiom_pet](main/apps/idiom_pet)<br>根目录构建：`FAP_APP=idiom_pet`<br>成语故事选择题、语音讲解、萌兽收集 |
| 156 | 气场测试｜今天你是哪一种？ | [vibe_check](main/apps/vibe_check)<br>根目录构建：`FAP_APP=vibe_check`<br>五道二选一题、语音播报与趣味人格卡 |
| 155 | 再跳一步 | [cloud_hop](main/apps/cloud_hop)<br>根目录构建：`FAP_APP=cloud_hop`<br>浮岛跳跃、精准连击、同题重赛 |
| 154 | 一刀刚好 | [perfect_slice](main/apps/perfect_slice)<br>根目录构建：`FAP_APP=perfect_slice`<br>按目标比例切蛋糕、两种难度、十刀计分 |
| 153 | 社交电量牌｜你不必随时在线 | [social_battery](main/apps/social_battery)<br>根目录构建：`FAP_APP=social_battery`<br>四种社交状态展示、锁定与独处计时 |
| 152 | 再叠一层 | [stack_rush](main/apps/stack_rush)<br>根目录构建：`FAP_APP=stack_rush`<br>对齐叠楼、精准恢复宽度、五十层挑战 |
| 151 | 番茄花园 | [tomato_bloom](main/apps/tomato_bloom)<br>根目录构建：`FAP_APP=tomato_bloom`<br>专注计时、番茄成长、休息与收获统计 |
| 150 | 姑苏十景·随身语音导览 | [suzhou-travel](applications/suzhou-travel)<br>独立工程：`cd applications/suzhou-travel`<br>姑苏十景图文语音导览、简介与看点切换 |
| 149 | 六悦博物馆口袋云游 | [six-arts-museum](applications/six-arts-museum)<br>独立工程：`cd applications/six-arts-museum`<br>博物馆展品浏览、故事与细节语音导览 |
| 148 | 我的世界语音图鉴 | [minecraft_guide](main/minecraft_guide.c)<br>根目录构建：`FAP_APP=minecraft_guide`<br>二十个角色与物品图鉴、翻页自动讲解 |

验证区分构建、主机测试与实机测试。参见[文档索引](docs/README.zh_CN.md)和[归档技能](skills/plays-archive/SKILL.zh_CN.md)。
