#include "vibe_check.h"
#include "vibe_check_audio.h"
#include "vibe_check_state.h"
#include "vibe_check_voice.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdio.h>

LV_FONT_DECLARE(vibe_check_zh_16);

typedef struct {
    const char *name;
    const char *tagline;
    uint32_t color;
    uint8_t energy;
    uint8_t chaos;
    uint8_t focus;
} vibe_result_t;

typedef struct {
    const char *prompt;
    const char *up;
    const char *down;
} vibe_question_t;

static const vibe_question_t QUESTIONS[VIBE_CHECK_QUESTION_COUNT] = {
    { "你现在的电量？", "窝着充电", "能量满格" },
    { "突然多出一小时？", "出门探索", "动手创造" },
    { "剧情突然反转？", "静观其变", "冲上去玩" },
    { "群聊里你通常？", "安静潜水", "带头开聊" },
    { "选一个增益？", "好运加成", "专注加成" },
};

static const vibe_result_t RESULTS[VIBE_CHECK_RESULT_COUNT] = {
    { "安静预言家", "总能看见别人忽略的细节", 0x7161A8, 34, 22, 86 },
    { "幸运探路者", "总能在墙上找到一扇门", 0x4DAA72, 72, 48, 42 },
    { "夜行建造师", "把脑洞一步步变成现实", 0x476A9B, 38, 31, 96 },
    { "梦境黑客", "把古怪想法变成现实", 0xDD6E9F, 78, 69, 81 },
    { "治愈魔法师", "柔软也是一种超能力", 0xA46D43, 45, 39, 70 },
    { "天选主角", "今天自带主角光环", 0xE78B32, 94, 67, 51 },
    { "混沌精灵", "规则只是温柔的建议", 0xD94D64, 83, 98, 33 },
    { "涡轮英雄", "今天一路全速前进", 0xE6B72F, 100, 74, 76 },
};

static lv_obj_t *s_screen;
static lv_obj_t *s_content;
static lv_obj_t *s_battery;
static lv_obj_t *s_battery_fill;
static lv_obj_t *s_reveal_name;
static lv_timer_t *s_reveal_timer;
static vibe_check_state_t s_state;
static bool s_buttons_available;
static uint8_t s_reveal_frame;
static uint64_t s_last_activity_ms;
static bool s_dimmed;
static bool s_screen_off;

#define DIM_AFTER_MS 60000
#define SCREEN_OFF_AFTER_MS 180000

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int width, int height,
                       uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, 0, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    return object;
}

static lv_obj_t *centered_label(lv_obj_t *parent, const char *text,
                                const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = ui_pixel_label(parent, text, font, color);
    lv_obj_set_width(label, lv_obj_get_width(parent));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    return label;
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery, "--");
        lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_TRANSP, 0);
        return;
    }
    if (soc > 100) soc = 100;
    lv_label_set_text_fmt(s_battery, "%d%%", soc);
    lv_obj_set_width(s_battery_fill, LV_MAX(1, (34 * soc + 99) / 100));
    lv_obj_set_style_bg_color(s_battery_fill,
        lv_color_hex(soc <= 20 ? UI_RED : (soc <= 45 ? UI_YELLOW : UI_GRASS)), 0);
    lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_COVER, 0);
}

