/* 蛇行小道 —— 三键版贪吃蛇:两个键左右转向,确定键抢一步。 */
#include "pa_game.h"
#include <string.h>

enum { CELL = 11, GRID_TOP = 3, STEP_START = 250, STEP_MIN = 95, GOLD_MS = 6000 };

static const int8_t STEP_X[4] = {1, 0, -1, 0};
static const int8_t STEP_Y[4] = {0, 1, 0, -1};

static bool occupied(const pa_snake_t *s, uint8_t x, uint8_t y, uint16_t upto)
{
    for (uint16_t i = 0; i < upto; i++)
        if (s->x[i] == x && s->y[i] == y) return true;
    return false;
}

/* 在空格里放一颗食物。每第四颗是金色的,限时六秒,过期就掉回普通分。 */
static void place_food(pa_run_t *run)
{
    pa_snake_t *s = &run->u.snake;
    unsigned free_cells = (unsigned)(PA_SNAKE_COLS * PA_SNAKE_ROWS) - s->length;
    unsigned pick = pa_below(&run->rng, free_cells ? free_cells : 1U);
    for (uint8_t y = 0; y < PA_SNAKE_ROWS; y++) {
        for (uint8_t x = 0; x < PA_SNAKE_COLS; x++) {
            if (occupied(s, x, y, s->length)) continue;
            if (pick-- == 0) { s->food_x = x; s->food_y = y; goto placed; }
        }
    }
placed:
    s->food_gold = (uint8_t)(((s->eaten + 1U) % 4U) == 0U);
    s->gold_ms = s->food_gold ? GOLD_MS : 0;
}

static void reset(pa_run_t *run)
{
    pa_snake_t *s = &run->u.snake;
    memset(s, 0, sizeof(*s));
    s->length = 4;
    s->dir = 0;
    for (uint16_t i = 0; i < s->length; i++) {
        s->x[i] = (uint8_t)(6 - i);
        s->y[i] = PA_SNAKE_ROWS / 2;
    }
    s->step_ms = STEP_START;
    place_food(run);
}

static void step(pa_run_t *run)
{
    pa_snake_t *s = &run->u.snake;
    int nx = s->x[0] + STEP_X[s->dir];
    int ny = s->y[0] + STEP_Y[s->dir];
    if (nx < 0 || ny < 0 || nx >= PA_SNAKE_COLS || ny >= PA_SNAKE_ROWS) {
        run->over = true;
        run->note = "撞到了边墙";
        return;
    }
    s->turned = 0;
    bool eating = (nx == s->food_x && ny == s->food_y);
    /* 不吃食物时尾巴会让位,所以最后一节不算撞。 */
    uint16_t body = eating ? s->length : (uint16_t)(s->length - 1);
    if (occupied(s, (uint8_t)nx, (uint8_t)ny, body)) {
        run->over = true;
        run->note = "咬到了自己";
        return;
    }
    if (eating && s->length < PA_SNAKE_MAX) s->length++;
    for (uint16_t i = s->length - 1; i > 0; i--) {
        s->x[i] = s->x[i - 1];
        s->y[i] = s->y[i - 1];
    }
    s->x[0] = (uint8_t)nx;
    s->y[0] = (uint8_t)ny;
    if (!eating) return;

    run->score += s->food_gold ? 30 : 10;
    s->eaten++;
    if (s->step_ms > STEP_MIN + 3) s->step_ms -= 3;
    place_food(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_snake_t *s = &run->u.snake;
    if (s->gold_ms) {
        s->gold_ms = (uint16_t)(s->gold_ms > ms ? s->gold_ms - ms : 0);
        if (!s->gold_ms) s->food_gold = 0;
    }
    s->acc_ms += ms;
    while (!run->over && s->acc_ms >= s->step_ms) {
        s->acc_ms -= s->step_ms;
        step(run);
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_snake_t *s = &run->u.snake;
    if (pressed == PA_KEY_OK) {           /* 抢一步:立刻走,不等节拍 */
        s->acc_ms = s->step_ms;
        return;
    }
    /* 一拍只接受一次转向:否则左右各按一下就等于原地掉头,直接撞死自己。 */
    if (s->turned) return;
    s->turned = 1;
    s->dir = (uint8_t)((pressed == PA_KEY_UP) ? (s->dir + 3U) % 4U : (s->dir + 1U) % 4U);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_snake_t *s = &run->u.snake;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   身长 %u", run->score, (unsigned)s->length);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, PA_SLATE, 4);
    for (int y = 0; y < PA_SNAKE_ROWS; y += 3)
        pa_frect(scene, 0, GRID_TOP + y * CELL, PA_FIELD_W, 1, 0x3B4A59, 0);

    pa_frect(scene, s->food_x * CELL + 1, GRID_TOP + s->food_y * CELL + 1, 9, 9,
             s->food_gold ? PA_YELLOW : PA_RED, 4);
    for (int i = (int)s->length - 1; i >= 0; i--)
        pa_frect(scene, s->x[i] * CELL + 1, GRID_TOP + s->y[i] * CELL + 1, 9, 9,
                 i == 0 ? PA_CREAM : PA_GREEN, i == 0 ? 3 : 2);

    if (s->food_gold)
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_ORANGE, PA_FONT_ZH, PA_CENTER,
                 "金果还剩 %u 秒", (unsigned)((s->gold_ms + 999U) / 1000U));
    else
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "吃得越多走得越快",
                PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "上左转　下右转　确定抢一步");
}

const pa_game_t pa_game_snake = {
    .id = 0,
    .name = "蛇行小道", .genre = "经典",
    .hint = "转向吃果越吃越快",
    .rule = {"上键向左拐，下键向右拐",
             "确定键立刻走一步，用来抢时间",
             "金色果子限时六秒，三十分"},
    .keys = "上左转　下右转　确定抢步",
    .ok_mode = PA_OK_PRESS,
    .star = {120, 260, 450},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
