/* 青蛙过河 —— 上下键左右挪,确定键往前跳一行。车流一层一个方向、一个速度,
   过一次河就整体加速,所以后面全靠等空档。 */
#include "pa_game.h"
#include <string.h>

enum {
    ROW_H = 17, ROWS = 9, FROG = 15, SIDE_STEP = 18,
    SLOT = 62, PERIOD = SLOT * 4, CAR_W = 34, CAR_H = 14,
    SAFE_MS = 900, HIT_MS = 700,
};

static int row_y(unsigned row) { return PA_FIELD_H - 18 - (int)row * ROW_H; }

static void build_lanes(pa_run_t *run)
{
    pa_frog_t *f = &run->u.frog;
    int base = 46 + (int)f->round * 9;
    for (unsigned lane = 0; lane < PA_FROG_LANES; lane++) {
        int speed = base + (int)pa_below(&run->rng, 26U + f->round * 3U);
        f->speed[lane] = (int16_t)((lane % 2) ? -speed : speed);
        f->offset_m[lane] = (int32_t)pa_below(&run->rng, PERIOD) * 1000;
        /* 至少留一个空槽,否则这一层根本过不去。 */
        uint8_t pattern = (uint8_t)(pa_below(&run->rng, 15U) + 1U);
        if (pattern == 15) pattern = 7;
        f->pattern[lane] = pattern;
    }
}

static void respawn(pa_run_t *run)
{
    pa_frog_t *f = &run->u.frog;
    f->row = 0;
    f->fx = (int16_t)(PA_FIELD_W / 2 - FROG / 2);
    f->safe_ms = SAFE_MS;
}

static void reset(pa_run_t *run)
{
    pa_frog_t *f = &run->u.frog;
    memset(f, 0, sizeof(*f));
    f->lives = 3;
    f->round = 1;
    build_lanes(run);
    respawn(run);
}

static int car_x(const pa_frog_t *f, unsigned lane, unsigned slot)
{
    int scroll = (int)((f->offset_m[lane] / 1000) % PERIOD);
    if (scroll < 0) scroll += PERIOD;
    return scroll + (int)slot * SLOT - SLOT;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_frog_t *f = &run->u.frog;
    if (f->hit_ms) f->hit_ms = (uint16_t)(f->hit_ms > ms ? f->hit_ms - ms : 0);
    if (f->safe_ms) f->safe_ms = (uint16_t)(f->safe_ms > ms ? f->safe_ms - ms : 0);
    for (unsigned lane = 0; lane < PA_FROG_LANES; lane++) {
        f->offset_m[lane] += (int32_t)f->speed[lane] * (int32_t)ms;
        f->offset_m[lane] %= (int32_t)PERIOD * 1000;
    }
    if (f->safe_ms || f->row == 0 || f->row >= ROWS - 1) return;

    /* 青蛙所在的行就是它所在的车道,竖直方向不用再判一次。 */
    unsigned lane = f->row - 1U;
    for (unsigned slot = 0; slot < 4; slot++) {
        if (!(f->pattern[lane] & (1U << slot))) continue;
        int x = car_x(f, lane, slot);
        if (f->fx >= x + CAR_W || x >= f->fx + FROG) continue;
        if (f->lives) f->lives--;
        f->hit_ms = HIT_MS;
        if (!f->lives) {
            run->over = true;
            run->note = "被车撞到了";
            return;
        }
        respawn(run);
        return;
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_frog_t *f = &run->u.frog;
    if (pressed == PA_KEY_UP) {
        f->fx = (int16_t)(f->fx - SIDE_STEP);
        if (f->fx < 0) f->fx = 0;
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        f->fx = (int16_t)(f->fx + SIDE_STEP);
        if (f->fx > PA_FIELD_W - FROG) f->fx = PA_FIELD_W - FROG;
        return;
    }
    if (f->row + 1U < ROWS) {
        f->row++;
        f->safe_ms = 0;
        return;
    }
    run->score += 100 + (long)f->round * 25;
    f->round++;
    build_lanes(run);
    respawn(run);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_frog_t *f = &run->u.frog;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   第 %u 趟", run->score, (unsigned)f->round);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x2B3947, 4);
    pa_frect(scene, 0, row_y(0), PA_FIELD_W, 18, 0x3E5B34, 0);
    pa_frect(scene, 0, row_y(ROWS - 1), PA_FIELD_W, ROW_H, 0x2E7D5B, 0);

    for (unsigned lane = 0; lane < PA_FROG_LANES; lane++) {
        int y = row_y(lane + 1U);
        pa_frect(scene, 0, y + ROW_H - 1, PA_FIELD_W, 1, 0x4A5A6B, 0);
        for (unsigned slot = 0; slot < 4; slot++) {
            if (!(f->pattern[lane] & (1U << slot))) continue;
            int x = car_x(f, lane, slot);
            if (x + CAR_W <= 0 || x >= PA_FIELD_W) continue;
            uint32_t colour = (lane % 3 == 0) ? PA_RED : (lane % 3 == 1) ? PA_ORANGE : PA_PINK;
            pa_frect(scene, x, y + 2, CAR_W, CAR_H, colour, 3);
            pa_frect(scene, x + 5, y + 4, 8, 5, 0xCDE7F5, 1);
        }
    }

    uint32_t body = f->hit_ms ? PA_RED : (f->safe_ms ? 0xB6E86B : PA_GRASS);
    pa_frect(scene, f->fx, row_y(f->row) + 1, FROG, FROG, body, 4);
    pa_frect(scene, f->fx + 3, row_y(f->row) + 4, 3, 3, PA_INK, 0);
    pa_frect(scene, f->fx + FROG - 6, row_y(f->row) + 4, 3, 3, PA_INK, 0);

    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
             f->lives > 1 ? PA_INK : PA_RED, PA_FONT_ZH, PA_CENTER,
             "还剩 %u 条命   到顶就过河", (unsigned)f->lives);
    pa_footer(scene, "上左移　下右移　确定前跳");
}

const pa_game_t pa_game_frog = {
    .id = 7,
    .name = "青蛙过河", .genre = "反应",
    .hint = "等空档再往前跳",
    .rule = {"上键左移，下键右移，确定往前跳",
             "跳到最上面那条绿带就算过河",
             "过一次河车流就整体加速，三条命"},
    .keys = "上左移　下右移　确定前跳",
    .ok_mode = PA_OK_PRESS,
    .star = {300, 800, 1600},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
