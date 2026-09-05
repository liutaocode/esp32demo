#include "idiom_pet.h"
#include "idiom_pet_state.h"
#include "idiom_pet_storage.h"
#include "idiom_pet_audio.h"
#include "idiom_pet_voice.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>

LV_FONT_DECLARE(idiom_pet_zh_16);
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static ip_state_t s_state;
static bool s_prepared, s_buttons, s_dimmed, s_off;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_footer, *s_save;
static lv_timer_t *s_frame, *s_battery_timer;
static lv_obj_t *s_audio_hint;
static bool s_audio_ready;
static int64_t s_activity, s_feedback_at;
static unsigned s_save_status = 99;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(parent, text, &idiom_pet_zh_16, color);
    lv_obj_set_pos(o, 0, y);
    lv_obj_set_width(o, 198);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(o, 3, 0);
    return o;
}

static const char *const s_sprites[IP_PETS] = {
    "..11....11.." "..121..121.." "..121..121.." "..12222221.."
    ".1222222221." ".1232222321." ".1222442221." "..12222221.."
    "...122221..." "..12222221.." "..12211221.." "...11..11...",
    ".11......11." ".121....121." ".1221111221." "..12222221.."
    ".1222222221." ".1232222321." "..12244221.." "...122221..."
    "..12222221.." "..122222211." "...12221121." "....111111..",
    "............" ".11......11." ".1211111121." ".1222222221."
    ".1222222221." ".1232222321." ".1222442221." "..12222221.."
    ".1222222221." "122222222221" ".1222222221." "..11111111..",
    ".....11....." "....1221...." "...122221..." "..12222221.."
    "..12322321.." "..12244221.." ".1122222211." "122222222221"
    ".1122222211." "...122221..." "....1111...." "....1..1....",
    ".1.1....1.1." ".111....111." "..11....11.." "..12111121.."
    ".1222222221." ".1232222321." "..12244221.." "...122221..."
    "...122221..." "..12222221.." "..12111121.." "...1....1...",
    "...11..11..." "...121121..." "..12222221.." ".1222222221."
    ".1232222321." "..12244221.." "1..122221..1" "121122221121"
    "122222222221" ".1112222111." "...122221..." "...11..11...",
};
static const uint32_t s_colors[IP_PETS] = {
    0x9BCD68, 0xFFB66B, 0xB8DFF8, 0xCAB8F4, 0xF5D16C, 0xB9B1EF
};

static void pet(int index, bool egg, int y)
{
    lv_obj_t *root = block(s_content, 69, y, 60, 60, UI_PAPER);
    if (egg) {
        block(root, 20, 3, 20, 6, UI_INK);
        block(root, 10, 9, 40, 42, UI_INK);
        block(root, 5, 24, 50, 21, UI_INK);
        block(root, 15, 9, 30, 39, UI_YELLOW);
        block(root, 10, 27, 40, 15, UI_YELLOW);
        block(root, 20, 18, 10, 8, UI_ORANGE);
        block(root, 32, 33, 8, 8, UI_ORANGE);
    } else {
        const char *pixels = s_sprites[index];
        const uint32_t palette[] = {0, UI_INK, s_colors[index], 0xFFFFFF, 0xED8293};
        for (int row = 0; row < 12; row++) {
            for (int col = 0; col < 12;) {
                char c = pixels[row * 12 + col];
                int end = col + 1;
                while (end < 12 && pixels[row * 12 + end] == c) end++;
                if (c >= '1' && c <= '4')
                    block(root, col * 5, row * 5, (end - col) * 5, 5, palette[c - '0']);
                col = end;
            }
        }
    }
}

