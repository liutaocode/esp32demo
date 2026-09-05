#include "memory_garden.h"
#include "memory_garden_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdint.h>

LV_FONT_DECLARE(memory_garden_zh_18);

#define ZH_FONT (&memory_garden_zh_18)
#define MEMORY_GARDEN_DIM_AFTER_MS 60000U
#define MEMORY_GARDEN_OFF_AFTER_MS 180000U
#define MEMORY_GARDEN_CARD_MS 1400U
#define MEMORY_GARDEN_FEEDBACK_MS 1000U

typedef struct {
    const char *name;
    uint32_t color;
} memory_garden_symbol_t;

static const memory_garden_symbol_t SYMBOLS[MEMORY_GARDEN_SYMBOL_COUNT] = {
    { "太阳", UI_YELLOW },
    { "花朵", 0xED6F9D },
    { "树叶", UI_GRASS },
    { "小鸟", 0x4C8DCE },
    { "果实", UI_RED },
    { "雨滴", 0x64C7EC },
};

static memory_garden_state_t s_state;
static lv_obj_t *s_screen;
static lv_obj_t *s_content;
static lv_obj_t *s_battery;
static lv_timer_t *s_battery_timer;
static lv_timer_t *s_idle_timer;
static lv_timer_t *s_phase_timer;
static uint32_t s_last_input_ms;
static bool s_buttons_available;
static bool s_backlight_off;
static bool s_backlight_dimmed;

static void render(void);

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

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color,
                            int x, int y, int width, lv_text_align_t align)
{
    lv_obj_t *label = ui_pixel_label(parent, text, font, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

static lv_obj_t *symbol_canvas(lv_obj_t *parent, uint8_t symbol, int x, int y)
{
    lv_obj_t *canvas = block(parent, x, y, 82, 82, 0xEAF7F1);
    lv_obj_set_style_border_width(canvas, 3, 0);
    lv_obj_set_style_border_color(canvas, lv_color_hex(UI_INK), 0);
    uint32_t accent = SYMBOLS[symbol].color;

    switch (symbol) {
    case 0: /* Sun */
        block(canvas, 29, 29, 24, 24, accent);
        block(canvas, 35, 15, 12, 9, accent);
        block(canvas, 35, 58, 12, 9, accent);
        block(canvas, 15, 35, 9, 12, accent);
        block(canvas, 58, 35, 9, 12, accent);
        block(canvas, 21, 21, 8, 8, accent);
        block(canvas, 53, 53, 8, 8, accent);
        break;
    case 1: /* Flower */
        block(canvas, 37, 45, 8, 25, UI_GRASS_DARK);
        block(canvas, 23, 19, 16, 16, accent);
        block(canvas, 43, 19, 16, 16, accent);
        block(canvas, 23, 39, 16, 16, accent);
        block(canvas, 43, 39, 16, 16, accent);
        block(canvas, 33, 29, 16, 16, UI_YELLOW);
        break;
    case 2: /* Leaf */
        block(canvas, 20, 36, 42, 10, UI_GRASS_DARK);
        block(canvas, 28, 26, 34, 10, accent);
        block(canvas, 28, 46, 26, 10, accent);
        block(canvas, 45, 20, 17, 42, accent);
        block(canvas, 20, 42, 8, 24, UI_GRASS_DARK);
        break;
    case 3: /* Bird */
        block(canvas, 21, 31, 39, 24, accent);
        block(canvas, 42, 22, 20, 18, accent);
        block(canvas, 12, 35, 17, 11, 0x3573B4);
        block(canvas, 29, 40, 22, 18, 0x3573B4);
        block(canvas, 61, 30, 10, 7, UI_ORANGE);
        block(canvas, 54, 27, 4, 4, UI_INK);
        block(canvas, 29, 55, 4, 10, UI_INK);
        block(canvas, 48, 55, 4, 10, UI_INK);
        break;
    case 4: /* Berry */
        block(canvas, 37, 15, 8, 13, UI_GRASS_DARK);
        block(canvas, 28, 21, 13, 9, UI_GRASS);
        block(canvas, 41, 21, 13, 9, UI_GRASS);
        block(canvas, 22, 31, 20, 20, accent);
        block(canvas, 42, 31, 20, 20, accent);
        block(canvas, 32, 49, 20, 20, 0xC92F38);
        block(canvas, 27, 35, 5, 5, 0xFFFFFF);
        block(canvas, 47, 35, 5, 5, 0xFFFFFF);
        break;
    default: /* Rain */
        block(canvas, 21, 25, 41, 18, 0xA8DDEA);
        block(canvas, 29, 18, 25, 12, 0xA8DDEA);
        block(canvas, 23, 50, 7, 15, accent);
        block(canvas, 38, 55, 7, 15, accent);
        block(canvas, 53, 50, 7, 15, accent);
        break;
    }
    return canvas;
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery) return;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery, "--%");
    } else {
        if (soc > 100) soc = 100;
        lv_label_set_text_fmt(s_battery, "%d%%", soc);
    }
}

