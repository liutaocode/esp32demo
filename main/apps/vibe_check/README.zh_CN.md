<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Vibe Check



Vibe Check 是一款为 FoloToy AI Passport 制作的快速离线人格卡游戏。完成五道
二选一题后，玩家会获得八种原创像素人格之一、三项直观特质，以及一个适合并排
比较或分享的短卡片码。

设备上的欢迎页、题目、选项、揭晓动画和结果卡均使用简体中文，并采用仅包含本
应用所需字形的紧凑中文子集字体。内置离线普通话语音会播报欢迎页、每道题与
上下键对应选项，并在最后念出人格名称和一句解释。

## 玩法

1. 在欢迎页按确定键。
2. 听完题目与两个选项后，直接按上键或下键作答，共五题。
3. 看完短暂的揭晓动画，与朋友比较结果。
4. 在结果卡按确定键重新开始。

欢迎页、题目页或结果页都可以长按确定键重听当前语音。

界面始终保留电量显示。无操作一分钟后背光变暗，三分钟后关闭；关闭后的第一次
按键只唤醒屏幕，不改变游戏状态。应用完全离线，不保存答案或个人数据。
音频初始化失败时仍可阅读全部文字并正常游玩。

## 实现

- `vibe_check_state.c` 包含与硬件无关的问答状态机。
- `vibe_check.c` 负责 LVGL 页面、像素人格、揭晓动画、电量显示与空闲背光管理。
- `vibe_check_voice.c` 在可取消的后台任务中播放语音；按键与 LVGL 回调只替换最新
  播报请求。
- `vibe_check_audio.c` 校验 14 段离线普通话 ADPCM 语音包；播放时每次只解码
  512 个采样，不会把整段语音载入内存。
- `assets/fonts/vibe_check_zh_16.c` 只包含界面所需的简体中文字形，数字回退到
  Montserrat 字体。
- 按键回调不执行存储、音频解码或网络操作。

重新生成并检查语音资源：

```bash
python3 tools/generate_vibe_check_audio.py --synthesize
python3 tools/generate_vibe_check_audio.py --check
```