static void render(void)
{
    s_audio_hint = NULL;
    lv_obj_clean(s_content);
    char text[100];
    switch (s_state.page) {
    case IP_HOME:
        label(s_content, "读懂一个故事", 0, UI_INK);
        label(s_content, "唤醒一只小兽", 25, UI_GRASS_DARK);
        pet(s_state.land * 2, true, 51);
        snprintf(text, sizeof(text), "<  %s  >", ip_land_names[s_state.land]);
        label(s_content, text, 121, UI_SKY_DARK);
        snprintf(text, sizeof(text), "每轮四题 / 图鉴 %u/6", ip_count(s_state.progress.pets));
        label(s_content, text, 151, UI_INK);
        label(s_content, "不计时，想好了再选", 181, UI_INK);
        lv_label_set_text(s_footer, "上下选岛 / 确定出发");
        break;
    case IP_QUIZ: {
        snprintf(text, sizeof(text), "%s %u/%u  %s", s_state.reviewing ? "复习" : "故事",
                 (unsigned)(s_state.reviewing ? s_state.review_index : s_state.index) + 1,
                 s_state.reviewing ? s_state.review_count : IP_ROUND,
                 ip_land_names[s_state.land]);
        label(s_content, text, 0, UI_SKY_DARK);
        const ip_question_t *q = &ip_questions[s_state.question];
        label(s_content, s_state.reviewing ? q->review : q->story, 27, UI_INK);
        for (unsigned i = 0; i < 3; i++) {
            bool chosen = s_state.selected == i;
            lv_obj_t *row = block(s_content, 0, 97 + i * 34, 198, 29,
                                    chosen ? UI_YELLOW : 0xDFEAD9);
            snprintf(text, sizeof(text), "%c  %s", chosen ? '>' : ' ', ip_option(&s_state, i));
            label(row, text, 4, UI_INK);
        }
        lv_label_set_text(s_footer, "上下选择 / 确定作答");
        break;
    }
    case IP_FEEDBACK: {
        const ip_question_t *q = &ip_questions[s_state.question];
        label(s_content, s_state.last_correct ? "懂啦！获得知识能量" : "没关系，一起弄懂", 0,
              s_state.last_correct ? UI_GRASS_DARK : UI_SKY_DARK);
        lv_obj_t *badge = block(s_content, 0, 34, 198, 36, UI_YELLOW);
        label(badge, q->idiom, 9, UI_INK);
        label(s_content, q->meaning, 85, UI_INK);
        label(s_content, s_state.last_correct ? "试着用它说一句话吧" : "稍后换个故事再试试", 149, UI_SKY_DARK);
        s_audio_hint = label(s_content, ip_voice_ready() ? "上键重听 / 下键停止" :
                             "语音不可用，可看文字", 182, UI_INK);
        lv_label_set_text(s_footer, "确定继续 / 长按回家");
        break;
    }
    case IP_HATCH:
        label(s_content, s_state.new_pet ? "新伙伴破壳啦！" : "老朋友又来陪你啦", 0, UI_GRASS_DARK);
        pet(s_state.pet, false, 31);
        label(s_content, ip_pet_names[s_state.pet], 104, UI_INK);
        snprintf(text, sizeof(text), "首次答对 %u/4", s_state.first_correct);
        label(s_content, text, 133, UI_SKY_DARK);
        label(s_content, "本轮四个故事已完成", 160, UI_INK);
        label(s_content, "休息一下，讲给朋友听", 181, UI_INK);
        lv_label_set_text(s_footer, "确定看图鉴 / 长按回家");
        break;
    case IP_ALBUM: {
        bool unlocked = s_state.progress.pets & (1U << s_state.cursor);
        snprintf(text, sizeof(text), "我的萌兽 %u/6", ip_count(s_state.progress.pets));
        label(s_content, text, 0, UI_GRASS_DARK);
        pet(s_state.cursor, !unlocked, 31);
        label(s_content, unlocked ? ip_pet_names[s_state.cursor] : "还在蛋里等你", 104, UI_INK);
        snprintf(text, sizeof(text), "%02u / 06  %s", s_state.cursor + 1,
                 ip_land_names[s_state.cursor / 2]);
        label(s_content, text, 133, UI_SKY_DARK);
        snprintf(text, sizeof(text), "已练会 %u / 待复习 %u", ip_count(s_state.progress.learned),
                 ip_count(s_state.progress.pending));
        label(s_content, text, 160, UI_INK);
        label(s_content, unlocked ? "用一个成语介绍它吧" : "每岛完成两轮收齐伙伴", 181, UI_INK);
        lv_label_set_text(s_footer, "上下翻页 / 确定回家");
        break;
    }
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启检查");
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}

static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    input_t input;
    bool handled = false, dirty = false;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (handled || now - input.at > 250) continue;
        handled = true;
        s_activity = now;
        bool waking = s_off;
        if (s_dimmed || s_off) bsp_display_backlight(100);
        s_dimmed = s_off = false;
        if (waking) continue;
        ip_progress_t before = s_state.progress;
        ip_page_t page = s_state.page;
        if (input.event == BSP_BTN_LONG && input.button == BSP_BTN_OK) {
            if (page == IP_HOME) ip_album(&s_state);
            else ip_home(&s_state);
        } else if (input.event == BSP_BTN_CLICK) {
            if (page == IP_FEEDBACK && input.button == BSP_BTN_UP)
                ip_voice_speak(s_state.question);
            else if (page == IP_FEEDBACK && input.button == BSP_BTN_DOWN) ip_voice_stop();
            else if (input.button == BSP_BTN_UP) ip_move(&s_state, -1);
            else if (input.button == BSP_BTN_DOWN) ip_move(&s_state, 1);
            else if (input.button == BSP_BTN_OK) {
                /* Do not let a queued double tap skip the explanation. */
                if (page != IP_FEEDBACK || now - s_feedback_at >= 900)
                    ip_confirm(&s_state);
            }
        }
        if (s_state.page == IP_FEEDBACK && page != IP_FEEDBACK) s_feedback_at = now;
        int voice = ip_audio_transition(page, &s_state);
        if (voice >= 0) ip_voice_speak((unsigned)voice);
        else if (voice == IP_AUDIO_STOP) ip_voice_stop();
        if (before.learned != s_state.progress.learned ||
            before.pending != s_state.progress.pending || before.pets != s_state.progress.pets)
            ip_storage_save(s_state.progress);
        dirty = true;
    }
    if (!s_dimmed && now - s_activity >= 60000) {
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (!s_off && now - s_activity >= 180000) {
        bsp_display_backlight(0); s_off = true;
        ip_voice_stop();
    }
    if (dirty) render();
    bool ready = ip_voice_ready();
    if (s_audio_hint && ready != s_audio_ready)
        lv_label_set_text(s_audio_hint, ready ? "上键重听 / 下键停止" : "语音不可用，可看文字");
    s_audio_ready = ready;
    unsigned status = ip_storage_status();
    if (status != s_save_status || dirty) {
        s_save_status = status;
        lv_label_set_text(s_save, status == 2 ? "存档不可用，本次仍可玩" :
            status == 1 ? "保存中，请稍候关机" :
            s_state.page == IP_HOME ? "已保存 / 长按确定看图鉴" : "进度已保存");
    }
}

