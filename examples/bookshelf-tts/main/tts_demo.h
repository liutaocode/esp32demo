#pragma once
#include <stdbool.h>
#include "bsp_button.h"
/* enter/exit require the LVGL lock; key only queues events and never waits. */
void tts_demo_enter(bool audio_ok, bool buttons_ok);
void tts_demo_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void tts_demo_exit(void);
