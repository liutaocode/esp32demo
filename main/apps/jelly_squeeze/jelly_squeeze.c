#include "jelly_squeeze.h"
#include "jelly_squeeze_state.h"
#include "jelly_squeeze_storage.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"

#include <stdatomic.h>

LV_FONT_DECLARE(jelly_squeeze_zh_16);
LV_FONT_DECLARE(jelly_squeeze_zh_24);

/* 面板内可用区域 220 x 228：顶栏、赛道、底栏。 */
enum {
    FIELD_W = JS_FIELD_W,
    FIELD_H = 228,
    CENTER  = FIELD_W / 2,
    BAR_Y   = JS_ARENA_Y + JS_ARENA_H + 1,
    BAR_H   = FIELD_H - BAR_Y,
};

#define SKY_BG    0xE6F5FC
#define SKY_LINE  0xC9E6F4
#define FLOOR     0xE0B98C
#define FLOOR_DIM 0xC49A6C
#define STEEL     0x7E8CA0
#define STEEL_RIM 0xB0BCCB
#define STEEL_DIM 0x5A6675
#define GHOST     0x5F9FC0
#define GOOD      0x2FA36B
#define BAD       0xC53A46

typedef struct { const char *name; uint32_t body, shade, blush; } flavor_t;

/* 八种口味，按累计过关数逐个解锁；配色只在这里定义。 */
static const flavor_t FLAVOR[JS_FLAVORS] = {
    {"草莓", 0xF4657E, 0xC93C58, 0xFFB6C4},
    {"柠檬", 0xFFD24D, 0xD9A519, 0xFFEFAF},
    {"青提", 0x8FD65A, 0x5FA531, 0xD5F2B6},
    {"蜜桃", 0xFFA46B, 0xDC7638, 0xFFD3B6},
    {"蓝莓", 0x7C9BF2, 0x4C6BC7, 0xC3D3FB},
    {"葡萄", 0xB98BEA, 0x8B5FC2, 0xE1CCF7},
    {"薄荷", 0x6FDCC8, 0x3AA895, 0xC2F3EA},
    {"可乐", 0xC08A57, 0x8E6134, 0xE8CBAE},
};

static const char *const SHAPE_NAME[JS_SHAPES] = {"最扁", "扁扁", "圆圆", "高高", "最高"};

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static js_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_footer;
static lv_obj_t *s_arena, *s_gate, *s_jelly, *s_body, *s_gloss, *s_shade;
static lv_obj_t *s_eye[2], *s_spark[2], *s_cheek[2], *s_mouth[3];
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity, s_last_long;
static js_progress_t s_stored;
static bool s_buttons, s_dimmed, s_off, s_suppress_long;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static const flavor_t *flavor(void) { return &FLAVOR[s_state.flavor % JS_FLAVORS]; }

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *outlined(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color,
                          int radius, uint32_t line, int thickness)
{
    lv_obj_t *obj = box(parent, x, y, w, h, color, radius);
    lv_obj_set_style_border_color(obj, lv_color_hex(line), 0);
    lv_obj_set_style_border_width(obj, thickness, 0);
    return obj;
}

static lv_obj_t *label_at(lv_obj_t *parent, const char *value, int x, int y, int w,
                          const lv_font_t *font, uint32_t color, lv_text_align_t align)
{
    lv_obj_t *obj = ui_pixel_label(parent, value, font, color);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_obj_set_style_text_align(obj, align, 0);
    return obj;
}

static lv_obj_t *line_at(lv_obj_t *parent, const char *value, int y, uint32_t color)
{
    return label_at(parent, value, 0, y, FIELD_W, &jelly_squeeze_zh_16, color, LV_TEXT_ALIGN_CENTER);
}

static const char *mode_name(void) { return s_state.mode ? "一口气" : "悠着点"; }

static const char *rank_name(unsigned cleared)
{
    if (cleared >= 21) return "果冻宗师";
    if (cleared >= 13) return "洞口老手";
    if (cleared >= 7) return "形状达人";
    if (cleared >= 3) return "挤挤新手";
    return "手生果冻";
}

