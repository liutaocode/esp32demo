/* 弹球对战 —— 你在下,电脑在上,先到七分赢下一局。挡板一次挪一格,
   所以要提前判断落点,而不是跟着球跑。 */
#include "pa_game.h"
#include <string.h>

enum {
    PAD_H = 8, BALL = 8, TOP_Y = 8, BOTTOM_Y = 144,
    WIN_POINTS = 7, SERVE_MS = 900, SPEED_START = 118, SPEED_MAX = 250,
};

static int clamp_pad(int x)
{
    if (x < 0) return 0;
    if (x > PA_FIELD_W - PA_PONG_PAD) return PA_FIELD_W - PA_PONG_PAD;
    return x;
}

static void serve(pa_run_t *run, int direction)
{
    pa_pong_t *p = &run->u.pong;
    p->bx_m = (int32_t)(PA_FIELD_W / 2 - BALL / 2) * 1000;
    p->by_m = (int32_t)(PA_FIELD_H / 2 - BALL / 2) * 1000;
    int speed = SPEED_START + (int)p->round * 16 + (int)p->rally * 2;
    if (speed > SPEED_MAX) speed = SPEED_MAX;
    p->vy = (int16_t)(direction * speed);
    p->vx = (int16_t)((pa_below(&run->rng, 2) ? 1 : -1) * (int)(40 + pa_below(&run->rng, 55)));
    p->serve_ms = SERVE_MS;
    p->serve_dir = (int8_t)direction;
    p->rally = 0;
}

static void reset(pa_run_t *run)
{
    pa_pong_t *p = &run->u.pong;
    memset(p, 0, sizeof(*p));
    p->round = 1;
    p->px = (int16_t)clamp_pad(PA_FIELD_W / 2 - PA_PONG_PAD / 2);
    p->ax = p->px;
    serve(run, 1);
}

static void point(pa_run_t *run, bool mine)
{
    pa_pong_t *p = &run->u.pong;
    if (mine) {
        p->you++;
        run->score += 40;
    } else {
        p->cpu++;
    }
    if (p->you >= WIN_POINTS) {
        run->score += 200;
        p->you = p->cpu = 0;
        if (p->round < 200) p->round++;
        serve(run, 1);
        return;
    }
    if (p->cpu >= WIN_POINTS) {
        run->over = true;
        run->note = "这一局被电脑拿下";
        return;
    }
    serve(run, mine ? -1 : 1);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_pong_t *p = &run->u.pong;
    if (p->serve_ms) {
        p->serve_ms = (uint16_t)(p->serve_ms > ms ? p->serve_ms - ms : 0);
        return;
    }

    /* 电脑挡板向球的落点靠拢,速度有限,所以贴边的球它接不住。 */
    int target = (int)(p->bx_m / 1000) + BALL / 2 - PA_PONG_PAD / 2;
    int reach = (int)((78U + (unsigned)p->round * 9U) * ms) / 1000;
    if (reach < 1) reach = 1;
    if (p->ax < target) p->ax = (int16_t)clamp_pad(p->ax + (target - p->ax > reach ? reach : target - p->ax));
    else if (p->ax > target) p->ax = (int16_t)clamp_pad(p->ax - (p->ax - target > reach ? reach : p->ax - target));

    p->bx_m += (int32_t)p->vx * (int32_t)ms;
    p->by_m += (int32_t)p->vy * (int32_t)ms;
    int bx = (int)(p->bx_m / 1000), by = (int)(p->by_m / 1000);
    if (bx < 0) { p->bx_m = 0; p->vx = (int16_t)-p->vx; }
    if (bx > PA_FIELD_W - BALL) {
        p->bx_m = (int32_t)(PA_FIELD_W - BALL) * 1000;
        p->vx = (int16_t)-p->vx;
    }
    bx = (int)(p->bx_m / 1000);

    if (p->vy > 0 && by + BALL >= BOTTOM_Y && by < BOTTOM_Y + PAD_H) {
        if (bx + BALL > p->px && bx < p->px + PA_PONG_PAD) {
            p->by_m = (int32_t)(BOTTOM_Y - BALL) * 1000;
            p->vy = (int16_t)-p->vy;
            int offset = bx + BALL / 2 - (p->px + PA_PONG_PAD / 2);
            p->vx = (int16_t)(p->vx + offset * 4);
            p->rally++;
            if (p->rally > p->best_rally) p->best_rally = p->rally;
            run->score += 5;
        }
    }
    if (p->vy < 0 && by <= TOP_Y + PAD_H && by + BALL > TOP_Y) {
        if (bx + BALL > p->ax && bx < p->ax + PA_PONG_PAD) {
            p->by_m = (int32_t)(TOP_Y + PAD_H) * 1000;
            p->vy = (int16_t)-p->vy;
            int offset = bx + BALL / 2 - (p->ax + PA_PONG_PAD / 2);
            p->vx = (int16_t)(p->vx + offset * 3);
            p->rally++;
        }
    }
    if (p->vx > 240) p->vx = 240;
    if (p->vx < -240) p->vx = -240;

    by = (int)(p->by_m / 1000);
    if (by > PA_FIELD_H) point(run, false);
    else if (by + BALL < 0) point(run, true);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_pong_t *p = &run->u.pong;
    if (pressed == PA_KEY_UP) p->px = (int16_t)clamp_pad(p->px - PA_PONG_STEP);
    else if (pressed == PA_KEY_DOWN) p->px = (int16_t)clamp_pad(p->px + PA_PONG_STEP);
    else if (p->serve_ms) p->serve_ms = 1;   /* 确定键提前开球 */
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_pong_t *p = &run->u.pong;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "你 %u 比 %u 电脑   第 %u 局", (unsigned)p->you, (unsigned)p->cpu,
             (unsigned)p->round);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x14303F, 4);
    for (int x = 6; x < PA_FIELD_W - 6; x += 22)
        pa_frect(scene, x, PA_FIELD_H / 2 - 1, 12, 2, 0x2F5567, 0);

    pa_frect(scene, p->ax, TOP_Y, PA_PONG_PAD, PAD_H, PA_PINK, 3);
    pa_frect(scene, p->px, BOTTOM_Y, PA_PONG_PAD, PAD_H, PA_YELLOW, 3);
    pa_frect(scene, (int)(p->bx_m / 1000), (int)(p->by_m / 1000), BALL, BALL, PA_WHITE, 4);

    if (p->serve_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "准备发球，确定可以提前",
                PA_GREEN, PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   连打 %u 回合", run->score, (unsigned)p->rally);
    pa_footer(scene, "上左移　下右移　先到七分");
}

const pa_game_t pa_game_pong = {
    .id = 9,
    .name = "弹球对战", .genre = "对战",
    .hint = "先到七分赢一局",
    .rule = {"上键左移，下键右移，一次挪一格",
             "球打在挡板边缘会拐得更斜",
             "先到七分赢下一局，输了就结束"},
    .keys = "上左移　下右移",
    .ok_mode = PA_OK_PRESS,
    .star = {400, 1000, 2000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
