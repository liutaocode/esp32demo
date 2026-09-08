// components/bsp/include/bsp_button.h
// 三个按键共用一个 ADC 引脚,靠分压电阻区分。电压窗口见 bsp_pins.h。
#pragma once

#include "esp_err.h"
#include <stdbool.h>

// 按键索引。数量用 bsp_pins.h 的 BSP_BTN_COUNT(硬件属性,归引脚表管),
// 这里不再定义尾项计数,避免出现 BSP_BTN_COUNT / BSP_BTN_COUNT_ 两个近似名字。
typedef enum {
    BSP_BTN_UP = 0,
    BSP_BTN_DOWN,
    BSP_BTN_OK,
} bsp_btn_t;

typedef enum {
    BSP_BTN_PRESS = 0,   // 按下瞬间(低延迟,适合游戏类即时响应)
    BSP_BTN_CLICK,       // 单击(按下并抬起)
    BSP_BTN_DOUBLE,      // 双击
    BSP_BTN_LONG,        // 长按
    BSP_BTN_RELEASE,     // 抬起瞬间;默认不派发,需 bsp_button_enable_release(true)
} bsp_btn_ev_t;

// 按键事件回调。运行于 button 组件的定时器任务,勿在其中阻塞或做重活。
typedef void (*bsp_btn_cb_t)(bsp_btn_t btn, bsp_btn_ev_t ev, void *user);

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user);

// 开关 BSP_BTN_RELEASE 事件。默认关闭:绝大多数页面只关心按下/单击/长按,
// 多派发一个事件会让它们的 switch 分支意外命中。"按住说话"这类需要知道
// 松手时刻的应用在 bsp_button_init() 之后显式打开。
// 需在 bsp_button_init() 成功之后调用;重复调用同一状态是空操作。
esp_err_t bsp_button_enable_release(bool enable);

// 读当前 ADC 原始电压(mV)。松开时约 3300;按住某键时约为该键的分压值。
// ★ 换了分压/上拉阻值后,用它测出自己的三档电压,再改 bsp_pins.h 的 BSP_BTN_MV_TABLE。
// 读取失败返回 -1。
int bsp_button_read_mv(void);