/* 一只小果冻：圆角身体、底部暗面、高光、眼睛带反光点、腮红和一张三段的笑嘴。
 * 一共十一个对象，逐帧只改位置和尺寸，不重建。 */
static void jelly_build(lv_obj_t *parent)
{
    const flavor_t *f = flavor();
    s_jelly = box(parent, 0, 0, 10, 10, 0, 0);
    lv_obj_set_style_bg_opa(s_jelly, LV_OPA_TRANSP, 0);
    s_body = outlined(s_jelly, 0, 0, 10, 10, f->body, 4, UI_INK, 3);
    s_shade = box(s_jelly, 0, 0, 4, 4, f->shade, 3);
    lv_obj_set_style_bg_opa(s_shade, LV_OPA_40, 0);
    s_gloss = box(s_jelly, 0, 0, 4, 4, 0xFFFFFF, 3);
    lv_obj_set_style_bg_opa(s_gloss, LV_OPA_60, 0);
    s_cheek[0] = box(s_jelly, 0, 0, 4, 3, f->blush, 2);
    s_cheek[1] = box(s_jelly, 0, 0, 4, 3, f->blush, 2);
    for (unsigned i = 0; i < 2; i++) {
        s_eye[i] = box(s_jelly, 0, 0, 4, 5, UI_INK, 3);
        s_spark[i] = box(s_jelly, 0, 0, 2, 2, 0xFFFFFF, 1);
    }
    for (unsigned i = 0; i < 3; i++) s_mouth[i] = box(s_jelly, 0, 0, 3, 3, UI_INK, 1);
}

