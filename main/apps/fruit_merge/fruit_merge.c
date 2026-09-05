#include "fruit_merge.h"
#include "fruit_merge_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(fruit_merge_zh_12);
LV_FONT_DECLARE(fruit_merge_zh_16);
LV_FONT_DECLARE(fruit_merge_zh_24);
static const char *FRUITS[] = {"", "樱桃", "葡萄", "李子", "橘子", "苹果", "桃子", "蜜瓜", "西瓜"};
static const uint32_t COLORS[] = {0xE4E9DD, 0xF19B9F, 0xBBA2DD, 0xBCB7F0, 0xFFBE64, 0xED8077, 0xFFD1B0, 0xD7E88D, 0x81C898};
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_control;
static uint8_t s_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static fm_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery;
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_activity, s_guard, s_next_step;
static bool s_buttons, s_dimmed, s_off, s_initialized;
static unsigned s_book_page;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0); lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 5, 0); lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *text_at(lv_obj_t *p, const char *text, int x, int y, int width,
                        const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, text, font, color);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, width);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static lv_obj_t *line(const char *text, int y)
{
    return text_at(s_content, text, 0, y, 198, &fruit_merge_zh_16, UI_INK);
}
static void heading(const char *text, int y)
{
    text_at(s_content, text, 0, y, 198, &fruit_merge_zh_24, 0x305D45);
}
static void action(const char *text, int y)
{
    box(s_content, 3, y, 192, 28, 0x315B46);
    text_at(s_content, text, 3, y + 4, 192, &fruit_merge_zh_16, 0xFFFFFF);
}
/* Native vector fruit badges: every rank has a name, color, and seed dots.
   No emoji, symbol font, image buffer, or color-only identification. */
