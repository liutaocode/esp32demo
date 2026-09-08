/* 数字华容道 —— 四乘四推板。任何时刻能动的方块只有两到四个,
   所以上下键在这几个之间循环,确定键把它推进空位就够了。 */
#include "pa_game.h"
#include <string.h>

enum { TILE = 34, GAP = 3, BOARD_X = 26, BOARD_Y = 7, BOARDS = 3, CLEAR_MS = 1200 };

static uint8_t blank_of(const pa_slide_t *s)
{
    for (uint8_t i = 0; i < 16; i++)
        if (!s->tile[i]) return i;
    return 0;
}

static void refresh_options(pa_slide_t *s)
{
    uint8_t blank = blank_of(s);
    int bx = blank % 4, by = blank / 4;
    s->count = 0;
    static const int8_t STEP_X[4] = {1, 0, -1, 0};
    static const int8_t STEP_Y[4] = {0, 1, 0, -1};
    for (unsigned d = 0; d < 4; d++) {
        int x = bx + STEP_X[d], y = by + STEP_Y[d];
        if (x < 0 || y < 0 || x >= 4 || y >= 4) continue;
        s->option[s->count++] = (uint8_t)(y * 4 + x);
    }
    if (s->pick >= s->count) s->pick = 0;
}

static bool solved(const pa_slide_t *s)
{
    for (uint8_t i = 0; i < 15; i++)
        if (s->tile[i] != i + 1) return false;
    return s->tile[15] == 0;
}

/* 从复原态倒着随机走,保证摆出来的局面一定能推回去。 */
static void shuffle(pa_run_t *run, unsigned steps)
{
    pa_slide_t *s = &run->u.slide;
    for (uint8_t i = 0; i < 15; i++) s->tile[i] = (uint8_t)(i + 1);
    s->tile[15] = 0;
    for (unsigned i = 0; i < steps; i++) {
        refresh_options(s);
        uint8_t choice = s->option[pa_below(&run->rng, s->count)];
        uint8_t blank = blank_of(s);
        s->tile[blank] = s->tile[choice];
        s->tile[choice] = 0;
    }
    if (solved(s)) shuffle(run, 7);
}

static void new_board(pa_run_t *run)
{
    pa_slide_t *s = &run->u.slide;
    shuffle(run, 240);
    s->moves = 0;
    s->pick = 0;
    refresh_options(s);
}

static void reset(pa_run_t *run)
{
    pa_slide_t *s = &run->u.slide;
    memset(s, 0, sizeof(*s));
    s->board = 1;
    new_board(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_slide_t *s = &run->u.slide;
    if (!s->clear_ms) return;
    s->clear_ms = (uint16_t)(s->clear_ms > ms ? s->clear_ms - ms : 0);
    if (s->clear_ms) return;
    if (s->board >= BOARDS) {
        run->over = true;
        run->note = "三块拼图都复原了";
        return;
    }
    s->board++;
    new_board(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_slide_t *s = &run->u.slide;
    if (s->clear_ms || !s->count) return;
    if (pressed == PA_KEY_UP) {
        s->pick = (uint8_t)((s->pick + s->count - 1U) % s->count);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        s->pick = (uint8_t)((s->pick + 1U) % s->count);
        return;
    }
    uint8_t choice = s->option[s->pick];
    uint8_t blank = blank_of(s);
    s->tile[blank] = s->tile[choice];
    s->tile[choice] = 0;
    s->moves++;
    refresh_options(s);
    if (!solved(s)) return;
    long bonus = 260 - (long)s->moves * 2;
    run->score += 150 + (bonus > 0 ? bonus : 0);
    s->clear_ms = CLEAR_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_slide_t *s = &run->u.slide;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 块   走了 %u 步", (unsigned)s->board, (unsigned)s->moves);
    pa_frect(scene, BOARD_X - 4, BOARD_Y - 4, 4 * TILE + 3 * GAP + 8,
             4 * TILE + 3 * GAP + 8, 0x5A6B7A, 5);
    for (uint8_t i = 0; i < 16; i++) {
        if (!s->tile[i]) continue;
        int x = BOARD_X + (i % 4) * (TILE + GAP);
        int y = BOARD_Y + (i / 4) * (TILE + GAP);
        bool picked = (!s->clear_ms && s->count && s->option[s->pick] == i);
        bool home = (s->tile[i] == i + 1);
        pa_frect(scene, x, y, TILE, TILE,
                 picked ? PA_YELLOW : home ? 0xD9E7D0 : PA_PAPER, 4);
        pa_num(scene, x, PA_FIELD_Y + y + 10, TILE, (long)s->tile[i], PA_INK,
               PA_FONT_NUM14, PA_CENTER);
    }
    if (s->clear_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "复原了", PA_GREEN,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   能动 %u 块", run->score, (unsigned)s->count);
    pa_footer(scene, "上下换方块　确定推进空位");
}

const pa_game_t pa_game_slide = {
    .id = 18,
    .name = "数字华容道", .genre = "益智",
    .hint = "在能动的方块间切换",
    .rule = {"上下键在能动的方块之间切换",
             "确定键把它推进空位",
             "按一到十五排好，一共三块拼图"},
    .keys = "上下换方块　确定推入",
    .ok_mode = PA_OK_PRESS,
    .star = {200, 450, 700},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
