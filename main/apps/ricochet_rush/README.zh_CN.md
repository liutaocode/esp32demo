<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 再弹一轮实现说明

产品介绍、按键与本地审核材料。

`ricochet_rush_state.c` 负责可复现地图、瞄准、固定步长碰撞、球数成长、计分和回合转换；`ricochet_rush.c` 负责中文 LVGL 界面、静态输入队列、局部刷新、闲置处理与生命周期。非 LVGL 任务调用进入/退出函数必须持有 BSP 的 LVGL 锁。仅当 `FAP_APP=ricochet_rush` 时主程序选用新应用，其他应用选择保持有效。

## 复现

先激活 ESP-IDF 5.5.3：

```sh
FAP_APP=ricochet_rush ./tools/validate.sh
```

命令编译并验证合并镜像，不会安装固件。其他应用构建会替换通用输出，因此应把已验证固件保留到本应用物料目录。

主机逻辑与正式界面测试：

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/ricochet_rush \
  tests/test_ricochet_rush_state.c main/apps/ricochet_rush/ricochet_rush_state.c \
  -lm -o /tmp/test_ricochet_rush_state
/tmp/test_ricochet_rush_state
cmake -S tests/ricochet_rush_ui -B /tmp/ricochet-rush-ui
cmake --build /tmp/ricochet-rush-ui -j 8
mkdir -p /tmp/ricochet-rush-previews
(cd /tmp/ricochet-rush-previews && /tmp/ricochet-rush-ui/preview)
python3 tools/render_ricochet_rush_preview.py --input /tmp/ricochet-rush-previews
python3 tools/generate_ricochet_rush_font.py --check
```

预览转换需要 Pillow。重新生成字体需要 Node/npm 与固定的 `lv_font_conv@1.5.3`，源字体为仓库的思源黑体。静态检查覆盖全部中文字符；UI 测试检查实际字体、文本宽度、极端瞄准位置、最大计数、所有页面、队列行为、暂停唤醒、外设缺失和五十次退出重入。

玩法测试覆盖砖面和砖角碰撞、分离后不重复扣血、拾取、侧墙、首次回球位置、超时归队、球数上限、八十张地图的确定性重玩、固定步长、失败和通关。实体键延迟、帧率、持续内存占用和玩家难度仍待实机验证。
