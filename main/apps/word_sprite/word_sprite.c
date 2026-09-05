#include "word_sprite.h"
#include "word_sprite_audio.h"
#include "word_sprite_catalog.h"
#include "word_sprite_runtime.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(word_sprite_zh_16);
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_input_control;
static uint8_t s_input_buffer[8 * sizeof(input_t)];
static QueueHandle_t s_input_queue;
static atomic_bool s_accept_input;
static ws_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_footer, *s_notice;
static lv_obj_t *s_placeholder;
static lv_obj_t *s_options[3];
static lv_timer_t *s_timer;
static bool s_buttons, s_dimmed, s_off, s_closing, s_last_audio;
static int64_t s_activity, s_battery_at, s_page_at;
static const char *WORLDS[] = {"动物森林", "美味小镇", "校园星球"};
static const char *STAGES[] = {"星光蛋", "嫩芽精灵", "翅膀精灵", "皇冠精灵"};
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(parent, text, &word_sprite_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, 197);
    lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}

static void float_pet(void *object, int32_t y) { lv_obj_set_y(object, y); }

static void pet(int x, int y)
{
    unsigned stage = ws_stage(&s_state);
    lv_obj_t *p = block(s_content, x, y, 88, 64, UI_PAPER);
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    uint32_t body = stage == 0 ? 0xFFF0B5 : stage == 1 ? 0x9DE7BD : 0xC8ADFF;
    block(p, 14, 53, 60, 5, 0xD7D8C7);
    if (stage >= 2) {
        block(p, 0, 22, 18, 25, UI_INK); block(p, 4, 22, 14, 18, 0x94DDEB);
        block(p, 70, 22, 18, 25, UI_INK); block(p, 70, 22, 14, 18, 0x94DDEB);
    }
    block(p, 22, 10, 44, 46, UI_INK);
    block(p, 16, 20, 56, 29, UI_INK);
    block(p, 26, 14, 36, 38, body);
    block(p, 20, 24, 48, 21, body);
    block(p, 26, 28, 6, 9, UI_INK); block(p, 56, 28, 6, 9, UI_INK);
    block(p, 36, 41, 16, 3, UI_INK);
    block(p, 23, 39, 9, 4, 0xF7A8A9); block(p, 56, 39, 9, 4, 0xF7A8A9);
    if (stage >= 1) { block(p, 42, 3, 4, 12, UI_INK); block(p, 46, 1, 14, 7, UI_GRASS); }
    if (stage == 3) {
        block(p, 28, 2, 32, 12, UI_YELLOW);
        block(p, 28, 0, 6, 7, UI_ORANGE); block(p, 41, 0, 6, 7, UI_ORANGE);
        block(p, 54, 0, 6, 7, UI_ORANGE);
    }
    lv_anim_t a;
    lv_anim_init(&a); lv_anim_set_var(&a, p); lv_anim_set_exec_cb(&a, float_pet);
    lv_anim_set_values(&a, y, y - 3); lv_anim_set_duration(&a, 900);
    lv_anim_set_playback_duration(&a, 900); lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void status(void)
{
    const char *save = ws_runtime_save_status() == 2 ? "保存中" :
                       ws_runtime_save_status() == 3 ? "保存失败" :
                       ws_runtime_save_status() == 0 ? "本次有效" : "已保存";
    if (!s_buttons) lv_label_set_text(s_notice, "按键不可用");
    else if (!ws_runtime_audio_ready()) lv_label_set_text(s_notice, "语音故障·阅读模式");
    else lv_label_set_text_fmt(s_notice, "离线语音 · %s", save);
}

static void selection(void)
{
    for (unsigned i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(s_options[i], lv_color_hex(i == s_state.selected ? UI_YELLOW : 0xE3EBEF), 0);
        lv_obj_set_style_border_width(s_options[i], i == s_state.selected ? 2 : 0, 0);
    }
}

static void render(void)
{
    lv_obj_clean(s_content);
    s_page_at = now_ms();
    if (s_state.page == WS_HOME) {
        label(s_content, "单词精灵", 0, UI_INK);
        lv_obj_t *sub = label(s_content, "", 24, UI_SKY_DARK);
        lv_label_set_text_fmt(sub, "%s · 点亮 %u/36", STAGES[ws_stage(&s_state)], ws_count(s_state.progress.learned));
        pet(55, 48);
        lv_obj_t *world = block(s_content, 0, 116, 197, 30, UI_YELLOW);
        label(world, WORLDS[s_state.world], 7, UI_INK);
        for (unsigned i = 0; i < 12; i++) {
            uint64_t bit = UINT64_C(1) << (s_state.world * 12 + i);
            uint32_t color = (s_state.progress.review & bit) ? UI_ORANGE :
                (s_state.progress.learned & bit) ? UI_GRASS : 0xD9DCDD;
            block(s_content, 5 + i * 16, 154, 11, 10, color);
        }
        lv_obj_t *hint = label(s_content, "", 173, UI_INK);
        lv_label_set_text_fmt(hint, "每局 6 词 · 待复习 %u", ws_count(s_state.progress.review));
        lv_label_set_text(s_footer, "上下选岛  确定开始\n长按下键：错词复习");
    } else if (s_state.page == WS_QUESTION) {
        lv_obj_t *heading = label(s_content, "", 0, UI_SKY_DARK);
        lv_label_set_text_fmt(heading, "%s  %u/%u", s_state.reviewing ? "找回单词" : "听力探险",
                              s_state.index + 1, s_state.count);
        const char *prompt = ws_runtime_audio_ready() ? "听到的是哪个词？" : ws_words[ws_word(&s_state)].english;
        label(s_content, prompt, 30, UI_INK);
        for (unsigned i = 0; i < 3; i++) {
            s_options[i] = block(s_content, 0, 65 + i * 40, 197, 32, UI_PAPER);
            lv_obj_set_style_border_color(s_options[i], lv_color_hex(UI_INK), 0);
            lv_obj_t *text = label(s_options[i], ws_words[s_state.options[i]].chinese, 7, UI_INK);
            lv_obj_set_width(text, 193);
        }
        selection();
        lv_label_set_text(s_footer, "上下选择  确定作答\n长按上键：再听一次");
    } else if (s_state.page == WS_FEEDBACK) {
        label(s_content, s_state.last_correct ? "答对啦！" : "再认识它一次", 0, UI_SKY_DARK);
        lv_obj_t *english = label(s_content, ws_words[ws_word(&s_state)].english, 29, UI_INK);
        lv_obj_set_style_text_font(english, &lv_font_montserrat_20, 0);
        label(s_content, ws_words[ws_word(&s_state)].chinese, 58, UI_INK);
        pet(55, 84);
        label(s_content, s_state.last_correct ? "跟着读一遍\n这颗单词星点亮了" : "跟着读一遍\n已加入错词复习", 153, UI_SKY_DARK);
        lv_label_set_text(s_footer, "确定继续  长按上键重听\n长按确定：保存回家");
    } else {
        label(s_content, "探险收获卡", 0, UI_INK);
        label(s_content, STAGES[ws_stage(&s_state)], 24, UI_SKY_DARK);
        pet(55, 47);
        lv_obj_t *score = label(s_content, "", 114, UI_INK);
        lv_label_set_text_fmt(score, "答对 %u/%u · 找回 %u", s_state.correct, s_state.count, s_state.recovered);
        lv_obj_t *total = label(s_content, "", 142, UI_SKY_DARK);
        lv_label_set_text_fmt(total, "图鉴 %u/36 · 待复习 %u", ws_count(s_state.progress.learned), ws_count(s_state.progress.review));
        label(s_content, "拍下成长，休息一下吧", 172, UI_INK);
        lv_label_set_text(s_footer, "确定回家  下键复习\n上键：同岛再练一局");
    }
    status();
}

static void start_round(bool review)
{
    if (!ws_start(&s_state, review)) {
        lv_label_set_text(s_notice, "没有待复习单词，去探险吧");
        return;
    }
    render();
    ws_runtime_speak(review ? WS_VOICE_REVIEW : ws_word(&s_state), review ? ws_word(&s_state) : -1);
}

static void handle_input(input_t in)
{
    if (now_ms() - in.at > 800) return;
    bool was_off = s_off;
    s_activity = now_ms();
    if (s_dimmed) { bsp_display_backlight(100); s_dimmed = s_off = false; }
    if (was_off) return;
    if (in.event == BSP_BTN_LONG) {
        if (in.button == BSP_BTN_OK) {
            ws_runtime_save(s_state.progress); ws_runtime_stop_audio();
            ws_home(&s_state); render();
        } else if (in.button == BSP_BTN_UP) {
            if (s_state.page == WS_FEEDBACK)
                ws_runtime_explain(ws_word(&s_state), -1);
            else if (s_state.page == WS_QUESTION)
                ws_runtime_speak(ws_word(&s_state), -1);
            else ws_runtime_speak(WS_VOICE_WELCOME, -1);
        } else if (in.button == BSP_BTN_DOWN && s_state.page == WS_HOME) start_round(true);
        return;
    }
    if (s_state.page == WS_HOME) {
        if (in.button == BSP_BTN_OK) start_round(false);
        else { ws_move(&s_state, in.button == BSP_BTN_UP ? -1 : 1); render(); }
    } else if (s_state.page == WS_QUESTION) {
        if (in.button == BSP_BTN_OK) {
            ws_answer(&s_state); render();
            ws_runtime_explain(ws_word(&s_state), s_state.last_correct ? WS_VOICE_CORRECT : WS_VOICE_RETRY);
        } else { ws_move(&s_state, in.button == BSP_BTN_UP ? -1 : 1); selection(); }
    } else if (s_state.page == WS_FEEDBACK) {
        /* A fresh deliberate press is needed after correction is displayed. */
        if (in.button == BSP_BTN_OK && in.at - s_page_at >= 600) {
            ws_next(&s_state); render();
            if (s_state.page == WS_RESULT) {
                ws_runtime_save(s_state.progress); ws_runtime_speak(WS_VOICE_FINISH, -1);
            } else ws_runtime_speak(ws_word(&s_state), -1);
        }
    } else {
        if (in.button == BSP_BTN_OK) { ws_home(&s_state); render(); ws_runtime_stop_audio(); }
        else start_round(in.button == BSP_BTN_DOWN);
    }
}

static void tick(lv_timer_t *timer)
{
    if (s_closing) {
        if (ws_runtime_stopped()) {
            lv_timer_delete(timer); s_timer = NULL;
            /* Load a neutral screen before deleting the active page. */
            if (lv_screen_active() == s_screen) {
                s_placeholder = lv_obj_create(NULL);
                lv_screen_load(s_placeholder);
            }
            lv_obj_delete(s_screen); s_screen = NULL;
        }
        return;
    }
    input_t in;
    for (unsigned i = 0; i < 8 && xQueueReceive(s_input_queue, &in, 0) == pdTRUE; i++) handle_input(in);
    int64_t now = now_ms();
    if (now - s_battery_at >= 10000) {
        int soc = bsp_battery_soc();
        if (soc < 0) lv_label_set_text(s_battery, "--%");
        else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
        s_battery_at = now;
        status();
    }
    if (s_last_audio != ws_runtime_audio_ready()) {
        s_last_audio = ws_runtime_audio_ready();
        render();
    }
    if (now - s_activity >= 180000 && !s_off) {
        bsp_display_backlight(0); s_off = s_dimmed = true; ws_runtime_stop_audio();
    } else if (now - s_activity >= 60000 && !s_dimmed) {
        bsp_display_backlight(20); s_dimmed = true;
    }
    /* Save state is cheap/atomic; update after completion without waiting 10 s. */
    static int last_save = -1;
    int save = ws_runtime_save_status();
    if (save != last_save) { last_save = save; status(); }
}

void word_sprite_prepare(void)
{
    atomic_store(&s_accept_input, false);
    s_input_queue = xQueueCreateStatic(8, sizeof(input_t), s_input_buffer, &s_input_control);
}

void word_sprite_enter(bool buttons_available, ws_progress_t progress)
{
    if (s_screen) return;
    s_buttons = buttons_available;
    s_closing = s_dimmed = s_off = false;
    s_last_audio = ws_runtime_audio_ready();
    s_activity = now_ms(); s_battery_at = s_activity - 10000;
    ws_init(&s_state, esp_random(), progress);
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = label(s_screen, "单词精灵", 15, 0xFFFFFF);
    lv_obj_set_pos(title, 5, 15);
    lv_obj_set_width(title, 151);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_set_pos(s_battery, 175, 36); lv_obj_set_width(s_battery, 57);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 66, 219, 209, UI_PAPER);
    lv_obj_set_style_pad_all(s_content, 4, 0);
    s_notice = ui_pixel_label(s_screen, "", &word_sprite_zh_16, 0xFFFFFF);
    lv_obj_set_pos(s_notice, 4, 45); lv_obj_set_width(s_notice, 164);
    lv_obj_set_style_text_font(s_notice, &word_sprite_zh_16, 0);
    lv_label_set_long_mode(s_notice, LV_LABEL_LONG_CLIP);
    s_footer = ui_pixel_label(s_screen, "", &word_sprite_zh_16, UI_INK);
    lv_obj_set_pos(s_footer, 0, 282); lv_obj_set_width(s_footer, 240);
    lv_obj_set_style_text_line_space(s_footer, -2, 0);
    lv_obj_set_style_text_align(s_footer, LV_TEXT_ALIGN_CENTER, 0);
    render(); lv_screen_load(s_screen);
    if (s_placeholder) { lv_obj_delete(s_placeholder); s_placeholder = NULL; }
    xQueueReset(s_input_queue);
    s_timer = lv_timer_create(tick, 30, NULL);
    atomic_store(&s_accept_input, buttons_available);
    ws_runtime_speak(WS_VOICE_WELCOME, -1);
}

void word_sprite_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept_input) || (event != BSP_BTN_CLICK && event != BSP_BTN_LONG)) return;
    input_t in = {button, event, now_ms()};
    xQueueSend(s_input_queue, &in, 0);
}

void word_sprite_exit(void)
{
    if (!s_screen || s_closing) return;
    atomic_store(&s_accept_input, false);
    ws_runtime_save(s_state.progress);
    ws_runtime_shutdown();
    s_closing = true;
}