static void fruit(lv_obj_t *parent, unsigned rank, int x, int y, int w, int h, bool active)
{
    lv_obj_t *o = box(parent, x, y, w, h, COLORS[rank]);
    lv_obj_set_style_border_width(o, active ? 2 : 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(active ? 0x194D38 : 0x6D7A65), 0);
    /* Place all details relative to the inner border. */
    int border = active ? 2 : 1;
    int inner = w - 2 * border;
    lv_obj_t *leaf = box(o, inner / 2 - 3, 2, 8, 3, 0x315B46);
    lv_obj_set_style_radius(leaf, 0, 0);
    box(o, inner / 2 - 9, 8, 3, 3, UI_INK);
    box(o, inner / 2 + 6, 8, 3, 3, UI_INK);
    int text_y = h > 40 ? (h - 2 * border - 20 < 22 ? h - 2 * border - 20 : 22) : 12;
    text_at(o, FRUITS[rank], 0, text_y, inner,
        h > 40 ? &fruit_merge_zh_16 : &fruit_merge_zh_12, UI_INK);
}
static void board(void)
{
    lv_obj_t *o = text_at(s_content, "", 0, 0, 114, &fruit_merge_zh_16, UI_INK);
    lv_label_set_text_fmt(o, "得分 %lu", (unsigned long)s_state.board.score);
    o = text_at(s_content, "", 118, 0, 80, &fruit_merge_zh_16, 0x315B46);
    if (s_state.chain) lv_label_set_text_fmt(o, "%u连合", s_state.chain);
    else lv_label_set_text(o, "慢慢想");
    o = text_at(s_content, "", 0, 23, 198, &fruit_merge_zh_16, UI_INK);
    lv_label_set_text_fmt(o, "本颗%s 下颗%s", FRUITS[s_state.board.current], FRUITS[s_state.board.next]);
    int landing = fm_landing(&s_state, s_state.board.selected);
    for (unsigned c = 0; c < FM_COLS; c++) {
        lv_obj_t *lane = box(s_content, 3 + c * 48, 45, 46, 167,
            c == s_state.board.selected ? 0xFFE7A0 : 0xE0E5D8);
        lv_obj_set_style_border_width(lane, c == s_state.board.selected ? 2 : 0, 0);
        lv_obj_set_style_border_color(lane, lv_color_hex(0xBD8030), 0);
        for (unsigned r = 0; r < FM_ROWS; r++) {
            uint8_t value = s_state.board.cells[r][c];
            int x = 6 + c * 48, y = 48 + (FM_ROWS - 1 - r) * 33;
            if (value) fruit(s_content, value, x, y, 40, 31,
                s_state.busy && r == s_state.focus_row && c == s_state.focus_col);
            else if (!s_state.busy && c == s_state.board.selected && (int)r == landing) {
                lv_obj_t *target = box(s_content, x, y, 40, 29, 0xFFF6D7);
                lv_obj_set_style_border_width(target, 1, 0);
                lv_obj_set_style_border_color(target, lv_color_hex(0xBD8030), 0);
                text_at(target, "落点", 0, 5, 38, &fruit_merge_zh_12, 0x805820);
            }
        }
    }
}
static void render(void)
{
    lv_obj_clean(s_content);
    const char *footer = "";
    lv_obj_t *o;
    switch (s_state.page) {
    case FM_HOME:
        heading("合出大西瓜", 0);
        fruit(s_content, 1, 8, 42, 52, 56, false);
        fruit(s_content, 4, 73, 42, 52, 56, false);
        fruit(s_content, 8, 138, 42, 52, 56, false);
        line("相同水果，碰到就合", 112);
        o = line("", 137); lv_label_set_text_fmt(o, "本次纪录 %lu", (unsigned long)s_state.best);
        action(s_state.has_game ? "确定继续这局" : "确定开始", 169);
        footer = "上键玩法  下键果册";
        break;
    case FM_RULES:
        heading("三键就会玩", 0);
        line("上键往左，下键往右", 41);
        line("短按确定，放下一颗", 69);
        line("同果相邻，自动合成", 97);
        line("满列换列，全满结算", 125);
        line("长按确定，休息撤回", 153);
        action("确定知道啦", 182);
        break;
    case FM_PLAY:
        board();
        if (s_state.busy) footer = "合成中，稍等一下";
        else if (s_state.full_notice || fm_landing(&s_state, s_state.board.selected) < 0) footer = "这列满啦，上下换列";
        else footer = "上左 下右 确定放";
        break;
    case FM_PAUSE:
        heading("歇一小会", 4);
        fruit(s_content, s_state.board.peak ? s_state.board.peak : 1, 67, 42, 64, 60, false);
        line(s_state.busy ? "合成暂停，继续再看" :
            s_state.undo_used ? "本局撤回已用完" : s_state.has_previous ? "上键撤回，每局一次" : "先放一颗，再来撤回", 120);
        action("确定继续", 151);
        line("下键回家，保留这局", 190);
        break;
    case FM_RESULT:
        heading("果篮满啦", 0);
        o = text_at(s_content, "", 0, 37, 198, &fruit_merge_zh_24, 0x315B46);
        lv_label_set_text_fmt(o, "%lu 分", (unsigned long)s_state.board.score);
        o = line("", 75); lv_label_set_text_fmt(o, "最大水果 %s", FRUITS[s_state.board.peak]);
        o = line("", 103); lv_label_set_text_fmt(o, "最高连合 %u 次", s_state.max_chain);
        line(!s_state.undo_used && s_state.has_previous ? "长按确定，可撤回救场" : "撤回已用，换个思路", 132);
        action("确定同题再来", 164);
        footer = "上键新一局  下键回家";
        break;
    case FM_BOOK:
        heading(s_book_page ? "越合越大" : "水果成长册", 0);
        for (unsigned i = 0; i < 4; i++) {
            unsigned rank = s_book_page * 4 + i + 1;
            int x = 10 + (i % 2) * 98, y = 40 + (i / 2) * 69;
            fruit(s_content, rank, x, y, 80, 43, false);
            text_at(s_content, s_state.collection & (1u << (rank - 1)) ? "已遇见" : "待遇见",
                x, y + 46, 80, &fruit_merge_zh_12, 0x315B46);
        }
        line("双西瓜相碰，腾出空间", 186);
        footer = "上下翻页  确定返回";
        break;
    }
    lv_label_set_text(s_footer, s_buttons ? footer : "按键异常，请重启");
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static void dispatch(input_t input)
{
    bsp_btn_t b = input.button;
    if (input.event == BSP_BTN_LONG) {
        if (s_state.page == FM_PLAY) fm_pause(&s_state);
        else if (s_state.page == FM_RESULT) (void)fm_undo(&s_state);
        else if (s_state.page == FM_PAUSE) fm_resume(&s_state);
        else s_state.page = FM_HOME;
        return;
    }
    switch (s_state.page) {
    case FM_HOME:
        if (b == BSP_BTN_OK) {
            if (s_state.has_game) fm_resume(&s_state);
            else { s_state.return_page = FM_PLAY; s_state.page = FM_RULES; }
        } else if (b == BSP_BTN_UP) { s_state.return_page = FM_HOME; s_state.page = FM_RULES; }
        else { s_book_page = 0; s_state.page = FM_BOOK; }
        break;
    case FM_RULES:
        if (b == BSP_BTN_OK) {
            if (s_state.return_page == FM_PLAY) fm_start(&s_state, esp_random());
            else s_state.page = FM_HOME;
        }
        break;
    case FM_PLAY:
        if (b == BSP_BTN_OK) {
            if (fm_drop(&s_state)) s_next_step = now_ms() + 220;
        } else fm_select(&s_state, b == BSP_BTN_UP ? -1 : 1);
        break;
    case FM_PAUSE:
        if (b == BSP_BTN_OK) fm_resume(&s_state);
        else if (b == BSP_BTN_UP) (void)fm_undo(&s_state);
        else s_state.page = FM_HOME;
        break;
    case FM_RESULT:
        if (b == BSP_BTN_DOWN) { s_state.has_game = false; s_state.page = FM_HOME; }
        else fm_start(&s_state, b == BSP_BTN_OK ? s_state.seed : esp_random());
        break;
    case FM_BOOK:
        if (b == BSP_BTN_OK) s_state.page = FM_HOME;
        else s_book_page ^= 1;
        break;
    }
}
static void tick(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms(); input_t input;
    bool changed = false;
    while (xQueueReceive(s_queue, &input, 0) == pdTRUE) {
        if (!s_buttons || now - input.at > 350 || input.at < s_guard) continue;
        s_activity = now;
        if (s_dimmed || s_off) {
            bsp_display_backlight(100); s_dimmed = s_off = false;
            s_guard = now + 800; continue;
        }
        fm_page_t before = s_state.page;
        dispatch(input); changed = true;
        if (before != s_state.page) { s_guard = now + 220; s_next_step = now + 220; }
        else if (input.button == BSP_BTN_OK) s_guard = now + 160;
    }
    if (s_state.page == FM_PLAY && s_state.busy && now >= s_next_step) {
        (void)fm_step(&s_state); changed = true; s_next_step = now + 180;
        if (!s_state.busy) s_guard = now + 120;
    }
    if (now - s_activity >= 60000 && !s_dimmed && !s_off) {
        fm_pause(&s_state); changed = true; bsp_display_backlight(20); s_dimmed = true;
    }
    if (now - s_activity >= 180000 && !s_off) { bsp_display_backlight(0); s_off = true; }
    if (changed) render();
}
void fruit_merge_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_storage, &s_control);
    if (!s_initialized) { fm_init(&s_state); s_initialized = true; }
}
void fruit_merge_enter(bool buttons_available)
{
    fruit_merge_prepare(); xQueueReset(s_queue);
    s_buttons = buttons_available; s_dimmed = s_off = false;
    bsp_display_backlight(100);
    s_activity = now_ms(); s_guard = s_activity + 220;
    if (s_state.page == FM_RESULT) s_state.has_game = false;
    s_state.page = FM_HOME;
    s_screen = ui_pixel_screen_create("");
    text_at(s_screen, "再合一颗", 12, 14, 136, &fruit_merge_zh_16, 0xFFFFFF);
    s_battery = text_at(s_screen, "--%", 164, 30, 70, &fruit_merge_zh_12, UI_INK);
    s_content = ui_pixel_panel_create(s_screen, 10, 53, 220, 234, UI_PAPER);
    s_footer = text_at(s_screen, "", 5, 294, 230, &fruit_merge_zh_16, UI_INK);
    render(); battery(NULL);
    s_timer = lv_timer_create(tick, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen); atomic_store(&s_accept, true);
}
void fruit_merge_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = s_content = s_footer = s_battery = NULL;
}
void fruit_merge_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept) || !s_queue || button < BSP_BTN_UP || button > BSP_BTN_OK) return;
    /* Release-class OK events avoid dropping a fruit before a long-press pause.
       The driver's double-click event is deliberately one action, not two. */
    if (button == BSP_BTN_OK) {
        if (event != BSP_BTN_CLICK && event != BSP_BTN_DOUBLE && event != BSP_BTN_LONG) return;
    } else if (event != BSP_BTN_PRESS) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}