static void render_welcome(void)
{
    make_label(s_content, "看见 · 记住 · 种下", ZH_FONT,
               UI_SKY_DARK, 0, 9, 208, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *mascot = ui_pixel_mascot_create(s_content, 85, 39);
    lv_obj_update_layout(mascot);
    ui_pixel_mascot_jump(mascot);
    make_label(s_content, "温柔的记忆练习", ZH_FONT,
               UI_INK, 0, 98, 208, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "三轮练习，没有失败。\n和家人一起种花园。",
               ZH_FONT, UI_INK, 7, 130, 194,
               LV_TEXT_ALIGN_CENTER);
    make_label(s_content, s_buttons_available ? "确定键  开始"
                                               : "按键不可用",
               ZH_FONT, UI_INK, 12, 188, 184,
               LV_TEXT_ALIGN_CENTER);
}

static void render_round_ready(void)
{
    lv_obj_t *round = make_label(s_content, "", ZH_FONT,
                                 UI_ORANGE, 0, 16, 208,
                                 LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(round, "第 %u / %u 轮", (unsigned)s_state.round + 1U,
                          MEMORY_GARDEN_ROUND_COUNT);
    lv_obj_t *count = make_label(s_content, "", ZH_FONT,
                                 UI_INK, 0, 48, 208,
                                 LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(count, "记住 %u 张卡片", s_state.sequence_length);
    symbol_canvas(s_content, (uint8_t)(s_state.round * 2U), 63, 79);
    make_label(s_content, "慢慢看，大声念出来", ZH_FONT,
               UI_INK, 2, 165, 204,
               LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "确定键  看卡片", ZH_FONT,
               UI_SKY_DARK, 0, 188, 208, LV_TEXT_ALIGN_CENTER);
}

static void render_memorize(void)
{
    lv_obj_t *progress = make_label(s_content, "", ZH_FONT,
                                    UI_SKY_DARK, 0, 4, 208,
                                    LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(progress, "观看  %u / %u",
                          (unsigned)s_state.position + 1U,
                          s_state.sequence_length);
    uint8_t symbol = s_state.sequence[s_state.position];
    symbol_canvas(s_content, symbol, 63, 38);
    make_label(s_content, SYMBOLS[symbol].name, ZH_FONT,
               UI_INK, 0, 132, 208, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "看清楚，大声说出来", ZH_FONT,
               UI_INK, 0, 178, 208, LV_TEXT_ALIGN_CENTER);
}

static void render_recall(void)
{
    lv_obj_t *progress = make_label(s_content, "", ZH_FONT,
                                    UI_SKY_DARK, 0, 3, 208,
                                    LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(progress, "回忆  %u / %u",
                          (unsigned)s_state.position + 1U,
                          s_state.sequence_length);
    make_label(s_content, "刚才下一张是什么？", ZH_FONT,
               UI_INK, 0, 29, 208, LV_TEXT_ALIGN_CENTER);
    symbol_canvas(s_content, s_state.selection, 63, 61);
    make_label(s_content, SYMBOLS[s_state.selection].name,
               ZH_FONT, UI_INK,
               0, 151, 208, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "上下键选择 · 确定种下",
               ZH_FONT, UI_INK, 0, 188, 208,
               LV_TEXT_ALIGN_CENTER);
}

static void render_feedback(void)
{
    const char *headline = s_state.last_correct ? "记住啦！"
                                                : "记忆正在生长";
    make_label(s_content, headline, ZH_FONT,
               s_state.last_correct ? UI_GRASS_DARK : UI_SKY_DARK,
               0, 15, 208, LV_TEXT_ALIGN_CENTER);
    symbol_canvas(s_content, s_state.last_expected, 63, 54);
    make_label(s_content, SYMBOLS[s_state.last_expected].name,
               ZH_FONT, UI_INK,
               0, 143, 208, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, s_state.last_correct ? "开出一朵花！"
                                               : "再看它一次。",
               ZH_FONT, UI_INK, 0, 181, 208,
               LV_TEXT_ALIGN_CENTER);
}

static void render_round_complete(void)
{
    make_label(s_content, "本轮完成", ZH_FONT,
               UI_GRASS_DARK, 0, 15, 208, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *score = make_label(s_content, "", ZH_FONT,
                                 UI_RED, 0, 57, 208,
                                 LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(score, "%u 朵花", s_state.round_correct);
    for (uint8_t index = 0; index < s_state.sequence_length; index++) {
        uint32_t color = index < s_state.round_correct ? UI_YELLOW : UI_MUTED;
        block(s_content, 33 + index * 31, 100, 18, 18, color);
        block(s_content, 40 + index * 31, 118, 4, 23, UI_GRASS_DARK);
    }
    make_label(s_content, "每次尝试都算数", ZH_FONT,
               UI_INK, 4, 157, 200,
               LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "确定键  继续", ZH_FONT,
               UI_SKY_DARK, 0, 188, 208, LV_TEXT_ALIGN_CENTER);
}

static void render_result(void)
{
    make_label(s_content, "你的花园", ZH_FONT,
               UI_SKY_DARK, 0, 1, 208, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, memory_garden_result_title(&s_state),
               ZH_FONT, UI_GRASS_DARK, 0, 24, 208,
               LV_TEXT_ALIGN_CENTER);
    for (uint8_t index = 0; index < MEMORY_GARDEN_TOTAL_ITEMS; index++) {
        int x = 25 + (index % 6U) * 29;
        int y = 59 + (index / 6U) * 34;
        uint32_t color = index < s_state.total_correct ? 0xED6F9D : UI_MUTED;
        block(s_content, x, y, 15, 15, color);
        block(s_content, x + 6, y + 15, 4, 14, UI_GRASS_DARK);
    }
    lv_obj_t *count = make_label(s_content, "", ZH_FONT,
                                 UI_INK, 0, 124, 208,
                                 LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(count, "%u 朵花 + %u 棵新芽",
                          s_state.total_correct,
                          MEMORY_GARDEN_TOTAL_ITEMS - s_state.total_correct);
    lv_obj_t *code = make_label(s_content, "", ZH_FONT,
                                UI_INK, 0, 145, 208,
                                LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(code, "花园 #%04X", s_state.share_code);
    make_label(s_content, "这是游戏，不是健康测试", ZH_FONT,
               UI_INK, 0, 168, 208, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "确定键  再种一次", ZH_FONT,
               UI_SKY_DARK, 0, 190, 208, LV_TEXT_ALIGN_CENTER);
}

static void render(void)
{
    lv_obj_clean(s_content);
    switch (s_state.page) {
    case MEMORY_GARDEN_WELCOME:
        render_welcome();
        break;
    case MEMORY_GARDEN_ROUND_READY:
        render_round_ready();
        break;
    case MEMORY_GARDEN_MEMORIZE:
        render_memorize();
        break;
    case MEMORY_GARDEN_RECALL:
        render_recall();
        break;
    case MEMORY_GARDEN_ITEM_FEEDBACK:
        render_feedback();
        break;
    case MEMORY_GARDEN_ROUND_COMPLETE:
        render_round_complete();
        break;
    case MEMORY_GARDEN_RESULT:
        render_result();
        break;
    }
}

static void phase_tick(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    s_phase_timer = NULL;
    memory_garden_state_advance(&s_state);
    render();
    if (s_state.page == MEMORY_GARDEN_MEMORIZE) {
        s_phase_timer = lv_timer_create(phase_tick, MEMORY_GARDEN_CARD_MS, NULL);
    }
}

static void idle_tick(lv_timer_t *timer)
{
    (void)timer;
    uint32_t idle_ms = lv_tick_elaps(s_last_input_ms);
    if (idle_ms >= MEMORY_GARDEN_OFF_AFTER_MS && !s_backlight_off) {
        bsp_display_backlight(0);
        s_backlight_off = true;
    } else if (idle_ms >= MEMORY_GARDEN_DIM_AFTER_MS && !s_backlight_dimmed) {
        bsp_display_backlight(20);
        s_backlight_dimmed = true;
    }
}

void memory_garden_enter(bool buttons_available)
{
    s_buttons_available = buttons_available;
    s_backlight_off = false;
    s_backlight_dimmed = false;
    s_last_input_ms = lv_tick_get();
    memory_garden_state_init(&s_state);

    s_screen = ui_pixel_screen_create("");
    make_label(s_screen, "天天记一记", ZH_FONT, 0xFFFFFF,
               12, 14, 137, LV_TEXT_ALIGN_CENTER);
    s_battery = make_label(s_screen, "--%", &lv_font_montserrat_14,
                           UI_INK, 158, 31, 76, LV_TEXT_ALIGN_RIGHT);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 224, 226, UI_PAPER);
    lv_obj_set_style_pad_all(s_content, 4, 0);
    render();
    refresh_battery(NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    s_idle_timer = lv_timer_create(idle_tick, 1000, NULL);
    lv_screen_load(s_screen);
}

void memory_garden_exit(void)
{
    if (s_phase_timer) lv_timer_delete(s_phase_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    if (s_idle_timer) lv_timer_delete(s_idle_timer);
    s_phase_timer = NULL;
    s_battery_timer = NULL;
    s_idle_timer = NULL;
    s_content = NULL;
    s_battery = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}

void memory_garden_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK || !s_buttons_available) return;

    bool was_off = s_backlight_off;
    s_last_input_ms = lv_tick_get();
    if (s_backlight_off || s_backlight_dimmed) {
        bsp_display_backlight(100);
        s_backlight_off = false;
        s_backlight_dimmed = false;
    }
    if (was_off || s_phase_timer) return;

    if (s_state.page == MEMORY_GARDEN_RECALL && button != BSP_BTN_OK) {
        memory_garden_state_move(&s_state,
            button == BSP_BTN_DOWN ? 1 : -1);
        render();
        return;
    }

    if (button != BSP_BTN_OK) return;
    memory_garden_event_t change = memory_garden_state_confirm(&s_state,
                                                               esp_random());
    if (change == MEMORY_GARDEN_NO_CHANGE) return;
    render();
    if (s_state.page == MEMORY_GARDEN_MEMORIZE) {
        s_phase_timer = lv_timer_create(phase_tick, MEMORY_GARDEN_CARD_MS, NULL);
    } else if (s_state.page == MEMORY_GARDEN_ITEM_FEEDBACK) {
        s_phase_timer = lv_timer_create(phase_tick, MEMORY_GARDEN_FEEDBACK_MS,
                                        NULL);
    }
}
