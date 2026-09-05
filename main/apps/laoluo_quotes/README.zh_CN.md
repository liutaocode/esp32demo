<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 老罗语录



“老罗语录”是一款为 FoloToy AI Passport 制作的离线口袋金句电台。应用每次启动会
随机打开一条语录，保留设备原有的像素视觉，并用中性普通话合成语音朗读每条内容。

## 操作方式

- 上键：上一条语录。
- 下键：下一条语录。
- 确定键：播放或重播当前语录的 TTS。

界面始终显示电量；闲置一分钟后背光变暗，三分钟后关闭。熄屏后的第一次按键只负责
唤醒，不会误切换语录。

## 内容与语音

首批收录 12 条短语录，覆盖行动、理想、思考、产品、转型和人生选择。可编辑的
`quotes.csv` 是唯一内容源，每一条都记录来源标题和链接。首批内容参考并核对了
[维基语录](https://zh.wikiquote.org/wiki/%E7%BD%97%E6%B0%B8%E6%B5%A9)、
[2006 年人物专访](https://news.sohu.com/20060404/n242621013.shtml)和
[2016 年访谈实录](https://www.ithome.com/0/214/423.htm)。

仓库内的 WAV 使用 macOS Tingting 中性系统音色生成，不是罗永浩本人录音，也不模拟
其本人声线。构建时音频被打包为 IMA ADPCM，固件在工作任务中每批解码 512 个采样，
不会在按键回调里执行阻塞音频操作。

## 新增语录

1. 在 `quotes.csv` 增加一行，填写唯一编号、分类、语录、来源标题和来源链接。
2. 重新生成目录、WAV、ADPCM 音频包与中文子集字库：

```bash
python3 tools/generate_laoluo_quotes_assets.py \
  --synthesize --voice Tingting \
  --font /path/to/NotoSansCJKsc-Regular.otf
```

中文字体使用 SIL Open Font License 1.1 许可的 Noto Sans CJK SC，许可全文位于
`assets/fonts/OFL-NotoSansCJK.txt`。

## 构建

使用 ESP-IDF 5.5.3：

```bash
FAP_APP=laoluo_quotes ./tools/validate.sh --firmware
```

本项目为非官方粉丝作品，与罗永浩本人及其公司无隶属或背书关系。

## 文件夹说明

- `quotes.csv`：带来源、可继续编辑的语录集。
- `assets/images/laoluo_quotes/`：本应用共用的发布图片与 README 配图。
- `laoluo_quotes.c`：用户界面与操作逻辑。
- `laoluo_quotes_state.*`：可独立测试的浏览状态。
- `laoluo_quotes_catalog.*`：根据语录集生成的内容数据。
- `laoluo_quotes_audio.*` 与 `laoluo_adpcm.*`：生成的朗读数据与播放解码。
