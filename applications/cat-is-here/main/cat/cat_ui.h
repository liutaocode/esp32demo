#pragma once
#include "cat_state.h"
#include "lvgl.h"
typedef struct {
    lv_obj_t *screen, *previous_screen, *scene, *title, *subtitle, *battery, *message, *hint;
    lv_obj_t *rows[6], *menu, *menu_shadow, *message_panel, *keys[3];
    const cat_state_t *cat;
    uint32_t motion;
    unsigned page, selection;
} cat_ui_t;
void cat_ui_create(cat_ui_t *ui, const cat_state_t *cat);
void cat_ui_render(cat_ui_t *ui, uint32_t motion, unsigned page, unsigned selection,
                   int battery, bool audio, bool storage, bool buttons);
void cat_ui_destroy(cat_ui_t *ui);
