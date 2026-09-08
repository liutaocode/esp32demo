#include "pocket_hype.h"
#include "pocket_hype_state.h"
#include "pocket_hype_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>
LV_FONT_DECLARE(pocket_hype_zh_12);
LV_FONT_DECLARE(pocket_hype_zh_16);
LV_FONT_DECLARE(pocket_hype_zh_24);
#define INK 0x263B46
#define JADE 0x087F78
#define CREAM 0xFFF7DF
#define CORAL 0xD74B57
#define NIGHT 0x333455
static const uint32_t colors[] = {0xFFCD56, 0xDBB8FA, 0x9EDFD6, 0xAED8EC, 0xFFD476, 0xFFC6C8, 0xF5B987};
static const char *const volumes[] = {"静音", "轻声", "标准", "响亮"};
static const char *const heat[] = {"小小回应", "气氛到位", "全场沸腾"};
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_control;
static uint8_t s_storage[8 * sizeof(input_t)];
static QueueHandle_t s_inputs;
static atomic_bool s_accept;
static ph_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery;
static lv_obj_t *s_arms[2], *s_people[5], *s_bits[10];
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_tick, s_last_activity;
static bool s_buttons, s_off, s_dimmed, s_audio_ok;
static bsp_btn_t s_wake_button;
static bool s_wake_guard;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t color, int r) {
    lv_obj_t *o = lv_obj_create(p); lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0); lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, r, 0); lv_obj_set_style_bg_color(o, lv_color_hex(color), 0); return o;
}
static lv_obj_t *label(lv_obj_t *p, const char *v, int x, int y, int w, const lv_font_t *f, uint32_t c) {
    lv_obj_t *o = ui_pixel_label(p, v, f, c); lv_obj_set_pos(o, x, y); lv_obj_set_width(o, w);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0); return o;
}
static lv_obj_t *line(const char *v, int y, uint32_t c) { return label(s_content, v, 0, y, 198, &pocket_hype_zh_16, c); }
static void stage(unsigned scene) {
    lv_obj_t *p = box(s_content, 0, 25, 198, 112, NIGHT, 10);
    box(p, 6, 6, 186, 100, colors[scene], 6);
    box(p, 6, 6, 15, 76, 0xAC4159, 4); box(p, 177, 6, 15, 76, 0xAC4159, 4);
    box(p, 11, 8, 3, 66, 0xD96277, 1); box(p, 183, 8, 3, 66, 0xD96277, 1);
    label(p, ph_names[scene], 22, 8, 154, &pocket_hype_zh_24, INK);
    /* Original pocket emcee: a television face, crown and clapping hands. */
    box(p, 93, 40, 12, 5, CORAL, 1);
    box(p, 91, 37, 4, 7, CORAL, 0); box(p, 103, 37, 4, 7, CORAL, 0);
    box(p, 70, 46, 58, 43, INK, 9);
    box(p, 74, 49, 50, 34, 0xFFFBEB, 6);
    box(p, 83, scene == 5 ? 61 : 58, 6, scene == 5 ? 3 : 8, INK, 1);
    box(p, 109, scene == 5 ? 61 : 58, 6, scene == 5 ? 3 : 8, INK, 1);
    box(p, 79, 68, 9, 4, 0xF1A096, 2); box(p, 110, 68, 9, 4, 0xF1A096, 2);
    box(p, 95, 68, 9, s_state.active && s_state.tier == 2 ? 10 : 4, INK, 2);
    box(p, 83, 87, 32, 9, JADE, 2);
    box(p, 93, 87, 12, 5, CORAL, 1);
    s_arms[0] = box(p, 48, 65, 16, 18, INK, 5); box(s_arms[0], 3, 2, 10, 12, 0xFFFBEB, 3);
    s_arms[1] = box(p, 134, 65, 16, 18, INK, 5); box(s_arms[1], 3, 2, 10, 12, 0xFFFBEB, 3);
    for (unsigned i = 0; i < 5; i++) {
        s_people[i] = box(p, 21 + i * 34, 98, 18, 13, i % 2 ? JADE : INK, 5);
        box(s_people[i], 3, 1, 12, 4, i % 2 ? 0x98DDCE : 0x75859C, 2);
    }
    for (unsigned i = 0; i < 10; i++) {
        s_bits[i] = box(p, 25 + (i * 19) % 148, 38 + (i * 13) % 40, 3, 5, i % 2 ? CORAL : JADE, 0);
        if (!s_state.active) lv_obj_add_flag(s_bits[i], LV_OBJ_FLAG_HIDDEN);
    }
}
static void render(void) {
    for (unsigned i = 0; i < 2; i++) s_arms[i] = NULL;
    for (unsigned i = 0; i < 5; i++) s_people[i] = NULL;
    for (unsigned i = 0; i < 10; i++) s_bits[i] = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == PH_LIVE) {
        lv_obj_t *t = line("", 0, JADE);
        lv_label_set_text_fmt(t, "%u / 7   %s", s_state.selected + 1,
            !s_audio_ok ? "声音未就绪" : !s_state.volume ? "安静捧场" : s_state.active ? heat[s_state.tier] : "今天你来控场");
        stage(s_state.active ? s_state.played : s_state.selected);
        line(s_state.active ? ph_lines[s_state.played][s_state.tier] : s_state.selected == PH_SCENES ? "下一声，会是什么呢" : "这一刻，就差你捧个场", 144, INK);
        for (unsigned i = 0; i < 3; i++) box(s_content, 43 + i * 39, 169, 34, 5,
            s_state.active && i <= s_state.tier ? CORAL : 0xDAD8C6, 2);
        t = label(s_content, "", 0, 184, 198, &pocket_hype_zh_12, JADE);
        lv_label_set_text_fmt(t, "本次捧场 %u 次  ·  点亮 %u/6", (unsigned)s_state.total, ph_collected(&s_state));
        lv_label_set_text(s_footer, "上下选场面  确定捧场\n连按更热闹  长按确定设置");
    } else if (s_state.page == PH_MENU) {
        line("后台准备室", 0, JADE);
        const char *items[] = {"", "", "接梗挑战", "怎么玩", "返回现场"};
        for (unsigned i = 0; i < 5; i++) {
            lv_obj_t *row = box(s_content, 0, 28 + i * 31, 198, 27, i == s_state.menu ? JADE : 0xF1EAD6, 5);
            lv_obj_t *t = label(row, items[i], 0, 3, 198, &pocket_hype_zh_16, i == s_state.menu ? 0xFFFFFF : INK);
            if (i == 0) lv_label_set_text_fmt(t, "声音：%s", volumes[s_state.volume]);
            if (i == 1) lv_label_set_text_fmt(t, "中文短句：%s", s_state.voice ? "开启" : "关闭");
        }
        label(s_content, "设置与纪录重启后复原", 0, 187, 198, &pocket_hype_zh_12, JADE);
        lv_label_set_text(s_footer, "上下选择  确定切换\n长按确定返回现场");
    } else if (s_state.page == PH_HELP) {
        line("一键救场指南", 0, JADE);
        const char *rows[] = {"上下选场面，确定就捧场", "连按同一场面，气势升级", "停一会儿，回到小小回应", "惊喜盲盒，相邻不重复", "接梗挑战，选出合适回应", "长按确定，随时返回现场"};
        for (unsigned i = 0; i < 6; i++) label(s_content, rows[i], 0, 31 + i * 26, 198, &pocket_hype_zh_12, INK);
        lv_label_set_text(s_footer, "确定返回设置\n声音可以关闭，表情照样捧场");
    } else if (s_state.page == PH_CHALLENGE) {
        lv_obj_t *t = line("", 0, JADE); lv_label_set_text_fmt(t, "接梗挑战  %u / 6", s_state.round + 1);
        box(s_content, 0, 30, 198, 55, 0xE0EEE7, 7);
        label(s_content, ph_cues[s_state.order[s_state.round]][s_state.cues[s_state.round]], 9, 40, 180, &pocket_hype_zh_16, INK);
        line("这个时候，你会怎么捧", 97, INK);
        lv_obj_t *card = box(s_content, 0, 126, 198, 42, JADE, 6);
        label(card, ph_names[s_state.selected], 0, 6, 198, &pocket_hype_zh_24, 0xFFFFFF);
        t = label(s_content, "", 0, 183, 198, &pocket_hype_zh_12, JADE);
        lv_label_set_text_fmt(t, "选项 %u/6  ·  已接住 %u 个梗", s_state.selected + 1, s_state.score);
        lv_label_set_text(s_footer, "上下选回应  确定接梗\n不用抢时间  长按确定回现场");
    } else if (s_state.page == PH_FEEDBACK) {
        line(s_state.correct ? "接得漂亮！" : "这次试试这样接", 0, s_state.correct ? JADE : CORAL);
        stage(s_state.played);
        line(ph_lines[s_state.played][s_state.tier], 144, INK);
        label(s_content, "捧场没有标准答案，开心就好", 0, 184, 198, &pocket_hype_zh_12, JADE);
        lv_label_set_text(s_footer, "确定继续下一题\n长按确定返回现场");
    } else {
        line("今日气氛组鉴定", 0, JADE);
        lv_obj_t *t = label(s_content, "", 0, 33, 198, &pocket_hype_zh_24, CORAL);
        lv_label_set_text_fmt(t, "接住 %u / 6", s_state.score);
        ui_pixel_mascot_create(s_content, 80, 72);
        line(s_state.score == 6 ? "全场导演" : s_state.score >= 4 ? "气氛担当" : "暖场新星", 134, JADE);
        label(s_content, "把它递给朋友，再来六个梗", 0, 174, 198, &pocket_hype_zh_12, INK);
        lv_label_set_text(s_footer, "确定再来一轮\n长按确定回现场试试身手");
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键未就绪\n请重启后再试");
}
static void battery(lv_timer_t *t) {
    (void)t; if (!s_battery) return;
    int n = bsp_battery_soc();
    if (n < 0) lv_label_set_text(s_battery, "电量未知");
    else lv_label_set_text_fmt(s_battery, "%d%%", n > 100 ? 100 : n);
}
static bool handle(input_t v, int64_t now) {
    s_last_activity = now;
    if (s_off) {
        s_off = s_dimmed = false; s_wake_guard = true; s_wake_button = v.button;
        bsp_display_backlight(85); return false;
    }
    if (s_dimmed) { s_dimmed = false; bsp_display_backlight(85); }
    /* Consume the entire waking gesture, including its later click/long event. */
    if (s_wake_guard) {
        if (s_wake_button == BSP_BTN_OK && v.button == BSP_BTN_OK) {
            if (v.event != BSP_BTN_PRESS) s_wake_guard = false;
            return false;
        }
        if (v.event == BSP_BTN_PRESS) s_wake_guard = false;
        else if (v.button == s_wake_button) return false;
    }
    if (v.button == BSP_BTN_OK && v.event == BSP_BTN_LONG) {
        ph_audio_stop(); ph_back(&s_state); return true;
    }
    if (v.button != BSP_BTN_OK && v.event == BSP_BTN_PRESS) {
        if (s_state.page == PH_LIVE || s_state.page == PH_CHALLENGE) ph_audio_stop();
        ph_move(&s_state, v.button == BSP_BTN_UP ? -1 : 1); return true;
    }
    if (v.button == BSP_BTN_OK && (v.event == BSP_BTN_CLICK || v.event == BSP_BTN_DOUBLE)) {
        ph_audio_stop();
        int clip = ph_confirm(&s_state, v.event == BSP_BTN_DOUBLE ? 2 : 1);
        if (clip >= 0) ph_audio_play((unsigned)clip, s_state.volume, s_state.voice);
        return true;
    }
    return false;
}
static void animate(int64_t now) {
    if (!s_arms[0]) return;
    unsigned step = (unsigned)(now / (s_state.active ? 100 : 450));
    int lift = s_state.active ? (int)((step & 1) * (4 + s_state.tier * 3)) : 0;
    lv_obj_set_y(s_arms[0], 65 - lift); lv_obj_set_y(s_arms[1], 65 - lift);
    for (unsigned i = 0; i < 5; i++) lv_obj_set_y(s_people[i], 98 - (s_state.active ? (int)((step + i) % 3) * 3 : 0));
    if (s_state.active) for (unsigned i = 0; i < 10; i++) {
        lv_obj_set_pos(s_bits[i], 25 + (i * 19) % 148, 38 + (step * 4 + i * 13) % 47);
    }
}
static void frame(lv_timer_t *t) {
    (void)t; int64_t now = now_ms();
    bool dirty = ph_tick(&s_state, (uint32_t)(now - s_last_tick)); s_last_tick = now;
    input_t v;
    for (unsigned i = 0; i < 8 && xQueueReceive(s_inputs, &v, 0) == pdTRUE; i++) {
        if (now - v.at <= 500) dirty |= handle(v, now);
    }
    bool audio_ok = ph_audio_ready();
    if (audio_ok != s_audio_ok) { s_audio_ok = audio_ok; dirty = true; }
    if (!s_dimmed && now - s_last_activity >= 60000) { s_dimmed = true; bsp_display_backlight(18); }
    if (!s_off && now - s_last_activity >= 180000) { s_off = true; ph_audio_stop(); bsp_display_backlight(0); }
    if (dirty) render();
    if (!s_off) animate(now);
}
void pocket_hype_prepare(void) {
    if (!s_inputs) s_inputs = xQueueCreateStatic(8, sizeof(input_t), s_storage, &s_control);
}
void pocket_hype_key(bsp_btn_t button, bsp_btn_ev_t event) {
    if (!atomic_load(&s_accept)) return;
    if (button != BSP_BTN_OK && event != BSP_BTN_PRESS) return;
    input_t v = {button, event, now_ms()}; (void)xQueueSend(s_inputs, &v, 0);
}
void pocket_hype_enter(bool buttons_available) {
    if (s_screen) return;
    pocket_hype_prepare(); ph_init(&s_state, (uint32_t)now_ms());
    s_buttons = buttons_available; s_off = s_dimmed = s_wake_guard = false;
    s_audio_ok = ph_audio_ready(); s_last_activity = s_last_tick = now_ms();
    s_screen = ui_pixel_screen_create("");
    label(s_screen, "口袋捧场王", 5, 16, 151, &pocket_hype_zh_16, 0xFFFFFF);
    s_battery = label(s_screen, "", 163, 31, 65, &pocket_hype_zh_12, INK);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, CREAM);
    s_footer = label(s_screen, "", 2, 288, 236, &pocket_hype_zh_12, INK);
    lv_obj_set_style_text_line_space(s_footer, 1, 0);
    render(); battery(NULL);
    s_timer = lv_timer_create(frame, 40, NULL); s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen); bsp_display_backlight(85);
    xQueueReset(s_inputs); atomic_store(&s_accept, buttons_available);
}
void pocket_hype_exit(void) {
    atomic_store(&s_accept, false); ph_audio_stop();
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    /* Audio owns no UI; cancellation is asynchronous and safe after deletion. */
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = s_content = s_battery = s_footer = NULL;
}
