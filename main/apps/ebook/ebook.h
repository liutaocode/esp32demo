// main/apps/ebook/ebook.h —— 电子书阅读器的入口。
#pragma once

#include <stdbool.h>

#include "bsp_button.h"

// 在 LVGL 建屏之前调用:挂载 books 分区、准备 netif 与事件循环、读出字号偏好。
void ebook_prepare(void);

// 建屏并启动工作任务。必须在持有 bsp_lvgl_lock() 时调用。
void ebook_enter(bool buttons_ok);

// 按键回调,运行在 button 任务里,只投递事件。
void ebook_key(bsp_btn_t btn, bsp_btn_ev_t ev);
