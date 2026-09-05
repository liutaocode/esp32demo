<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 再合一颗实现说明

独立应用通过 `FAP_APP=fruit_merge` 选择，已有应用及默认选择仍可使用。

## 设计

输入：上下键响应按下；确定键响应单击或双击，两者均作为一次操作；长按确定暂停，不会先落果。按键回调只投递带时间戳的有界事件。所有界面访问归 LVGL 定时器管理；过期输入和页面切换保护防止排队落果及唤醒误操作。

状态：`fruit_merge_state.c` 不依赖 LVGL 或 ESP-IDF。行从地板向上存储，每次落果保存完整棋盘和随机数状态供一次撤回。先按下、左、右、上的顺序找当前水果的邻居，再按从底向上、从左向右的顺序查找重力造成的其他接触。界面每隔 180 毫秒处理一对并压实棋盘；斜对角不合成，最高级配对后清除。每次合成都减少占用格子，因此一手至多处理 20 对。得分为 `2^(原等级+1) * 本手连合序号`，上限 999999。

重玩：开局前两颗固定樱桃，之后为 50% 樱桃、30% 葡萄、20% 李子；序列仅由种子和步数决定。撤回恢复完全相同的序列。果册和纪录表示本次开机实际达到过的结果，因此撤回不会抹去它们。

输出：12/16/24 像素思源黑体中文子集、代码绘制的水果牌、本颗与下颗预告、高亮列及落点。每级显示名称，避免仅靠颜色辨别。保留像素天空、草地与标题牌；电量放在右上云朵下方，读取不可用时显示 `--%`。

资源与生命周期：不加入音频、网络、存储写入、位图帧缓存或工作任务。使用一个四元素静态输入队列与两个 LVGL 定时器。退出先禁用输入，删除两只定时器，再删除屏幕。字体保存在闪存，纪录和果册仅在内存中。调暗和关闭背光不会休眠处理器。

## 验证

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain/apps/fruit_merge \
  tests/test_fruit_merge_state.c main/apps/fruit_merge/fruit_merge_state.c \
  -o /tmp/test_fruit_merge_state
/tmp/test_fruit_merge_state
python3 tools/generate_fruit_merge_font.py --check
cmake -S tests/fruit_merge_ui -B /tmp/fruit-merge-ui-build
cmake --build /tmp/fruit-merge-ui-build -j8
mkdir -p /tmp/fruit-merge-previews
(cd /tmp/fruit-merge-previews && /tmp/fruit-merge-ui-build/preview)
python3 tools/render_fruit_merge_preview.py
FAP_APP=fruit_merge ./tools/validate.sh
```

完整门禁需先激活 ESP-IDF 5.5.3；预览转换需 Pillow。界面测试运行真实应用界面代码及 LVGL，检查每个字符、排除英文界面文字、标签溢出、同级标签重叠、面板边界、输入队列、唤醒和重复退出进入。压力测试图使用人工构造状态，不作为游戏实测证据。剩余实体检查见审核准备。
