#pragma once
#include "bsp_button.h"
#include "word_sprite_state.h"

/* Prepare before registering button callbacks. Enter/exit under LVGL lock.
 * exit is asynchronous: timer waits for the I/O worker's shutdown handshake. */
void word_sprite_prepare(void);
void word_sprite_enter(bool buttons_available, ws_progress_t progress);
void word_sprite_key(bsp_btn_t button, bsp_btn_ev_t event);
void word_sprite_exit(void);
