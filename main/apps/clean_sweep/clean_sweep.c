#include "clean_sweep.h"
#include "clean_sweep_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <string.h>

LV_FONT_DECLARE(clean_sweep_zh_12);
LV_FONT_DECLARE(clean_sweep_zh_16);
LV_FONT_DECLARE(clean_sweep_zh_24);

/* 屏幕 240x320。游戏页整屏盖住主题,井 150x300 加右侧信息栏占满,
   上下不留空条:开局之后画面里只剩井、下一个方块、三个数字和电量,
   按键说明放在首页和暂停页,不占用玩的时候的地方。 */
enum {
    CS_CELL = 15,
    CS_WELL_X = 2,   CS_WELL_Y = 6,
    CS_WELL_BORDER = 3,
    CS_WELL_W = CS_COLS * CS_CELL,   /* 150 */
    CS_WELL_H = CS_ROWS * CS_CELL,   /* 300 */
    CS_HUD_X = 161,  CS_HUD_W = 77,
    CS_HUD_INNER = CS_HUD_W - 2 * CS_WELL_BORDER,
    CS_NIGHT_H = 320,
    CS_PANEL_W = 198,                /* ui_pixel 面板的内容宽度 */
    CS_QUEUE_LEN = 6,
    CS_IDLE_DIM_MS = 60000,
    CS_IDLE_OFF_MS = 180000,
    CS_STALE_MS = 400,
};

#define CS_NIGHT 0x101E33
#define CS_WELL  0x081326
#define CS_GRID  0x1C3050
#define CS_GREEN 0x168B79
#define CS_GLOW  0xFFF6C4

static const uint32_t CS_COLOR[CS_PIECES] = {
    0x35D6E8, /* 长条 */
    0x3E6BE6, /* J */
    0xFF9A2E, /* L */
    0xFFD928, /* 方块 */
    0x62D63C, /* S */
    0xF04438, /* Z */
    0xB264F0, /* T */
};

/* 首页示意图:一口小井,底行铺满正在发光,上方有一个正在落下的 T 形。 */
static const uint8_t CS_DEMO[10][CS_COLS] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 2, 0, 0, 0, 0, 0, 0, 5, 5},
    {2, 2, 0, 0, 0, 0, 0, 3, 5, 5},
    {7, 7, 7, 0, 0, 0, 6, 3, 3, 3},
    {4, 4, 1, 1, 1, 1, 6, 6, 2, 2},
};

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
typedef struct {
    uint8_t page, fx, rot, piece;
    int8_t px, py;
    uint32_t serial, lines, bucket;
} board_key_t;

