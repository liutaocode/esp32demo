<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 一刀刚好实现说明

玩法与截图。

独立应用使用纯 C 状态机、真实 LVGL 页面、四事件静态按键队列和专用 16 像素中文字库。不初始化音频和网络。进入与退出需要持有 LVGL 锁；按键回调只投递按下事件，LVGL 定时器负责状态和绘制。退出先关闭输入、删除定时器，再销毁页面；再次进入会回收临时空白屏。

刀的位置使用定点反射路径。先按最后显示的位置判定下刀，再推进移动；丢弃超过 200 毫秒的旧按键。帧间隔超过 200 毫秒时自动暂停切割。电池 I2C 读取避开切割阶段。蛋糕装饰按切片边界计算交集，即使一像素切片也保持尺寸有效。英数后备字体仅用于数字和标点。

## 构建和检查

在仓库根目录激活 ESP-IDF 5.5.3 后运行：

```bash
FAP_APP=perfect_slice ./tools/validate.sh
python3 tools/generate_perfect_slice_font.py
cmake -S tests/perfect_slice_ui -B build/perfect-slice-ui
cmake --build build/perfect-slice-ui -j 8
(cd build/perfect-slice-ui && ./preview)
```

完整门禁包含纯状态测试。单独 UI 程序使用已解析的 LVGL 依赖和共享主机桩，生成 PPM 截图；检查十种目标、两种模式的全部 179 个切割位置、缺字、文字宽度、队列溢出、过期事件、连按、卡帧、闲置唤醒、电池和按键降级，以及五十次退出重进。

选择本应用不改变分区。主机渲染和固件编译不能代替实机测试。设备上应验证两种模式完整一局、按下到下刀的可见延迟、切割和反馈页的暂停恢复、闲置调暗唤醒、可读性以及长时间运行。