static void create_battery(void)
{
    lv_obj_t *shell = block(s_screen, 174, 35, 54, 22, UI_INK);
    lv_obj_t *inside = block(shell, 3, 3, 46, 16, 0x3E4650);
    block(s_screen, 228, 41, 4, 10, UI_INK);
    s_battery_fill = block(inside, 3, 3, 34, 10, UI_GRASS);
    s_battery = ui_pixel_label(inside, "--", &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_set_width(s_battery, 44);
    lv_obj_set_pos(s_battery, 0, 0);
    lv_label_set_long_mode(s_battery, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_CENTER, 0);
}

static void create_key_hint(const char *text)
{
    lv_obj_t *hint = block(s_content, 26, 237, 188, 23, 0x403225);
    lv_obj_set_style_border_width(hint, 1, 0);
    lv_obj_set_style_border_color(hint, lv_color_hex(UI_INK), 0);
    centered_label(hint, text, &vibe_check_zh_16, 0xFFFFFF);
}

static void show_welcome(void)
{
    lv_obj_clean(s_content);
    lv_obj_t *card = ui_pixel_panel_create(s_content, 13, 16, 214, 190, UI_PAPER);
    lv_obj_t *headline = ui_pixel_label(card, "今天的你",
                                        &vibe_check_zh_16, UI_INK);
    lv_obj_align(headline, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_t *today = ui_pixel_label(card, "是哪一种？", &vibe_check_zh_16, UI_RED);
    lv_obj_align(today, LV_ALIGN_TOP_MID, 0, 31);

    lv_obj_t *mascot = ui_pixel_mascot_create(card, 79, 68);
    lv_obj_set_style_transform_scale(mascot, 320, 0);
    lv_obj_set_style_transform_pivot_x(mascot, 19, 0);
    lv_obj_set_style_transform_pivot_y(mascot, 24, 0);

    lv_obj_t *copy = ui_pixel_label(card, "五次选择，解锁隐藏人格",
                                    &vibe_check_zh_16, UI_SKY_DARK);
    lv_obj_align(copy, LV_ALIGN_BOTTOM_MID, 0, -6);
    create_key_hint(!s_buttons_available ? "按键不可用" :
                    vibe_check_voice_ready() ? "确定开始 · 长按确定重听" :
                                               "确定开始 · 语音不可用");
}

static lv_obj_t *choice_card(int y, const char *key, const char *text,
                             uint32_t key_color)
{
    lv_obj_t *card = ui_pixel_panel_create(s_content, 15, y, 210, 66, UI_PAPER);
    lv_obj_t *key_box = block(card, 6, 12, 42, 30, key_color);
    centered_label(key_box, key, &vibe_check_zh_16, 0xFFFFFF);
    lv_obj_t *label = ui_pixel_label(card, text, &vibe_check_zh_16, UI_INK);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 60, 0);
    return card;
}

static void show_question(void)
{
    lv_obj_clean(s_content);
    const vibe_question_t *question = &QUESTIONS[s_state.question];

    lv_obj_t *counter = ui_pixel_label(s_content, "", &vibe_check_zh_16, 0xFFFFFF);
    lv_label_set_text_fmt(counter, "第 %u / %u 题", (unsigned)s_state.question + 1,
                          VIBE_CHECK_QUESTION_COUNT);
    lv_obj_set_pos(counter, 18, 8);

    lv_obj_t *prompt = ui_pixel_label(s_content, question->prompt,
                                      &vibe_check_zh_16, UI_INK);
    lv_obj_set_width(prompt, 220);
    lv_obj_set_style_text_align(prompt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(prompt, 10, 34);

    choice_card(69, "上", question->up, UI_SKY_DARK);
    choice_card(147, "下", question->down, UI_RED);
    create_key_hint(vibe_check_voice_ready() ? "上下作答 · 长按确定重听" :
                                               "上下键作答 · 文字模式");
    vibe_check_voice_speak(VC_VOICE_QUESTION_0 + s_state.question);
}

static void trait_bar(lv_obj_t *parent, int y, const char *name, uint8_t value,
                      uint32_t color)
{
    lv_obj_t *label = ui_pixel_label(parent, name, &vibe_check_zh_16, UI_INK);
    lv_obj_set_pos(label, 4, y - 3);
    lv_obj_t *track = block(parent, 70, y, 118, 10, 0xC7CED1);
    lv_obj_set_style_border_width(track, 2, 0);
    lv_obj_set_style_border_color(track, lv_color_hex(UI_INK), 0);
    block(track, 2, 2, (112 * value + 99) / 100, 6, color);
}

static void create_avatar(lv_obj_t *parent, const vibe_result_t *result)
{
    lv_obj_t *frame = block(parent, 7, 7, 72, 72, 0xD9F3FF);
    lv_obj_set_style_border_width(frame, 3, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(UI_INK), 0);
    uint32_t accent = result->color;
    block(frame, 20, 7, 32, 9, accent);
    block(frame, 12, 16, 48, 40, accent);
    block(frame, 16, 23, 40, 28, 0xFFF2D1);
    block(frame, 22, 29, 7, 7, UI_INK);
    block(frame, 43, 29, 7, 7, UI_INK);
    block(frame, 29, 43, 14, 5, UI_INK);
    block(frame, 8, 53, 56, 15, accent);
    block(frame, 3, 57, 12, 11, accent);
    block(frame, 57, 57, 12, 11, accent);
}

static void show_result(void)
{
    lv_obj_clean(s_content);
    const vibe_result_t *result = &RESULTS[s_state.result];
    lv_obj_t *card = ui_pixel_panel_create(s_content, 9, 7, 222, 220, UI_PAPER);
    create_avatar(card, result);

    lv_obj_t *eyebrow = ui_pixel_label(card, "今日人格", &vibe_check_zh_16,
                                       UI_SKY_DARK);
    lv_obj_set_pos(eyebrow, 89, 8);
    lv_obj_t *name = ui_pixel_label(card, result->name, &vibe_check_zh_16, UI_INK);
    lv_obj_set_width(name, 118);
    lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(name, -3, 0);
    lv_obj_set_pos(name, 89, 29);

    lv_obj_t *tagline = ui_pixel_label(card, result->tagline, &vibe_check_zh_16,
                                       result->color);
    lv_obj_set_width(tagline, 202);
    lv_obj_set_style_text_align(tagline, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(tagline, 0, 89);

    trait_bar(card, 124, "能量", result->energy, result->color);
    trait_bar(card, 150, "混沌", result->chaos, result->color);
    trait_bar(card, 176, "专注", result->focus, result->color);

    char code[20];
    snprintf(code, sizeof(code), "人格卡  #%03X", s_state.share_code);
    lv_obj_t *share = ui_pixel_label(card, code, &vibe_check_zh_16, 0xFFFFFF);
    lv_obj_set_style_bg_color(share, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_bg_opa(share, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(share, 7, 0);
    lv_obj_set_style_pad_right(share, 7, 0);
    lv_obj_align(share, LV_ALIGN_BOTTOM_MID, 0, -2);
    create_key_hint(vibe_check_voice_ready() ? "确定再测 · 长按确定重听" :
                                               "确定再测 · 文字模式");
    vibe_check_voice_speak(VC_VOICE_RESULT_0 + s_state.result);
}

static void reveal_tick(lv_timer_t *timer)
{
    if (++s_reveal_frame < 9) {
        uint8_t preview = (uint8_t)((s_state.result + s_reveal_frame * 3U) %
                                    VIBE_CHECK_RESULT_COUNT);
        lv_label_set_text(s_reveal_name, RESULTS[preview].name);
        lv_obj_set_style_text_color(s_reveal_name,
                                    lv_color_hex(RESULTS[preview].color), 0);
        return;
    }
    lv_timer_delete(timer);
    s_reveal_timer = NULL;
    s_reveal_name = NULL;
    show_result();
}

static void start_reveal(void)
{
    vibe_check_voice_stop();
    lv_obj_clean(s_content);
    lv_obj_t *card = ui_pixel_panel_create(s_content, 15, 60, 210, 132, UI_PAPER);
    centered_label(card, "正在揭晓……", &vibe_check_zh_16, UI_SKY_DARK);
    s_reveal_name = ui_pixel_label(card, RESULTS[0].name, &vibe_check_zh_16,
                                   RESULTS[0].color);
    lv_obj_set_width(s_reveal_name, 190);
    lv_obj_set_style_text_align(s_reveal_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_reveal_name, LV_ALIGN_CENTER, 0, 22);
    s_reveal_frame = 0;
    s_reveal_timer = lv_timer_create(reveal_tick, 90, NULL);
}

static void power_tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t idle_ms = (uint64_t)esp_timer_get_time() / 1000ULL - s_last_activity_ms;
    if (!s_screen_off && idle_ms >= SCREEN_OFF_AFTER_MS) {
        bsp_display_backlight(0);
        vibe_check_voice_stop();
        s_screen_off = true;
        s_dimmed = true;
    } else if (!s_dimmed && idle_ms >= DIM_AFTER_MS) {
        bsp_display_backlight(20);
        s_dimmed = true;
    }
}

void vibe_check_enter(bool buttons_available, bool audio_available)
{
    s_buttons_available = buttons_available;
    vibe_check_voice_start(audio_available);
    vibe_check_state_init(&s_state);
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = ui_pixel_label(s_screen, "气场测试", &vibe_check_zh_16,
                                     0xFFFFFF);
    lv_obj_set_pos(title, 5, 15);
    lv_obj_set_width(title, 151);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    create_battery();
    s_content = block(s_screen, 0, 55, 240, 265, UI_SKY);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
    show_welcome();
    refresh_battery(NULL);
    lv_timer_create(refresh_battery, 10000, NULL);
    s_last_activity_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    lv_timer_create(power_tick, 1000, NULL);
    lv_screen_load(s_screen);
    vibe_check_voice_speak(VC_VOICE_WELCOME);
}

void vibe_check_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!s_buttons_available ||
        (event != BSP_BTN_CLICK && event != BSP_BTN_LONG)) {
        return;
    }
    s_last_activity_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    bool was_off = s_screen_off;
    if (s_dimmed) {
        bsp_display_backlight(100);
        s_dimmed = false;
        s_screen_off = false;
    }
    if (was_off || s_reveal_timer) return;

    if (event == BSP_BTN_LONG) {
        if (button != BSP_BTN_OK) return;
        if (s_state.page == VIBE_CHECK_WELCOME) {
            vibe_check_voice_speak(VC_VOICE_WELCOME);
        } else if (s_state.page == VIBE_CHECK_QUESTION) {
            vibe_check_voice_speak(VC_VOICE_QUESTION_0 + s_state.question);
        } else {
            vibe_check_voice_speak(VC_VOICE_RESULT_0 + s_state.result);
        }
        return;
    }

    if (s_state.page == VIBE_CHECK_QUESTION &&
        (button == BSP_BTN_UP || button == BSP_BTN_DOWN)) {
        vibe_check_state_move(&s_state, button == BSP_BTN_UP ? -1 : 1);
        vibe_check_event_t change = vibe_check_state_confirm(&s_state, esp_random());
        if (change == VIBE_CHECK_FINISHED) start_reveal();
        else show_question();
    } else if (button == BSP_BTN_OK && s_state.page != VIBE_CHECK_QUESTION) {
        vibe_check_event_t change = vibe_check_state_confirm(&s_state, esp_random());
        if (change == VIBE_CHECK_FINISHED) start_reveal();
        else show_question();
    }
}