void idiom_pet_prepare(void)
{
    if (s_prepared) return;
    s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
    ip_init(&s_state, esp_random(), ip_storage_init());
    s_prepared = true;
}

void idiom_pet_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    if (event != BSP_BTN_CLICK && !(event == BSP_BTN_LONG && button == BSP_BTN_OK)) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}

void idiom_pet_enter(bool buttons_available)
{
    if (s_screen || !s_prepared) return;
    s_buttons = buttons_available;
    s_dimmed = s_off = false;
    s_activity = now_ms();
    ip_home(&s_state);
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = label(s_screen, "成语萌兽", 16, 0xFFFFFF);
    lv_obj_set_width(title, 151);
    lv_obj_set_x(title, 5);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 164, 33);
    lv_obj_set_width(s_battery, 64);
    lv_label_set_long_mode(s_battery, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 53, 220, 218, UI_PAPER);
    lv_obj_set_style_pad_bottom(s_content, 0, 0);
    s_footer = label(s_screen, "", 297, UI_INK);
    lv_obj_set_width(s_footer, 240);
    s_save = label(s_screen, "", 278, UI_INK);
    lv_obj_set_width(s_save, 240);
    lv_obj_set_style_bg_color(s_save, lv_color_hex(UI_GRASS), 0);
    lv_obj_set_style_bg_opa(s_save, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_footer, lv_color_hex(UI_GRASS), 0);
    lv_obj_set_style_bg_opa(s_footer, LV_OPA_COVER, 0);
    s_save_status = 99;
    render();
    refresh_battery(NULL);
    s_frame = lv_timer_create(frame, 25, NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);
    bsp_display_backlight(100);
    xQueueReset(s_queue);
    atomic_store(&s_accept, buttons_available);
}

void idiom_pet_exit(void)
{
    atomic_store(&s_accept, false);
    ip_voice_stop();
    s_audio_hint = NULL;
    if (s_frame) lv_timer_delete(s_frame);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_frame = s_battery_timer = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = s_content = s_battery = s_footer = s_save = NULL;
    /* Storage worker owns only copied values and may finish after UI exit. */
}
