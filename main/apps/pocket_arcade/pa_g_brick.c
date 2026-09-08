/* 打砖块 —— 挡板一次挪一格,球撞挡板边缘会拐得更斜。硬砖要打两下。 */
#include "pa_game.h"
#include <string.h>

enum {
    BRICK_X = 3, BRICK_Y = 10, BRICK_W = 22, BRICK_H = 10, BRICK_GAP = 2,
    PAD_W = 40, PAD_H = 7, PAD_Y = 146, PAD_STEP = 16, BALL = 7,
    SERVE_MS = 800, SPEED_START = 120, SPEED_MAX = 210,
};

static int clamp_pad(int x)
{
    if (x < 0) return 0;
    if (x > PA_FIELD_W - PAD_W) return PA_FIELD_W - PAD_W;
    return x;
}

static unsigned remaining(const pa_brick_t *b)
{
    unsigned count = 0;
    for (unsigned row = 0; row < PA_BRICK_ROWS; row++)
        for (unsigned col = 0; col < PA_BRICK_W; col++)
            count += b->brick[row][col] ? 1U : 0U;
    return count;
}

static void serve(pa_run_t *run)
{
    pa_brick_t *b = &run->u.brick;
    int speed = SPEED_START + (int)b->level * 14;
    if (speed > SPEED_MAX) speed = SPEED_MAX;
    b->bx_m = (int32_t)(b->px + PAD_W / 2 - BALL / 2) * 1000;
    b->by_m = (int32_t)(PAD_Y - BALL - 2) * 1000;
    b->vy = (int16_t)-speed;
    b->vx = (int16_t)((pa_below(&run->rng, 2) ? 1 : -1) * (speed / 2 + 10));
    b->serve_ms = SERVE_MS;
}

static void build_wall(pa_run_t *run)
{
    pa_brick_t *b = &run->u.brick;
    for (unsigned row = 0; row < PA_BRICK_ROWS; row++)
        for (unsigned col = 0; col < PA_BRICK_W; col++)
            b->brick[row][col] = (uint8_t)((b->level >= 2 && row == 0) ? 2 : 1);
    b->px = (int16_t)clamp_pad(PA_FIELD_W / 2 - PAD_W / 2);
    serve(run);
}

static void reset(pa_run_t *run)
{
    pa_brick_t *b = &run->u.brick;
    memset(b, 0, sizeof(*b));
    b->lives = 3;
    b->level = 1;
    build_wall(run);
}

