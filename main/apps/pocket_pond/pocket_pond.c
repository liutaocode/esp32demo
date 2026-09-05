#include "pocket_pond.h"
#include "pocket_pond_state.h"
#include "pocket_pond_storage.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(pocket_pond_zh_16);
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static pp_state_t s_state;
static enum { HOME, ROUND, ALBUM } s_view, s_return_view;
static unsigned s_fish, s_save_status;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_footer, *s_save;
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_activity, s_last_input;
static bool s_buttons, s_dimmed, s_off, s_prepared;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0); lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *text(lv_obj_t *p, const char *value, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, value, &pocket_pond_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, 198);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static void fish_art(lv_obj_t *p, int x, int y, unsigned fish, bool known)
{
    static const uint32_t colors[PP_FISH] = {0xF5AF5B,0xEF7595,0xF4D84B,0x76BE73,0xBEE7ED,0xF49375,0xAAA1EA,0xF7CA5B,0xEF785C};
    uint32_t color = known ? colors[fish] : 0x9AB9C4;
    box(p, x, y + 13, 15, 30, UI_INK, 3);
    box(p, x + 3, y + 17, 13, 22, color, 2);
    box(p, x + 15, y + 4, 59, 48, UI_INK, 17);
    box(p, x + 19, y + 8, 51, 40, color, 14);
    box(p, x + 33, y, 19, 10, UI_INK, 2);
    box(p, x + 37, y + 2, 12, 8, color, 2);
    box(p, x + 54, y + 18, 10, 12, 0xFFFFFF, 3);
    box(p, x + 59, y + 21, 5, 6, UI_INK, 1);
    box(p, x + 62, y + 36, 7, 3, UI_INK, 0);
    box(p, x + 36, y + 29, 7, 12, UI_INK, 2);
    if (known) {
        if (fish == 3 || fish == 8) for (int n = 0; n < 3; n++) box(p, x + 23 + n * 10, y + 14, 4, 7, fish == 8 ? 0xFFF3D1 : 0x327C68, 1);
        if (fish == 6 || fish == 7) { box(p, x + 24, y + 18, 10, 4, 0xFFFFFF, 0); box(p, x + 27, y + 15, 4, 10, 0xFFFFFF, 0); }
        if (fish == 4) box(p, x + 23, y + 15, 22, 6, 0xFFFFFF, 3);
    }
}
static void pond(unsigned fish, bool known, bool wave)
{
    lv_obj_t *p = box(s_content, 0, 27, 198, 89, 0xD9F0EC, 10);
    box(p, 0, 63, 198, 26, 0xB6DBD3, 0);
    for (int i = 0; i < 4; i++) box(p, 12 + i * 48, 70 + (i % 2) * 8, 25, 3, 0x83B9B0, 1);
    if (wave) {
        for (int i = 0; i < 3; i++) { box(p, 28 + i * 49, 26, 39, 24, 0x65AABE, 12); box(p, 31 + i * 49, 23, 30, 12, 0xFFFFFF, 6); }
    } else fish_art(p, 65, 15, fish, known);
    box(p, 18, 17, 6, 6, 0xFFFFFF, LV_RADIUS_CIRCLE);
    box(p, 172, 37, 8, 8, 0xFFFFFF, LV_RADIUS_CIRCLE);
}
static void save_status(void)
{
    s_save_status = pp_storage_status();
    if (s_save) lv_label_set_text(s_save, s_save_status == 0 ? "鱼册已保存" : s_save_status == 1 ? "鱼册保存中" : "存储不可用，仅本次");
}
static void render(void)
{
    s_save = NULL; lv_obj_clean(s_content);
    if (s_view == HOME) {
        text(s_content, "再捞一条，还是收手？", 0, UI_INK);
        pond(8, true, false);
        text(s_content, "九条鱼，三次浪", 122, UI_INK);
        text(s_content, "遇到第二次浪，本趟清空", 144, 0xA8462D);
        text(s_content, "仅停车休息时游玩", 164, 0x356F66);
        text(s_content, "没有倒计时，随时放下", 184, UI_INK);
        lv_label_set_text(s_footer, s_state.page == PP_PLAY ? "确定继续 / 上键鱼册" : "确定开捞 / 上键鱼册");
    } else if (s_view == ALBUM) {
        lv_obj_t *title = text(s_content, "", 0, UI_INK);
        lv_label_set_text_fmt(title, "鱼册 %u/9   已集 %u 种", s_fish + 1, pp_species(&s_state.progress));
        pond(s_fish, s_state.progress.caught[s_fish] != 0, false);
        lv_obj_t *name = text(s_content, "", 122, UI_INK);
        lv_label_set_text_fmt(name, "%s  %u 分", pp_names[s_fish], pp_values[s_fish]);
        text(s_content, pp_notes[s_fish], 144, 0x356F66);
        unsigned count = s_state.progress.caught[s_fish];
        lv_obj_t *owned = text(s_content, "", 164, UI_INK);
        lv_label_set_text_fmt(owned, "%s / 收藏 %u 条", count >= 20 ? "金章" : count >= 5 ? "银章" : count ? "铜章" : "未收录", count);
        s_save = text(s_content, "", 184, 0x356F66); save_status();
        lv_label_set_text(s_footer, "上下翻页 / 确定返回");
    } else if (s_state.page == PP_RESULT) {
        text(s_content, s_state.lost ? "鱼回池塘，好运下趟" : s_state.record ? "新纪录！收获满满" : "稳稳收好，这趟值了", 0, s_state.lost ? 0xA8462D : 0x356F66);
        pond(s_state.last < PP_FISH ? s_state.last : 8, true, s_state.lost);
        lv_obj_t *score = text(s_content, "", 122, UI_INK);
        lv_label_set_text_fmt(score, "本趟收下 %u 分", s_state.lost ? 0 : s_state.score);
        lv_obj_t *stats = text(s_content, "", 144, UI_INK);
        lv_label_set_text_fmt(stats, "最高 %u 分 / 鱼册 %u 种", s_state.progress.best, pp_species(&s_state.progress));
        text(s_content, s_state.lost ? "以前的收藏一条也不少" : "一条铜章，五条银章", 164, 0x356F66);
        s_save = text(s_content, "", 184, 0x356F66); save_status();
        lv_label_set_text(s_footer, "确定再来 / 上键鱼册");
    } else {
        lv_obj_t *stats = text(s_content, "", 0, UI_INK);
        lv_label_set_text_fmt(stats, "鱼篓 %u 分   浪 %u/2", s_state.score, s_state.waves);
        bool wave = s_state.last == PP_FISH;
        pond(s_state.last < PP_FISH ? s_state.last : 0, true, wave);
        lv_obj_t *caught = text(s_content, "", 122, wave ? 0xA8462D : UI_INK);
        if (wave) lv_label_set_text(caught, "起浪了！再遇浪就清空");
        else if (s_state.last < PP_FISH) lv_label_set_text_fmt(caught, "%s +%u 分%s", pp_names[s_state.last], pp_values[s_state.last], s_state.progress.caught[s_state.last] ? "" : " 新鱼");
        else lv_label_set_text(caught, "确定捞鱼，下键收好");
        lv_obj_t *left = text(s_content, "", 144, UI_INK);
        lv_label_set_text_fmt(left, "池中剩 %u 鱼 / %u 浪", pp_remaining_fish(&s_state), 3 - s_state.waves);
        lv_obj_t *risk = text(s_content, "", 164, s_state.waves ? 0xA8462D : 0x356F66);
        lv_label_set_text_fmt(risk, "%s %u/%u", s_state.waves ? "下次冲走概率" : "下次起浪概率", 3 - s_state.waves, PP_CARDS - s_state.cursor);
        text(s_content, "长按确定歇会儿", 184, 0x356F66);
        lv_label_set_text(s_footer, "确定再捞 / 下键收好");
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启");
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static bool handle(input_t input, int64_t now)
{
    s_last_activity = now;
    if (s_off || s_dimmed) { s_off = s_dimmed = false; bsp_display_backlight(100); return false; }
    if (input.event == BSP_BTN_LONG) { s_view = HOME; return true; }
    if (s_view == ALBUM) {
        if (input.button == BSP_BTN_OK) s_view = s_return_view;
        else s_fish = (s_fish + (input.button == BSP_BTN_UP ? 1 : 8)) % PP_FISH;
        return true;
    }
    if (input.button == BSP_BTN_UP) { s_return_view = s_view; s_view = ALBUM; return true; }
    if (s_view == HOME || s_state.page == PP_RESULT) {
        if (input.button != BSP_BTN_OK) return false;
        if (s_state.page != PP_PLAY) pp_start(&s_state, esp_random());
        s_view = ROUND; return true;
    }
    bool changed = input.button == BSP_BTN_OK ? pp_draw(&s_state) : pp_bank(&s_state);
    if (changed && s_state.page == PP_RESULT) pp_storage_save(s_state.progress);
    return changed;
}
static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms(); input_t input;
    bool handled = false, dirty = false;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (!handled && now - input.at <= 300 && now - s_last_input >= 250) {
            dirty = handle(input, now); handled = true; s_last_input = now;
        }
    }
    if (!s_dimmed && now - s_last_activity >= 30000) { s_dimmed = true; bsp_display_backlight(20); }
    if (!s_off && now - s_last_activity >= 90000) { s_off = true; bsp_display_backlight(0); }
    if (dirty) render();
    else if (s_save && s_save_status != pp_storage_status()) save_status();
}
void pocket_pond_prepare(void)
{
    if (s_prepared) return;
    s_prepared = true;
    s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
    s_state.progress = pp_storage_init();
}
void pocket_pond_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    if (event != BSP_BTN_CLICK && event != BSP_BTN_DOUBLE && !(button == BSP_BTN_OK && event == BSP_BTN_LONG)) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}
void pocket_pond_enter(bool buttons_available)
{
    if (s_screen || !s_prepared) return;
    s_buttons = buttons_available; s_view = HOME;
    s_dimmed = s_off = false; s_last_activity = now_ms(); s_last_input = s_last_activity - 250;
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = text(s_screen, "口袋捞鱼", 15, 0xFFFFFF);
    lv_obj_set_x(title, 5); lv_obj_set_width(title, 151);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 163, 31); lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, UI_PAPER);
    s_footer = text(s_screen, "", 294, UI_INK);
    lv_obj_set_x(s_footer, 4); lv_obj_set_width(s_footer, 232);
    render(); battery(NULL);
    s_timer = lv_timer_create(frame, 30, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen); bsp_display_backlight(100);
    xQueueReset(s_queue); atomic_store(&s_accept, buttons_available);
}
void pocket_pond_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_content = s_battery = s_footer = s_save = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