static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[CS_QUEUE_LEN * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static cs_state_t s_state;
static lv_obj_t *s_screen, *s_stage, *s_battery, *s_hint;
static lv_obj_t *s_well, *s_hud, *s_next, *s_mascot;
static lv_obj_t *s_score, *s_lines, *s_pace, *s_combo;
static lv_obj_t *s_banner, *s_banner_shadow, *s_banner_gain;
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity;
static board_key_t s_shown_board;
static uint32_t s_shown_score, s_shown_lines, s_shown_pace, s_shown_combo;
static uint8_t s_shown_next;
static int s_soc = -1;
static int s_shake;
static bool s_buttons, s_dimmed, s_off, s_suppress_long;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static uint32_t tint(uint32_t color, int percent)
{
    int r = (int)((color >> 16) & 0xFF), g = (int)((color >> 8) & 0xFF), b = (int)(color & 0xFF);
    if (percent >= 0) {
        r += (255 - r) * percent / 100;
        g += (255 - g) * percent / 100;
        b += (255 - b) * percent / 100;
    } else {
        r = r * (100 + percent) / 100;
        g = g * (100 + percent) / 100;
        b = b * (100 + percent) / 100;
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

/* 带墨线边框的容器,和 ui_pixel 面板同一套观感,但内边距归零便于精确排版。 */
static lv_obj_t *framed(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = box(parent, x, y, w, h, color);
    lv_obj_set_style_border_color(obj, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(obj, CS_WELL_BORDER, 0);
    lv_obj_set_style_radius(obj, 3, 0);
    return obj;
}

static lv_obj_t *text_at(lv_obj_t *parent, const char *value, int x, int y, int w,
                         const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = ui_pixel_label(parent, value, font, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

/* ---------- 绘制 ---------- */

/* LVGL 为每个矩形都分配一个绘制任务,再在渲染时才裁剪。井一次重画要发几百个
   矩形,而刷新按 20 行分块进行,所以这里先自己丢掉块外的矩形,省掉大部分分配。 */
static bool visible(lv_layer_t *layer, int x, int y, int w, int h)
{
    const lv_area_t *clip = &layer->_clip_area;
    return x <= clip->x2 && x + w - 1 >= clip->x1 && y <= clip->y2 && y + h - 1 >= clip->y1;
}

static void fill(lv_layer_t *layer, int x, int y, int w, int h,
                 uint32_t color, lv_opa_t opa, int radius)
{
    if (w <= 0 || h <= 0 || !opa || !visible(layer, x, y, w, h)) return;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa = opa;
    dsc.radius = radius;
    lv_area_t area = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &dsc, &area);
}

/* 一格方块:深色底、亮色面、左上角高光,像素风。 */
static void brick(lv_layer_t *layer, int x, int y, int size, uint32_t color, lv_opa_t opa)
{
    if (!visible(layer, x, y, size, size)) return;
    fill(layer, x, y, size, size, tint(color, -50), opa, 2);
    fill(layer, x + 1, y + 1, size - 3, size - 3, color, opa, 2);
    fill(layer, x + 2, y + 2, size / 3, size / 3, tint(color, 50), opa, 1);
}

static void outline(lv_layer_t *layer, int x, int y, int size, uint32_t color)
{
    if (!visible(layer, x, y, size, size)) return;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_TRANSP;
    dsc.border_color = lv_color_hex(color);
    dsc.border_width = 2;
    dsc.border_opa = LV_OPA_60;
    dsc.radius = 2;
    lv_area_t area = {x, y, x + size - 1, y + size - 1};
    lv_draw_rect(layer, &dsc, &area);
}

/* 消除动画的一行:先闪白,再向两边炸开并冒火星,最后整行消失。 */
static void draw_clearing_row(lv_layer_t *layer, int ox, int oy, int row, unsigned progress)
{
    int y = oy + row * CS_CELL;
    if (s_state.fx == CS_FX_COLLAPSE) return;
    if (s_state.fx == CS_FX_FLASH) {
        bool bright = (s_state.fx_ms / 80) % 2 == 0;
        if (bright) {
            fill(layer, ox, y, CS_WELL_W, CS_CELL, CS_GLOW, LV_OPA_COVER, 2);
            return;
        }
        for (int col = 0; col < CS_COLS; col++)
            brick(layer, ox + col * CS_CELL, y, CS_CELL,
                  tint(CS_COLOR[s_state.cell[row][col] - 1], 40), LV_OPA_COVER);
        return;
    }

    int size = CS_CELL - (int)(progress * (CS_CELL - 3) / 1000);
    lv_opa_t opa = (lv_opa_t)(255 - progress * 210 / 1000);
    for (int col = 0; col < CS_COLS; col++) {
        int dir = col < CS_COLS / 2 ? -1 : 1;
        int dx = dir * (int)(progress * 46 / 1000);
        int dy = -(int)(progress * 16 / 1000);
        brick(layer, ox + col * CS_CELL + dx + (CS_CELL - size) / 2,
              y + dy + (CS_CELL - size) / 2, size,
              CS_COLOR[s_state.cell[row][col] - 1], opa);
    }
    for (int spark = 0; spark < 7; spark++) {
        int base = (row * 37 + spark * 43) % CS_WELL_W;
        int rise = (int)(progress * (12 + spark * 3) / 1000);
        int drift = (spark % 2 ? 1 : -1) * (int)(progress * (20 + spark * 4) / 1000);
        int sx = base + drift, sy = y + CS_CELL / 2 - rise;
        if (sx < 0 || sx > CS_WELL_W - 3) continue;
        fill(layer, ox + sx, sy, 3, 3, CS_GLOW, (lv_opa_t)(255 - progress * 255 / 1000), 0);
    }
}

static void draw_well(lv_event_t *event)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_content_coords(lv_event_get_target_obj(event), &area);
    int ox = area.x1, oy = area.y1;

    for (int col = 1; col < CS_COLS; col++)
        fill(layer, ox + col * CS_CELL, oy, 1, CS_WELL_H, CS_GRID, LV_OPA_COVER, 0);
    for (int row = 1; row < CS_ROWS; row++)
        fill(layer, ox, oy + row * CS_CELL, CS_WELL_W, 1, CS_GRID, LV_OPA_COVER, 0);

    bool clearing = s_state.page == CS_CLEARING;
    unsigned progress = clearing ? cs_fx_progress(&s_state) : 0;
    for (int row = 0; row < CS_ROWS; row++) {
        if (clearing && cs_row_clearing(&s_state, row)) {
            draw_clearing_row(layer, ox, oy, row, progress);
            continue;
        }
        int dy = 0;
        if (clearing && s_state.fx == CS_FX_COLLAPSE)
            dy = (int)(cs_row_shift(&s_state, row) * CS_CELL * progress / 1000);
        for (int col = 0; col < CS_COLS; col++)
            if (s_state.cell[row][col])
                brick(layer, ox + col * CS_CELL, oy + row * CS_CELL + dy, CS_CELL,
                      CS_COLOR[s_state.cell[row][col] - 1], LV_OPA_COVER);
    }
    if (clearing || s_state.page == CS_RESULT) return;

    int ghost = cs_ghost_y(&s_state);
    uint32_t color = CS_COLOR[s_state.piece];
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            if (!cs_shape_cell(s_state.piece, s_state.rot, row, col)) continue;
            int bx = ox + (s_state.px + col) * CS_CELL;
            if (ghost != s_state.py)
                outline(layer, bx, oy + (ghost + row) * CS_CELL, CS_CELL, color);
            if (s_state.py + row >= 0)
                brick(layer, bx, oy + (s_state.py + row) * CS_CELL, CS_CELL, color, LV_OPA_COVER);
        }
}

/* 右栏的“下一个”预览,居中摆放。 */
static void draw_next(lv_event_t *event)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_content_coords(lv_event_get_target_obj(event), &area);
    int first_col = 4, last_col = -1, first_row = 4, last_row = -1;
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            if (!cs_shape_cell(s_state.next, 0, row, col)) continue;
            if (col < first_col) first_col = col;
            if (col > last_col) last_col = col;
            if (row < first_row) first_row = row;
            if (row > last_row) last_row = row;
        }
    if (last_col < 0) return;
    int cell = 11;
    int w = (last_col - first_col + 1) * cell, h = (last_row - first_row + 1) * cell;
    int ox = area.x1 + (lv_area_get_width(&area) - w) / 2 - first_col * cell;
    int oy = area.y1 + (lv_area_get_height(&area) - h) / 2 - first_row * cell;
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            if (cs_shape_cell(s_state.next, 0, row, col))
                brick(layer, ox + col * cell, oy + row * cell, cell,
                      CS_COLOR[s_state.next], LV_OPA_COVER);
}

static void draw_demo(lv_event_t *event)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_content_coords(lv_event_get_target_obj(event), &area);
    int cell = 8;
    int ox = area.x1 + (lv_area_get_width(&area) - CS_COLS * cell) / 2, oy = area.y1 + 2;
    fill(layer, ox - 2, oy - 2, CS_COLS * cell + 4, 10 * cell + 4, UI_INK, LV_OPA_COVER, 2);
    fill(layer, ox, oy, CS_COLS * cell, 10 * cell, CS_WELL, LV_OPA_COVER, 0);
    for (int row = 0; row < 10; row++)
        for (int col = 0; col < CS_COLS; col++)
            if (CS_DEMO[row][col])
                brick(layer, ox + col * cell, oy + row * cell, cell,
                      CS_DEMO[row][col] == 1 ? CS_GLOW : CS_COLOR[CS_DEMO[row][col] - 1],
                      LV_OPA_COVER);
    fill(layer, ox, oy + 9 * cell, CS_COLS * cell, cell, CS_GLOW, LV_OPA_40, 0);
    /* 一个正在下落的 T 形和它的落点虚影。 */
    for (int col = 3; col <= 5; col++) {
        brick(layer, ox + col * cell, oy + 2 * cell, cell, CS_COLOR[6], LV_OPA_COVER);
        outline(layer, ox + col * cell, oy + 8 * cell, cell, CS_COLOR[6]);
    }
    brick(layer, ox + 4 * cell, oy + cell, cell, CS_COLOR[6], LV_OPA_COVER);
    outline(layer, ox + 4 * cell, oy + 7 * cell, cell, CS_COLOR[6]);
}

