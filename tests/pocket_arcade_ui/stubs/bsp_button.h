#pragma once
typedef enum { BSP_BTN_UP, BSP_BTN_DOWN, BSP_BTN_OK } bsp_btn_t;
typedef enum { BSP_BTN_PRESS, BSP_BTN_CLICK, BSP_BTN_DOUBLE, BSP_BTN_LONG } bsp_btn_ev_t;
/* esp_err_t 就是 int;主机预览里由各自的 preview.c 提供实现。 */
int bsp_button_set_long_press_ms(bsp_btn_t btn, unsigned ms);
