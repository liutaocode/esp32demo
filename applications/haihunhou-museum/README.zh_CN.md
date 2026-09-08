<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 海昏侯博物馆口袋云游

<p align="center">
  <img src="assets/images/haihunhou-museum-cover.png" alt="海昏侯博物馆口袋云游封面" width="480">
</p>

这是一款可离线使用的十一站博物馆云导游。从刘贺由昌邑王、短暂即位到获封海昏侯
的人生出发，再借黄金、青铜灯、漆画、简牍、编钟和钱币，拼出一位西汉诸侯的生活。

每一站都有原创像素插图，并配“故事”和“细看”两段中文讲解，共 22 段离线语音。

## 画面预览

<p align="center">
  <img src="assets/images/haihunhou-museum-runtime.png" alt="海昏侯博物馆雁鱼青铜灯页面" width="240">
</p>

## 怎么玩

- 上键：上一站。
- 下键：下一站。
- 确定键：切换当前页的“故事”和“细看”。

启动、翻页和切换视图都会自动朗读。新的操作会立即打断上一段讲解，浏览和按键不会
被长语音卡住。

## 十一站路线

路线包括刘贺、马蹄金与麟趾金、金饼、雁鱼青铜灯、孔子衣镜、《论语》简、编钟、
五铢钱、馆长彭明瀚和到馆信息。这是一条便于理解的策展式路线，不是馆方发布的文物
价值排名。

## 调研依据

海昏侯国遗址把侯国都城、墓园、陵墓、博物馆和遗址现场连在一起。南昌市政府当前
资料记载，遗址自 2011 年开始发掘，出土文物一万余件套。开放、预约和交通可能调整，
请在出发前以官方最新公告为准。

主要来源：

- [南昌市政府：海昏侯国遗址与博物馆概览](https://www.nc.gov.cn/ncszf/rwfg/202208/8621e793af2543cbbb817e2ca2b7c1ca.shtml)
- [文化和旅游部：南昌汉代海昏侯国遗址博物馆](https://zhuanti.mct.gov.cn/hhhbwg2022.html)
- [海昏侯国遗址管理局：代表性青铜器与汉代生活](https://www.hhh.gov.cn/article/6636.html)
- [中国日报政务信息：地址、开放时间与预约](https://govt.chinadaily.com.cn/s/202501/15/WS67875d37498eec7e1f72d4a1/nanchang-museum-for-haihun-fief-of-han-dynasty.html)
- [中国出版传媒商报：馆长彭明瀚](https://www.cbbr.com.cn/contents/533/102937.html)

## 构建

可直接烧录的合并镜像位于
`build/FoloToy-AI-Passport-haihunhou-full.bin`。

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh
```

## 素材与许可

- `assets/fonts/haihunhou_zh_16.c` 为 Noto Sans CJK SC 中文子集，采用 SIL
  Open Font License 1.1。
- 展品图均为固件内原创像素画，不含博物馆照片、官方标识或第三方作品。
- `assets/music/haihunhou_tts/` 保存 22 段由项目自编文案生成的中文讲解 WAV，
  构建时打包为 IMA-ADPCM，在设备上分块离线播放。
- 基础固件来源：[folotoy/ai-passport](https://github.com/folotoy/ai-passport)。