/* ---------- 文案 ---------- */

static const char *mode_name(void)
{
    return s_state.mode == CS_SPRINT ? "冲刺二十行" : "经典无尽";
}

static const char *rank_name(void)
{
    if (s_state.mode == CS_SPRINT) {
        if (!s_state.won) return "先站稳再提速";
        if (s_state.elapsed_ms < 120000) return "闪电清场";
        if (s_state.elapsed_ms < 200000) return "手很稳";
        return "稳扎稳打";
    }
    if (s_state.score >= 12000) return "一格不留";
    if (s_state.score >= 5000) return "消行熟手";
    if (s_state.score >= 1500) return "渐入佳境";
    return "先摆摆看";
}

static void seconds_text(char *out, size_t size, uint32_t ms)
{
    lv_snprintf(out, size, "%u.%u", (unsigned)(ms / 1000), (unsigned)(ms % 1000) / 100);
}

/* ---------- 页面 ---------- */

/* 右栏只留电量、下一个方块和三个数字,标题一律省掉:开局之后没有一句多余的字。 */
static void build_hud(void)
{
    s_hud = framed(s_stage, CS_HUD_X, CS_WELL_Y, CS_HUD_W, CS_WELL_H + 2 * CS_WELL_BORDER,
                   UI_PAPER);
    s_battery = text_at(s_hud, "", 0, 2, CS_HUD_INNER, &clean_sweep_zh_12, UI_INK);
    s_next = box(s_hud, 0, 20, CS_HUD_INNER, 44, 0xE2E6D8);
    lv_obj_set_style_radius(s_next, 3, 0);
    lv_obj_add_event_cb(s_next, draw_next, LV_EVENT_DRAW_MAIN, NULL);

    text_at(s_hud, "分数", 0, 74, CS_HUD_INNER, &clean_sweep_zh_12, UI_INK);
    s_score = text_at(s_hud, "0", 0, 88, CS_HUD_INNER, &lv_font_montserrat_20, UI_INK);
    text_at(s_hud, "消行", 0, 116, CS_HUD_INNER, &clean_sweep_zh_12, UI_INK);
    s_lines = text_at(s_hud, "0", 0, 130, CS_HUD_INNER, &lv_font_montserrat_20, UI_INK);
    text_at(s_hud, s_state.mode == CS_SPRINT ? "用时" : "等级",
            0, 158, CS_HUD_INNER, &clean_sweep_zh_12, UI_INK);
    s_pace = text_at(s_hud, "0", 0, 172, CS_HUD_INNER, &lv_font_montserrat_20, UI_INK);
    s_combo = text_at(s_hud, "", 0, 200, CS_HUD_INNER, &clean_sweep_zh_12, UI_ORANGE);
    s_mascot = ui_pixel_mascot_create(s_hud, (CS_HUD_INNER - 38) / 2, 226);
}