/* 按当前宽高摆好果冻的五官；被闸门卡住时压扁一点，看得出是被挤到了。 */
static void jelly_layout(void)
{
    if (!s_jelly) return;
    js_size_t body = js_body(&s_state);
    int w = body.w, h = body.h;
    if (s_state.page == JS_FAIL) { w += 10; h -= 8; }
    if (h < 20) h = 20;
    lv_obj_set_pos(s_jelly, CENTER - w / 2, JS_GROUND - h);
    lv_obj_set_size(s_jelly, w, h);
    int cx = w / 2, small = w < h ? w : h;
    lv_obj_set_size(s_body, w, h);
    lv_obj_set_style_radius(s_body, small * 48 / 100, 0);

    int shade_w = w - small * 40 / 100, shade_h = h * 22 / 100;
    if (shade_h < 4) shade_h = 4;
    lv_obj_set_pos(s_shade, cx - shade_w / 2, h - shade_h - 4);
    lv_obj_set_size(s_shade, shade_w, shade_h);

    int gloss_w = w * 26 / 100, gloss_h = h * 20 / 100;
    if (gloss_w < 5) gloss_w = 5;
    if (gloss_h < 5) gloss_h = 5;
    lv_obj_set_pos(s_gloss, cx - w * 30 / 100, h * 14 / 100);
    lv_obj_set_size(s_gloss, gloss_w, gloss_h);

    /* 晃得越厉害眼睛睁得越大，稳下来就眯回去。 */
    int wobble = js_wobble(&s_state);
    int eye_w = w * 17 / 100, eye_h = h * 20 / 100 + wobble / 16;
    if (eye_w < 6) eye_w = 6;
    if (eye_h < 6) eye_h = 6;
    if (eye_h > h * 34 / 100) eye_h = h * 34 / 100;
    if (s_state.page == JS_FAIL) eye_h = 4;
    int eye_y = h * 32 / 100, eye_dx = w * 22 / 100;
    for (unsigned i = 0; i < 2; i++) {
        int x = cx + (i ? eye_dx : -eye_dx) - eye_w / 2;
        lv_obj_set_pos(s_eye[i], x, eye_y);
        lv_obj_set_size(s_eye[i], eye_w, eye_h);
        int spark = eye_w / 3 < 2 ? 2 : eye_w / 3;
        lv_obj_set_pos(s_spark[i], x + eye_w / 5, eye_y + eye_h / 5);
        lv_obj_set_size(s_spark[i], spark, spark);
        lv_obj_set_style_opa(s_spark[i], s_state.page == JS_FAIL ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    }

    int cheek_w = w * 16 / 100, cheek_h = h * 9 / 100;
    if (cheek_w < 5) cheek_w = 5;
    if (cheek_h < 3) cheek_h = 3;
    int cheek_y = eye_y + eye_h + h * 5 / 100;
    if (cheek_y + cheek_h > h - 5) cheek_y = h - 5 - cheek_h;
    for (unsigned i = 0; i < 2; i++) {
        lv_obj_set_pos(s_cheek[i], cx + (i ? w * 33 / 100 : -w * 33 / 100) - cheek_w / 2, cheek_y);
        lv_obj_set_size(s_cheek[i], cheek_w, cheek_h);
    }

    /* 嘴由三段拼成：平时中间下沉是笑，卡住时拉成一条直线。 */
    bool stuck = s_state.page == JS_FAIL;
    int mouth_w = w * (stuck ? 30 : 22) / 100;
    if (mouth_w < 9) mouth_w = 9;
    int piece = mouth_w / 3, drop = stuck ? 0 : 2;
    int mouth_y = cheek_y + cheek_h / 2;
    if (mouth_y + drop + 3 > h - 4) mouth_y = h - 7 - drop;
    for (unsigned i = 0; i < 3; i++) {
        lv_obj_set_pos(s_mouth[i], cx - mouth_w / 2 + (int)i * piece, mouth_y + (i == 1 ? drop : 0));
        lv_obj_set_size(s_mouth[i], piece, 3);
    }
}

/* 闸门：整块钢板中间留一个门洞，门框颜色随判定变绿或变红。 */
static void gate_build(lv_obj_t *parent)
{
    js_size_t hole = js_gate(&s_state);
    int left = CENTER - hole.w / 2, right = CENTER + hole.w / 2;
    int lintel = JS_GATE_H - hole.h;
    uint32_t frame = s_state.page == JS_PASS ? GOOD : (s_state.page == JS_FAIL ? BAD : UI_INK);

    s_gate = box(parent, 0, 0, FIELD_W, JS_GATE_H, 0, 0);
    lv_obj_set_style_bg_opa(s_gate, LV_OPA_TRANSP, 0);
    box(s_gate, 0, 0, left, JS_GATE_H, STEEL, 0);
    box(s_gate, right, 0, FIELD_W - right, JS_GATE_H, STEEL, 0);
    box(s_gate, left, 0, hole.w, lintel, STEEL, 0);
    box(s_gate, 0, 0, FIELD_W, 4, STEEL_RIM, 0);
    box(s_gate, 0, JS_GATE_H - 4, left, 4, STEEL_DIM, 0);
    box(s_gate, right, JS_GATE_H - 4, FIELD_W - right, 4, STEEL_DIM, 0);
    for (int i = 0; i < 3; i++) {
        box(s_gate, 12, 22 + i * 30, 9, 9, STEEL_DIM, 4);
        box(s_gate, FIELD_W - 21, 22 + i * 30, 9, 9, STEEL_DIM, 4);
    }
    /* 门框：左右门柱与门楣，通过时变绿，卡住时变红。 */
    box(s_gate, left - 3, lintel - 3, 3, hole.h + 3, frame, 0);
    box(s_gate, right, lintel - 3, 3, hole.h + 3, frame, 0);
    box(s_gate, left - 3, lintel - 3, hole.w + 6, 3, frame, 0);
}

static void arena_build(void)
{
    s_arena = box(s_content, 0, JS_ARENA_Y, FIELD_W, JS_ARENA_H, SKY_BG, 0);
    /* 两条竖导轨把赛道框成一台压模机，闸门就沿着它们落下来。 */
    for (unsigned side = 0; side < 2; side++) {
        int x = side ? FIELD_W - 7 : 0;
        box(s_arena, x, 0, 7, JS_GROUND, STEEL_RIM, 0);
        box(s_arena, side ? FIELD_W - 3 : 0, 0, 3, JS_GROUND, STEEL_DIM, 0);
        for (int i = 0; i < 5; i++) box(s_arena, x + 1, 12 + i * 32, 4, 16, STEEL, 0);
    }
    for (int i = 0; i < 4; i++) box(s_arena, 26 + i * 48, 34 + (i % 2) * 46, 9, 9, SKY_LINE, 4);
    box(s_arena, 0, JS_GROUND, FIELD_W, JS_ARENA_H - JS_GROUND, FLOOR, 0);
    box(s_arena, 0, JS_GROUND, FIELD_W, 3, UI_INK, 0);
    for (int x = 0; x < FIELD_W; x += 16) box(s_arena, x, JS_GROUND + 6, 9, 4, FLOOR_DIM, 1);
}

static void round_scene(void)
{
    arena_build();
    js_size_t hole = js_gate(&s_state), want = js_shape_of(s_state.shape_index);
    /* 地面上的两道刻线把门洞宽度投到果冻脚下，随时能对齐。 */
    box(s_arena, CENTER - hole.w / 2 - 4, JS_GROUND + 3, 4, 9, UI_INK, 0);
    box(s_arena, CENTER + hole.w / 2, JS_GROUND + 3, 4, 9, UI_INK, 0);
    if (!s_state.mode) {
        /* 悠着点会画出目标轮廓，一口气只能看门洞。 */
        lv_obj_t *ghost = outlined(s_arena, CENTER - want.w / 2, JS_GROUND - want.h,
                                   want.w, want.h, 0, (want.w < want.h ? want.w : want.h) * 48 / 100,
                                   GHOST, 3);
        lv_obj_set_style_bg_opa(ghost, LV_OPA_TRANSP, 0);
    }
    gate_build(s_arena);
    /* 果冻画在闸门之上：过门时从门洞里看得见，卡住时能看见它压在钢板上出不去。 */
    jelly_build(s_arena);
    jelly_layout();
    lv_obj_set_y(s_gate, js_gate_bottom(&s_state) - JS_GATE_H);
}

static void hud(void)
{
    lv_obj_t *level = label_at(s_content, "", 2, 0, 74, &jelly_squeeze_zh_16, UI_INK, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text_fmt(level, "第%u关", s_state.level + 1u);
    lv_obj_t *score = label_at(s_content, "", 78, 0, 82, &jelly_squeeze_zh_16, UI_INK, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(score, "%u分", (unsigned)s_state.score);
    unsigned total = s_state.mode ? 1u : 3u;
    for (unsigned i = 0; i < total; i++) {
        bool alive = i < s_state.lives;
        outlined(s_content, 164 + (int)i * 18, 3, 15, 15,
                 alive ? flavor()->body : 0xCBD5DC, 6, UI_INK, 2);
    }
}

static void status_bar(void)
{
    label_at(s_content, "稳住", 2, BAR_Y, 36, &jelly_squeeze_zh_16, UI_INK, LV_TEXT_ALIGN_LEFT);
    unsigned total = s_state.mode ? 2u : 3u;
    for (unsigned i = 0; i < total; i++) {
        outlined(s_content, 42 + (int)i * 17, BAR_Y + 4, 13, 13,
                 i < s_state.saves ? UI_YELLOW : 0xCBD5DC, 6, UI_INK, 2);
    }
    lv_obj_t *hint = label_at(s_content, "", 100, BAR_Y, 118, &jelly_squeeze_zh_16,
                              UI_SKY_DARK, LV_TEXT_ALIGN_RIGHT);
    if (s_state.combo >= 2) lv_label_set_text_fmt(hint, "连击 %u", s_state.combo);
    else lv_label_set_text_fmt(hint, "目标 %s", SHAPE_NAME[s_state.shape_index]);
}

static const char *play_feedback(void)
{
    if (s_state.page == JS_PASS) {
        if (!s_state.perfect) return "过了！再稳一点更值钱";
        return s_state.combo >= 3 ? "连着刚刚好，稳得很" : "刚刚好，贴着门洞过";
    }
    if (s_state.page == JS_FAIL) return s_state.lives ? "卡住了，形状差一点" : "最后一次也卡住了";
    return NULL;
}

static void home_page(void)
{
    line_at(s_content, "看门洞，把果冻挤成那形状", 0, UI_INK);
    lv_obj_t *stage = box(s_content, 0, 26, FIELD_W, 104, SKY_BG, 0);
    box(stage, 0, 0, 63, 86, STEEL, 0);
    box(stage, 157, 0, 63, 86, STEEL, 0);
    box(stage, 63, 0, 94, 16, STEEL, 0);
    box(stage, 0, 0, FIELD_W, 4, STEEL_RIM, 0);
    for (int i = 0; i < 2; i++) {
        box(stage, 14, 30 + i * 28, 9, 9, STEEL_DIM, 4);
        box(stage, 197, 30 + i * 28, 9, 9, STEEL_DIM, 4);
    }
    box(stage, 60, 13, 3, 73, UI_INK, 0);
    box(stage, 157, 13, 3, 73, UI_INK, 0);
    box(stage, 60, 13, 100, 3, UI_INK, 0);
    box(stage, 0, 86, FIELD_W, 18, FLOOR, 0);
    box(stage, 0, 86, FIELD_W, 3, UI_INK, 0);
    for (int x = 0; x < FIELD_W; x += 16) box(stage, x, 92, 9, 4, FLOOR_DIM, 1);
    box(stage, 30, 22, 8, 8, SKY_LINE, 4);
    box(stage, 184, 54, 8, 8, SKY_LINE, 4);
    const flavor_t *f = flavor();
    /* 首页这只果冻和赛道上那只画法一致，只是尺寸固定。 */
    lv_obj_t *ball = outlined(stage, CENTER - 30, 26, 60, 60, f->body, 28, UI_INK, 3);
    lv_obj_t *shade = box(ball, 11, 36, 32, 13, f->shade, 5);
    lv_obj_set_style_bg_opa(shade, LV_OPA_40, 0);
    lv_obj_t *gloss = box(ball, 6, 8, 15, 11, 0xFFFFFF, 5);
    lv_obj_set_style_bg_opa(gloss, LV_OPA_60, 0);
    box(ball, 4, 32, 9, 5, f->blush, 2);
    box(ball, 41, 32, 9, 5, f->blush, 2);
    for (unsigned i = 0; i < 2; i++) {
        int x = i ? 31 : 13;
        box(ball, x, 18, 10, 12, UI_INK, 5);
        box(ball, x + 2, 20, 3, 3, 0xFFFFFF, 1);
    }
    box(ball, 21, 38, 4, 3, UI_INK, 1);
    box(ball, 25, 40, 4, 3, UI_INK, 1);
    box(ball, 29, 38, 4, 3, UI_INK, 1);

    lv_obj_t *plate = outlined(s_content, 30, 136, 160, 38, UI_YELLOW, 6, UI_INK, 3);
    label_at(plate, mode_name(), 0, 1, 154, &jelly_squeeze_zh_24, UI_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *taste = line_at(s_content, "", 178, UI_GRASS_DARK);
    unsigned need = js_unlock_need(&s_state);
    if (need) lv_label_set_text_fmt(taste, "%s果冻　再过 %u 关开新味", flavor()->name, need);
    else lv_label_set_text_fmt(taste, "%s果冻　八种口味全开", flavor()->name);
    lv_obj_t *best = line_at(s_content, "", 202, UI_SKY_DARK);
    lv_label_set_text_fmt(best, "本机最高 %u 分", (unsigned)s_state.best[s_state.mode]);
    lv_label_set_text(s_footer, "上换口味 / 下换难度 / 确定开始");
}

static void result_page(void)
{
    const char *head = s_state.cleared >= 13 ? "挤得真顺" :
                       s_state.cleared >= 5 ? "越挤越有感觉" : "这次卡住了";
    label_at(s_content, head, 0, 0, FIELD_W, &jelly_squeeze_zh_24, UI_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *score = label_at(s_content, "", 0, 40, FIELD_W, &lv_font_montserrat_20,
                               UI_SKY_DARK, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(score, "%u", (unsigned)s_state.score);
    line_at(s_content, rank_name(s_state.cleared), 68, UI_GRASS_DARK);
    lv_obj_t *row = line_at(s_content, "", 94, UI_INK);
    lv_label_set_text_fmt(row, "过门 %u　刚刚好 %u", s_state.cleared, s_state.perfects);
    row = line_at(s_content, "", 118, UI_INK);
    lv_label_set_text_fmt(row, "最高连击 %u　%s", s_state.best_combo, mode_name());
    box(s_content, 8, 146, FIELD_W - 16, 32, SKY_BG, 4);
    row = line_at(s_content, "", 151, UI_SKY_DARK);
    lv_label_set_text_fmt(row, "同题挑战 %04u", s_state.challenge);
    line_at(s_content, s_state.new_best ? "新纪录！递给朋友试试" : "拍下成绩，递给朋友挑战",
            186, s_state.new_best ? UI_RED : UI_INK);
    lv_label_set_text(s_footer, "确定同题 / 上换题 / 下返回");
}

static void render(void)
{
    s_arena = s_gate = s_jelly = s_body = s_gloss = s_shade = NULL;
    for (unsigned i = 0; i < 2; i++) s_eye[i] = s_spark[i] = s_cheek[i] = NULL;
    for (unsigned i = 0; i < 3; i++) s_mouth[i] = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == JS_HOME) {
        home_page();
    } else if (s_state.page == JS_RESULT) {
        result_page();
    } else {
        hud();
        round_scene();
        status_bar();
        const char *feedback = s_state.page == JS_PAUSED ? "已暂停，果冻在等你" : play_feedback();
        if (feedback) {
            lv_obj_t *banner = outlined(s_arena, 6, 8, FIELD_W - 12, 30, UI_PAPER, 6, UI_INK, 3);
            label_at(banner, feedback, 0, 3, FIELD_W - 18, &jelly_squeeze_zh_16,
                     s_state.page == JS_FAIL ? BAD : UI_INK, LV_TEXT_ALIGN_CENTER);
        }
        lv_label_set_text(s_footer, s_state.page == JS_PAUSED ?
            "上键继续 / 下键返回首页" : "上拉高 / 下压扁 / 确定稳住");
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启设备");
}

/* 每帧只移动闸门和果冻，不重建页面。 */
static void animate(void)
{
    if (!s_gate || !s_jelly) return;
    lv_obj_set_y(s_gate, js_gate_bottom(&s_state) - JS_GATE_H);
    jelly_layout();
}

static void battery(lv_timer_t *timer)
{
    (void)timer;
    /* I2C 读取可能阻塞，只在不计时的页面上做。 */
    if (!s_battery || s_state.page == JS_PLAY || s_state.page == JS_PASS) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}

static void persist(void)
{
    js_progress_t now = js_save(&s_state);
    if (now.total_cleared == s_stored.total_cleared &&
        now.best_relaxed == s_stored.best_relaxed && now.best_dash == s_stored.best_dash) return;
    s_stored = now;
    js_storage_save(now);
}

static bool handle(input_t input, int64_t now)
{
    s_last_activity = now;
    if (s_off) {
        s_off = s_dimmed = false;
        s_suppress_long = true;
        bsp_display_backlight(100);
        return false;
    }
    if (s_dimmed) { s_dimmed = false; bsp_display_backlight(100); }
    if (input.event == BSP_BTN_LONG) {
        if (s_suppress_long) return false;
        s_last_long = now;
        if (s_state.page == JS_RESULT || s_state.page == JS_HOME) { js_home(&s_state); return true; }
        js_pause(&s_state);
        return true;
    }
    s_suppress_long = false;
    /* 长按已经处理过，抬手后补来的单击不再当成一次操作。 */
    if (input.event == BSP_BTN_CLICK && now - s_last_long < 400) return false;
    if (s_state.page == JS_HOME) {
        if (input.button == BSP_BTN_OK) { js_start(&s_state, (uint16_t)(esp_random() % 10000u)); return true; }
        if (input.button == BSP_BTN_UP) return js_pick_flavor(&s_state, 1);
        s_state.mode ^= 1U;
        return true;
    }
    if (s_state.page == JS_RESULT) {
        if (input.button == BSP_BTN_OK) js_start(&s_state, s_state.challenge);
        else if (input.button == BSP_BTN_UP) js_start(&s_state, (uint16_t)((s_state.challenge + 1u) % 10000u));
        else js_home(&s_state);
        return true;
    }
    if (s_state.page == JS_PAUSED) {
        if (input.button == BSP_BTN_UP) { js_pause(&s_state); return true; }
        if (input.button == BSP_BTN_DOWN) { js_home(&s_state); return true; }
        return false;
    }
    if (input.button == BSP_BTN_OK) return js_steady(&s_state);
    /* 拉高压扁只改目标档位，形状由弹性模拟慢慢跟上，所以要提前动手。 */
    js_press(&s_state, input.button == BSP_BTN_UP ? 1 : -1);
    return false;
}

static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    uint32_t elapsed = (uint32_t)(now - s_last_frame);
    s_last_frame = now;
    input_t input;
    bool handled = false, rebuild = false;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (!handled && now - input.at <= 200) { rebuild = handle(input, now); handled = true; }
    }
    js_page_t previous = s_state.page;
    unsigned level = s_state.level, saves = s_state.saves, combo = s_state.combo;
    js_tick(&s_state, elapsed);
    rebuild |= previous != s_state.page || level != s_state.level ||
               saves != s_state.saves || combo != s_state.combo;
    if (!s_dimmed && now - s_last_activity >= 60000) {
        if (s_state.page == JS_PLAY || s_state.page == JS_PASS || s_state.page == JS_FAIL) {
            js_pause(&s_state);
            rebuild = true;
        }
        s_dimmed = true;
        bsp_display_backlight(20);
    }
    if (!s_off && now - s_last_activity >= 180000) { s_off = true; bsp_display_backlight(0); }
    if (s_state.page == JS_RESULT || s_state.new_unlock) persist();
    if (rebuild) render();
    else animate();
}

void jelly_squeeze_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
}

void jelly_squeeze_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    bool wanted = (button != BSP_BTN_OK && event == BSP_BTN_PRESS) ||
                  (button == BSP_BTN_OK && (event == BSP_BTN_CLICK || event == BSP_BTN_LONG));
    if (!wanted) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}

void jelly_squeeze_enter(bool buttons_available, js_progress_t saved)
{
    if (s_screen) return;
    jelly_squeeze_prepare();
    js_home(&s_state);
    js_load(&s_state, saved);
    s_stored = saved;
    s_buttons = buttons_available;
    s_dimmed = s_off = s_suppress_long = false;
    s_last_frame = s_last_activity = now_ms();
    s_last_long = 0;
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = label_at(s_screen, "一挤就过", 5, 15, 151, &jelly_squeeze_zh_16,
                               0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    (void)title;
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 163, 29);
    lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 6, 46, 228, 236, UI_PAPER);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    s_footer = label_at(s_screen, "", 2, 294, 236, &jelly_squeeze_zh_16, UI_INK, LV_TEXT_ALIGN_CENTER);
    render();
    battery(NULL);
    s_timer = lv_timer_create(frame, 30, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen);
    bsp_display_backlight(100);
    xQueueReset(s_queue);
    atomic_store(&s_accept, buttons_available);
}

void jelly_squeeze_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_content = s_battery = s_footer = NULL;
    s_arena = s_gate = s_jelly = s_body = s_gloss = s_shade = NULL;
    for (unsigned i = 0; i < 2; i++) s_eye[i] = s_spark[i] = s_cheek[i] = NULL;
    for (unsigned i = 0; i < 3; i++) s_mouth[i] = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