static bool hit_brick(pa_run_t *run, int x, int y)
{
    pa_brick_t *b = &run->u.brick;
    int col = (x - BRICK_X) / (BRICK_W + BRICK_GAP);
    int row = (y - BRICK_Y) / (BRICK_H + BRICK_GAP);
    if (col < 0 || row < 0 || col >= PA_BRICK_W || row >= PA_BRICK_ROWS) return false;
    int left = BRICK_X + col * (BRICK_W + BRICK_GAP);
    int top = BRICK_Y + row * (BRICK_H + BRICK_GAP);
    if (x < left || x >= left + BRICK_W || y < top || y >= top + BRICK_H) return false;
    if (!b->brick[row][col]) return false;
    b->brick[row][col]--;
    run->score += b->brick[row][col] ? 5 : 15;
    return true;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_brick_t *b = &run->u.brick;
    if (b->serve_ms) {
        b->serve_ms = (uint16_t)(b->serve_ms > ms ? b->serve_ms - ms : 0);
        b->bx_m = (int32_t)(b->px + PAD_W / 2 - BALL / 2) * 1000;
        return;
    }
    b->bx_m += (int32_t)b->vx * (int32_t)ms;
    b->by_m += (int32_t)b->vy * (int32_t)ms;
    int x = (int)(b->bx_m / 1000), y = (int)(b->by_m / 1000);

    if (x < 0) { b->bx_m = 0; b->vx = (int16_t)-b->vx; }
    if (x > PA_FIELD_W - BALL) {
        b->bx_m = (int32_t)(PA_FIELD_W - BALL) * 1000;
        b->vx = (int16_t)-b->vx;
    }
    if (y < 0) { b->by_m = 0; b->vy = (int16_t)-b->vy; }
    x = (int)(b->bx_m / 1000);
    y = (int)(b->by_m / 1000);

    /* 用球的四个角去撞砖,斜着穿过缝隙也能判到。 */
    if (hit_brick(run, x + BALL / 2, y) || hit_brick(run, x + BALL / 2, y + BALL))
        b->vy = (int16_t)-b->vy;
    else if (hit_brick(run, x, y + BALL / 2) || hit_brick(run, x + BALL, y + BALL / 2))
        b->vx = (int16_t)-b->vx;

    if (b->vy > 0 && y + BALL >= PAD_Y && y < PAD_Y + PAD_H &&
        x + BALL > b->px && x < b->px + PAD_W) {
        b->by_m = (int32_t)(PAD_Y - BALL) * 1000;
        b->vy = (int16_t)-b->vy;
        int offset = x + BALL / 2 - (b->px + PAD_W / 2);
        b->vx = (int16_t)(b->vx + offset * 5);
        if (b->vx > 200) b->vx = 200;
        if (b->vx < -200) b->vx = -200;
        run->score += 1;
    }

    if (!remaining(b)) {
        run->score += 200 + (long)b->level * 30;
        if (b->level < 200) b->level++;
        build_wall(run);
        return;
    }
    if ((int)(b->by_m / 1000) <= PA_FIELD_H) return;
    if (b->lives) b->lives--;
    if (!b->lives) {
        run->over = true;
        run->note = "球全掉光了";
        return;
    }
    serve(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_brick_t *b = &run->u.brick;
    if (pressed == PA_KEY_UP) b->px = (int16_t)clamp_pad(b->px - PAD_STEP);
    else if (pressed == PA_KEY_DOWN) b->px = (int16_t)clamp_pad(b->px + PAD_STEP);
    else if (b->serve_ms) b->serve_ms = 1;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_brick_t *b = &run->u.brick;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   第 %u 关", run->score, (unsigned)b->level);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x16202B, 4);
    static const uint32_t ROW_COLOUR[PA_BRICK_ROWS] = {PA_PINK, PA_ORANGE, PA_YELLOW, PA_GRASS};
    for (unsigned row = 0; row < PA_BRICK_ROWS; row++)
        for (unsigned col = 0; col < PA_BRICK_W; col++) {
            if (!b->brick[row][col]) continue;
            pa_frect(scene, BRICK_X + (int)col * (BRICK_W + BRICK_GAP),
                     BRICK_Y + (int)row * (BRICK_H + BRICK_GAP), BRICK_W, BRICK_H,
                     b->brick[row][col] > 1 ? PA_GRAY : ROW_COLOUR[row], 2);
        }
    pa_frect(scene, b->px, PAD_Y, PAD_W, PAD_H, PA_CREAM, 3);
    pa_frect(scene, (int)(b->bx_m / 1000), (int)(b->by_m / 1000), BALL, BALL, PA_WHITE, 4);

    if (b->serve_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "确定可以提前发球", PA_GREEN,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
                 b->lives > 1 ? PA_INK : PA_RED, PA_FONT_ZH, PA_CENTER,
                 "剩 %u 条命   还有 %u 块", (unsigned)b->lives, remaining(b));
    pa_footer(scene, "上左移　下右移　确定发球");
}

const pa_game_t pa_game_brick = {
    .id = 8,
    .name = "打砖块", .genre = "反应",
    .hint = "边缘接球拐得更斜",
    .rule = {"上键左移，下键右移，确定发球",
             "球撞挡板边缘会拐得更斜",
             "灰砖要打两下，清完一关球更快"},
    .keys = "上左移　下右移　确定发球",
    .ok_mode = PA_OK_PRESS,
    .star = {400, 1000, 2000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