static void build_play(void)
{
    box(s_stage, 0, 0, 240, CS_NIGHT_H, CS_NIGHT);
    s_well = framed(s_stage, CS_WELL_X, CS_WELL_Y, CS_WELL_W + 2 * CS_WELL_BORDER,
                    CS_WELL_H + 2 * CS_WELL_BORDER, CS_WELL);
    lv_obj_add_event_cb(s_well, draw_well, LV_EVENT_DRAW_MAIN, NULL);
    build_hud();

    s_banner_shadow = text_at(s_stage, "", CS_WELL_X + 2, 126, CS_WELL_W + 6,
                              &clean_sweep_zh_24, UI_INK);
    s_banner = text_at(s_stage, "", CS_WELL_X, 124, CS_WELL_W + 6, &clean_sweep_zh_24, CS_GLOW);
    s_banner_gain = text_at(s_stage, "", CS_WELL_X, 158, CS_WELL_W + 6,
                            &clean_sweep_zh_16, UI_YELLOW);
    lv_obj_add_flag(s_banner_shadow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_banner_gain, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_hint, "");
}

static void build_home(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_stage, 8, 52, 220, 226, UI_PAPER);
    text_at(panel, "三键玩的俄罗斯方块", 0, 0, CS_PANEL_W, &clean_sweep_zh_16, UI_INK);
    lv_obj_t *demo = box(panel, 0, 24, CS_PANEL_W, 86, UI_PAPER);
    lv_obj_add_event_cb(demo, draw_demo, LV_EVENT_DRAW_MAIN, NULL);
    text_at(panel, mode_name(), 0, 114, CS_PANEL_W, &clean_sweep_zh_16, CS_GREEN);
    text_at(panel, s_state.mode == CS_SPRINT ? "清满二十行，比谁更快" : "越消越快，堆到顶结束",
            0, 140, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    lv_obj_t *best = text_at(panel, "", 0, 160, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    if (s_state.mode == CS_SPRINT) {
        if (s_state.best_sprint_ms) {
            char value[16];
            seconds_text(value, sizeof(value), s_state.best_sprint_ms);
            lv_label_set_text_fmt(best, "本次开机最快 %s 秒", value);
        } else {
            lv_label_set_text(best, "本次开机还没有纪录");
        }
    } else {
        lv_label_set_text_fmt(best, "本次开机最高 %u 分", (unsigned)s_state.best_score);
    }
    text_at(panel, "上左　下右　确定转　双击落底", 0, 180, CS_PANEL_W,
            &clean_sweep_zh_12, UI_INK);
    lv_label_set_text(s_hint, "上下换模式　确定开始");
}

static void build_paused(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_stage, 8, 52, 220, 226, UI_PAPER);
    text_at(panel, "已暂停，方块等你", 0, 0, CS_PANEL_W, &clean_sweep_zh_16, UI_INK);
    ui_pixel_mascot_create(panel, (CS_PANEL_W - 38) / 2, 28);
    text_at(panel, "上键或确定继续", 0, 84, CS_PANEL_W, &clean_sweep_zh_16, CS_GREEN);
    text_at(panel, "下键回首页", 0, 110, CS_PANEL_W, &clean_sweep_zh_16, UI_INK);
    lv_obj_t *now = text_at(panel, "", 0, 144, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    lv_label_set_text_fmt(now, "当前 %u 分   已消 %u 行",
                          (unsigned)s_state.score, (unsigned)s_state.lines);
    text_at(panel, "上左　下右　确定转　双击落底", 0, 166, CS_PANEL_W,
            &clean_sweep_zh_12, UI_INK);
    lv_label_set_text(s_hint, "确定继续　下键回首页");
}

static void build_result(void)
{
    lv_obj_t *panel = ui_pixel_panel_create(s_stage, 8, 52, 220, 226, UI_PAPER);
    bool sprint = s_state.mode == CS_SPRINT;
    text_at(panel, sprint ? (s_state.won ? "二十行清完" : "堆到顶了") : "堆到顶了",
            0, 0, CS_PANEL_W, &clean_sweep_zh_16, s_state.won ? CS_GREEN : UI_RED);

    lv_obj_t *big = text_at(panel, "", 0, 24, CS_PANEL_W, &clean_sweep_zh_24, UI_INK);
    if (sprint && s_state.won) {
        char value[16];
        seconds_text(value, sizeof(value), s_state.elapsed_ms);
        lv_label_set_text(big, value);
        text_at(panel, "秒", 0, 60, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    } else {
        lv_label_set_text_fmt(big, "%u", (unsigned)s_state.score);
        text_at(panel, "分", 0, 60, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    }

    lv_obj_t *stat = text_at(panel, "", 0, 80, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    lv_label_set_text_fmt(stat, "消行 %u   最长连击 %u",
                          (unsigned)s_state.lines, (unsigned)s_state.best_combo);
    lv_obj_t *stat2 = text_at(panel, "", 0, 98, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    lv_label_set_text_fmt(stat2, "落块 %u   题号 %04u",
                          (unsigned)s_state.pieces, (unsigned)s_state.challenge);
    text_at(panel, rank_name(), 0, 118, CS_PANEL_W, &clean_sweep_zh_16, CS_GREEN);
    text_at(panel, s_state.new_best ? "新纪录！叫朋友来同题挑战" : "确定同题再来一局",
            0, 146, CS_PANEL_W, &clean_sweep_zh_12, s_state.new_best ? UI_RED : UI_INK);
    text_at(panel, "上键换题　下键回首页", 0, 166, CS_PANEL_W, &clean_sweep_zh_12, UI_INK);
    text_at(panel, "同一个题号，方块顺序完全一样", 0, 184, CS_PANEL_W,
            &clean_sweep_zh_12, UI_INK);
    lv_label_set_text(s_hint, "确定同题再来　下键回首页");
}

static void refresh_battery_label(void)
{
    if (!s_battery) return;
    if (s_soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", s_soc > 100 ? 100 : s_soc);
}

/* 电量走 I2C,只在不在局中的页面读,免得读数卡进落块的那一帧。 */
static void battery(lv_timer_t *timer)
{
    (void)timer;
    if (s_state.page == CS_PLAY || s_state.page == CS_CLEARING) return;
    s_soc = bsp_battery_soc();
    refresh_battery_label();
}

/* 消除动画和落块共用同一套布局,页面在两者之间切换时不重建。 */
static int layout_of(cs_page_t page) { return page == CS_CLEARING ? CS_PLAY : page; }

static void render(void)
{
    s_well = s_hud = s_next = s_mascot = NULL;
    s_score = s_lines = s_pace = s_combo = NULL;
    s_banner = s_banner_shadow = s_banner_gain = s_battery = NULL;
    lv_obj_clean(s_stage);
    memset(&s_shown_board, 0xFF, sizeof(s_shown_board));
    s_shake = 0;
    s_shown_score = s_shown_lines = s_shown_pace = s_shown_combo = UINT32_MAX;
    s_shown_next = 0xFF;

    if (s_state.page == CS_HOME) build_home();
    else if (s_state.page == CS_PAUSED) build_paused();
    else if (s_state.page == CS_RESULT) build_result();
    else build_play();

    if (s_state.page != CS_PLAY && s_state.page != CS_CLEARING) {
        s_battery = text_at(s_stage, "", 163, 31, 65, &clean_sweep_zh_12, UI_INK);
        lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    }
    refresh_battery_label();
    if (!s_buttons) lv_label_set_text(s_hint, "按键不可用，请重启设备");
    /* 先把坐标算出来,后面读吉祥物位置做跳动动画时才不会拿到 0。 */
    lv_obj_update_layout(s_stage);
}

/* 每帧只改动会变的文字,并按需要让井重画。 */
static void animate(void)
{
    if (!s_well) return;
    board_key_t key;
    memset(&key, 0, sizeof(key)); /* 填充字节也要清零,后面整块比较 */
    key.page = (uint8_t)s_state.page;
    key.fx = (uint8_t)s_state.fx;
    key.rot = s_state.rot;
    key.piece = s_state.piece;
    key.px = s_state.px;
    key.py = s_state.py;
    key.serial = s_state.piece_serial;
    key.lines = s_state.lines;
    key.bucket = s_state.page == CS_CLEARING ? s_state.fx_ms / 40 : 0;
    if (memcmp(&key, &s_shown_board, sizeof(key)) != 0) {
        s_shown_board = key;
        lv_obj_invalidate(s_well);
    }
    if (s_shown_next != s_state.next) {
        s_shown_next = s_state.next;
        lv_obj_invalidate(s_next);
    }
    if (s_shown_score != s_state.score) {
        s_shown_score = s_state.score;
        lv_label_set_text_fmt(s_score, "%u", (unsigned)s_state.score);
    }
    if (s_shown_lines != s_state.lines) {
        s_shown_lines = s_state.lines;
        lv_label_set_text_fmt(s_lines, "%u", (unsigned)s_state.lines);
    }
    uint32_t pace = s_state.mode == CS_SPRINT ? s_state.elapsed_ms / 100 : s_state.level;
    if (s_shown_pace != pace) {
        s_shown_pace = pace;
        if (s_state.mode == CS_SPRINT) {
            char value[16];
            seconds_text(value, sizeof(value), s_state.elapsed_ms);
            lv_label_set_text(s_pace, value);
        } else {
            lv_label_set_text_fmt(s_pace, "%u", (unsigned)s_state.level);
        }
    }
    uint32_t combo = s_state.page == CS_CLEARING ? s_state.combo : 0;
    if (s_shown_combo != combo) {
        s_shown_combo = combo;
        if (combo > 1) lv_label_set_text_fmt(s_combo, "连击 ×%u", (unsigned)combo);
        else lv_label_set_text(s_combo, "");
    }

    bool show = s_state.page == CS_CLEARING;
    if (show) {
        const char *word = "消掉一行";
        if (s_state.perfect) word = "全清！";
        else if (s_state.clear_count == 4) word = "四行！";
        else if (s_state.clear_count == 3) word = "三行！";
        else if (s_state.clear_count == 2) word = "双行！";
        lv_label_set_text(s_banner, word);
        lv_label_set_text(s_banner_shadow, word);
        lv_label_set_text_fmt(s_banner_gain, "+%u", (unsigned)s_state.gain);
    }
    /* 四行齐消时让井轻轻抖两下。 */
    int shake = 0;
    if (show && s_state.clear_count == 4 && s_state.fx == CS_FX_FLASH)
        shake = (s_state.fx_ms / 60) % 2 ? 2 : -2;
    if (shake != s_shake) {
        s_shake = shake;
        lv_obj_set_x(s_well, CS_WELL_X + shake);
    }
    if (show == lv_obj_has_flag(s_banner, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_t *const banners[] = {s_banner, s_banner_shadow, s_banner_gain};
        for (unsigned i = 0; i < 3; i++) {
            if (show) lv_obj_remove_flag(banners[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(banners[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (show) ui_pixel_mascot_jump(s_mascot);
    }
}

/* ---------- 输入 ---------- */

/* 方向键取按下瞬间,横向移动没有延迟。确定键取单击、双击和长按:
   按键组件对同一次按下只会给出其中一个,单击不会在长按之后补发,双击也不会
   先发一次单击,所以"旋转"和"落底"不会互相触发。 */
static bool handle_move(bsp_btn_t button, int64_t now)
{
    int dir = button == BSP_BTN_UP ? -1 : 1;
    if (s_state.page == CS_PLAY) {
        cs_move(&s_state, dir);
        return false;
    }
    if (s_state.page == CS_HOME) {
        s_state.mode = s_state.mode == CS_SPRINT ? CS_CLASSIC : CS_SPRINT;
        return true;
    }
    if (s_state.page == CS_PAUSED) {
        if (button == BSP_BTN_UP) return cs_resume(&s_state);
        cs_home(&s_state);
        return true;
    }
    if (s_state.page == CS_RESULT) {
        if (button == BSP_BTN_UP) cs_start(&s_state, s_state.challenge + 1);
        else cs_home(&s_state);
        (void)now;
        return true;
    }
    return false;
}

/* 确定键单击:游戏中是旋转,其余页面是这一页的主操作。 */
static bool handle_ok(int64_t now)
{
    if (s_state.page == CS_PLAY) {
        cs_rotate(&s_state);
        return false;
    }
    if (s_state.page == CS_HOME) {
        cs_start(&s_state, (uint32_t)(now % CS_CHALLENGE_MAX));
        return true;
    }
    if (s_state.page == CS_RESULT) {
        cs_start(&s_state, s_state.challenge);
        return true;
    }
    if (s_state.page == CS_PAUSED) return cs_resume(&s_state);
    return false;
}

/* 确定键双击:游戏中直接落底,落点就是虚影的位置;其余页面当成一次单击。 */
static bool handle_double(int64_t now)
{
    if (s_state.page != CS_PLAY) return handle_ok(now);
    cs_drop(&s_state);
    return false;
}

/* 确定键长按:游戏中暂停,暂停页回首页,其余页面不做事。 */
static bool handle_hold(void)
{
    if (s_state.page == CS_PLAY) return cs_pause(&s_state);
    if (s_state.page != CS_PAUSED) return false;
    cs_home(&s_state);
    return true;
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
    if (s_dimmed) {
        s_dimmed = false;
        bsp_display_backlight(100);
    }
    if (input.event == BSP_BTN_LONG) {
        if (s_suppress_long) return false;
        return handle_hold();
    }
    s_suppress_long = false;
    if (input.button != BSP_BTN_OK) return handle_move(input.button, now);
    if (input.event == BSP_BTN_DOUBLE) return handle_double(now);
    return handle_ok(now);
}

static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    uint32_t elapsed = (uint32_t)(now - s_last_frame);
    s_last_frame = now;

    input_t input;
    bool rebuild = false;
    cs_page_t previous = s_state.page;
    for (unsigned i = 0; i < CS_QUEUE_LEN && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++)
        if (now - input.at <= CS_STALE_MS) rebuild |= handle(input, now);
    cs_tick(&s_state, elapsed);
    rebuild |= layout_of(previous) != layout_of(s_state.page);

    if (!s_dimmed && now - s_last_activity >= CS_IDLE_DIM_MS) {
        if (s_state.page == CS_PLAY && cs_pause(&s_state)) rebuild = true;
        s_dimmed = true;
        bsp_display_backlight(20);
    }
    if (!s_off && now - s_last_activity >= CS_IDLE_OFF_MS) {
        s_off = true;
        bsp_display_backlight(0);
    }
    /* 进出游戏页要换整套布局,其余情况只改动会变的部分。 */
    if (rebuild) render();
    else animate();
}

void clean_sweep_prepare(void)
{
    if (!s_queue)
        s_queue = xQueueCreateStatic(CS_QUEUE_LEN, sizeof(input_t), s_queue_storage,
                                     &s_queue_control);
}

void clean_sweep_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    bool wanted = button == BSP_BTN_OK
        ? (event == BSP_BTN_CLICK || event == BSP_BTN_DOUBLE || event == BSP_BTN_LONG)
        : event == BSP_BTN_PRESS;
    if (!wanted) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}

void clean_sweep_enter(bool buttons_available)
{
    if (s_screen) return;
    clean_sweep_prepare();
    memset(&s_state, 0, sizeof(s_state));
    cs_home(&s_state);
    s_buttons = buttons_available;
    s_dimmed = s_off = s_suppress_long = false;
    s_last_frame = s_last_activity = now_ms();
    s_screen = ui_pixel_screen_create("");
    text_at(s_screen, "俄罗斯方块", 5, 15, 151, &clean_sweep_zh_16, 0xFFFFFF);
    s_stage = box(s_screen, 0, 0, 240, 320, UI_SKY);
    lv_obj_set_style_bg_opa(s_stage, LV_OPA_TRANSP, 0);
    s_hint = text_at(s_screen, "", 4, 294, 232, &clean_sweep_zh_12, UI_INK);

    s_soc = bsp_battery_soc();
    render();
    s_timer = lv_timer_create(frame, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen);
    bsp_display_backlight(100);
    xQueueReset(s_queue);
    atomic_store(&s_accept, buttons_available);
}

void clean_sweep_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_well = s_hud = s_next = s_mascot = NULL;
    s_score = s_lines = s_pace = s_combo = NULL;
    s_banner = s_banner_shadow = s_banner_gain = NULL;
    s_battery = s_hint = s_stage = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
