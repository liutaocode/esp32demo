#pragma once
#include "lvgl.h"
#include "tally_state.h"
#include "tally_feedback.h"
typedef enum { TC_STORED, TC_PENDING, TC_SAVING, TC_SAVE_ERROR, TC_INPUT_ERROR, TC_RECOVER_ERROR, TC_BUTTON_ERROR, TC_ARCHIVED } tc_notice;
void tc_ui_create(void);
void tc_ui_render(const tc_state *s, tc_notice notice, int battery, int64_t now);
void tc_ui_destroy(void);
lv_obj_t *tc_ui_screen(void);

void tc_ui_feedback(tc_feedback event);
