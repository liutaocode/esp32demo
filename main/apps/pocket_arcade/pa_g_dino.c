/* 恐龙快跑 —— 跳过仙人掌,低头钻过飞鸟。跑得越远越快,
   后半段必须靠"跳"和"蹲"两种动作交替才活得下去。 */
#include "pa_game.h"
#include <string.h>

enum {
    DINO_X = 26, DINO_W = 18, DINO_TALL = 24, DINO_DUCK = 13,
    JUMP_VY = -430, GRAVITY = 1500, DUCK_MS = 520,
    SPEED_START = 108, SPEED_MAX = 225, BIRD_SCORE = 140,
};

static const uint8_t OBS_W[4] = {0, 10, 16, 20};
static const uint8_t OBS_H[4] = {0, 20, 28, 12};

static void place_obstacle(pa_run_t *run, unsigned index, int32_t x_m)
{
    pa_dino_t *d = &run->u.dino;
    unsigned roll = pa_below(&run->rng, 100);
    uint8_t kind = (roll < 45) ? 1 : (roll < 80) ? 2 : 3;
    if (kind == 3 && run->score < BIRD_SCORE) kind = 1;
    d->ox_m[index] = x_m;
    d->kind[index] = kind;
}

static int32_t furthest(const pa_dino_t *d)
{
    int32_t best = d->ox_m[0];
    for (unsigned i = 1; i < PA_DINO_OBS; i++)
        if (d->ox_m[i] > best) best = d->ox_m[i];
    return best;
}

static void reset(pa_run_t *run)
{
    pa_dino_t *d = &run->u.dino;
    memset(d, 0, sizeof(*d));
    d->speed = SPEED_START;
    for (unsigned i = 0; i < PA_DINO_OBS; i++)
        place_obstacle(run, i, (int32_t)(PA_FIELD_W + 40 + (int)i * 110) * 1000);
}

static bool overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_dino_t *d = &run->u.dino;
    d->step_ms = (uint16_t)((d->step_ms + ms) % 400U);
    if (d->duck_ms) d->duck_ms = (uint16_t)(d->duck_ms > ms ? d->duck_ms - ms : 0);

    bool airborne = d->y_m < 0;
    if (airborne) {
        int gravity = d->duck_ms ? GRAVITY * 2 : GRAVITY;   /* 空中按蹲就快落 */
        d->vy = (int16_t)(d->vy + (int)(gravity * (int)ms) / 1000);
        d->y_m += (int32_t)d->vy * (int32_t)ms;
        if (d->y_m >= 0) { d->y_m = 0; d->vy = 0; }
    }

    d->dist_m += (int32_t)d->speed * (int32_t)ms;
    run->score = d->dist_m / 8000;
    if (d->speed < SPEED_MAX && (run->score % 40) == 0 && run->score > 0)
        d->speed = (uint16_t)(SPEED_START + run->score / 4);
    if (d->speed > SPEED_MAX) d->speed = SPEED_MAX;

    int dy = PA_DINO_GROUND - (d->duck_ms && !airborne ? DINO_DUCK : DINO_TALL)
             + (int)(d->y_m / 1000);
    int dh = (d->duck_ms && !airborne) ? DINO_DUCK : DINO_TALL;
    int dw = (d->duck_ms && !airborne) ? DINO_W + 6 : DINO_W;

    for (unsigned i = 0; i < PA_DINO_OBS; i++) {
        d->ox_m[i] -= (int32_t)d->speed * (int32_t)ms;
        int ox = (int)(d->ox_m[i] / 1000);
        uint8_t kind = d->kind[i];
        if (ox + OBS_W[kind] < 0) {
            int32_t gap = (int32_t)(78 + (int)pa_below(&run->rng, 62) + (int)d->speed / 3);
            place_obstacle(run, i, furthest(d) + gap * 1000);
            continue;
        }
        int oy = (kind == 3) ? PA_DINO_GROUND - 32 : PA_DINO_GROUND - OBS_H[kind];
        if (overlaps(DINO_X, dy, dw, dh, ox, oy, OBS_W[kind], OBS_H[kind])) {
            run->over = true;
            run->note = (kind == 3) ? "被飞鸟撞到了" : "撞上了仙人掌";
            return;
        }
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_dino_t *d = &run->u.dino;
    if (pressed == PA_KEY_DOWN) { d->duck_ms = DUCK_MS; return; }
    if (d->y_m < 0) return;      /* 空中不能二段跳 */
    d->vy = JUMP_VY;
    d->y_m = -1;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_dino_t *d = &run->u.dino;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "跑了 %ld 米", run->score);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, PA_CREAM, 4);
    pa_frect(scene, 0, PA_DINO_GROUND, PA_FIELD_W, 3, PA_INK, 0);
    for (int x = -(int)((d->dist_m / 1000) % 40); x < PA_FIELD_W; x += 40)
        pa_frect(scene, x, PA_DINO_GROUND + 8, 12, 2, PA_GRAY, 0);

    bool airborne = d->y_m < 0;
    bool ducking = d->duck_ms && !airborne;
    int dh = ducking ? DINO_DUCK : DINO_TALL;
    int dw = ducking ? DINO_W + 6 : DINO_W;
    int dy = PA_DINO_GROUND - dh + (int)(d->y_m / 1000);
    pa_frect(scene, DINO_X, dy, dw, dh, PA_GREEN, 3);
    pa_frect(scene, DINO_X + dw - 6, dy + 3, 3, 3, PA_WHITE, 0);
    if (!airborne && d->step_ms < 200) pa_frect(scene, DINO_X + 2, dy + dh, 5, 4, PA_GREEN, 0);
    else if (!airborne) pa_frect(scene, DINO_X + dw - 8, dy + dh, 5, 4, PA_GREEN, 0);

    for (unsigned i = 0; i < PA_DINO_OBS; i++) {
        uint8_t kind = d->kind[i];
        int ox = (int)(d->ox_m[i] / 1000);
        int oy = (kind == 3) ? PA_DINO_GROUND - 32 : PA_DINO_GROUND - OBS_H[kind];
        pa_frect(scene, ox, oy, OBS_W[kind], OBS_H[kind],
                 kind == 3 ? PA_SLATE : PA_GRASSD, kind == 3 ? 5 : 2);
        if (kind == 3) pa_frect(scene, ox + 4, oy - 4, 12, 4, PA_SLATE, 2);
    }

    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
            run->score >= BIRD_SCORE ? "飞鸟来了，记得低头" : "跳起来再按下键会快落",
            PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "上跳　下低头　确定也跳");
}

const pa_game_t pa_game_dino = {
    .id = 4,
    .name = "恐龙快跑", .genre = "反应",
    .hint = "跳仙人掌钻飞鸟",
    .rule = {"上键起跳，下键低头，确定也是跳",
             "跑到一百四十米以后开始出现飞鸟",
             "空中按下键会快落，用来抢节奏"},
    .keys = "上跳　下低头　确定跳",
    .ok_mode = PA_OK_PRESS,
    .star = {250, 600, 1200},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
