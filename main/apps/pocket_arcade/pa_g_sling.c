/* 弹弓打靶 —— 确定键先停角度条,再停力度条,然后看抛物线。
   有风,靶子每发都换位置,所以打完一发要重新估。 */
#include "pa_game.h"
#include <string.h>

enum {
    GROUND = 140, SLING_X = 16, GRAVITY = 380,
    TARGET_H = 26, SHOTS = 10, HOLD_MS = 1000,
    BAR_X = 12, BAR_W = 174, BAR_Y = 146, BAR_H = 10,
};

static int shot_angle(const pa_sling_t *s) { return 15 + s->angle * 60 / 100; }
static int shot_speed(const pa_sling_t *s) { return 60 + s->power * 190 / 100; }

static void place_target(pa_run_t *run)
{
    pa_sling_t *s = &run->u.sling;
    s->target_w = (int16_t)(30 - (int)s->shot * 2);
    if (s->target_w < 14) s->target_w = 14;
    s->target_x = (int16_t)(84 + (int)pa_below(&run->rng, 92U));
    if (s->target_x + s->target_w > PA_FIELD_W - 4)
        s->target_x = (int16_t)(PA_FIELD_W - 4 - s->target_w);
    s->wind = (int8_t)((int)pa_below(&run->rng, 41U) - 20);
    s->phase = 0;
    s->angle = 0;
    s->angle_dir = 1;
    s->power = 0;
    s->power_dir = 1;
    s->hit = false;
}

static void reset(pa_run_t *run)
{
    pa_sling_t *s = &run->u.sling;
    memset(s, 0, sizeof(*s));
    place_target(run);
}

static void launch(pa_sling_t *s)
{
    int speed = shot_speed(s), angle = shot_angle(s);
    s->x_m = (int32_t)(SLING_X + 6) * 1000;
    s->y_m = (int32_t)(GROUND - 18) * 1000;
    s->vx = (int16_t)(speed * pa_cos(angle) / 1000);
    s->vy = (int16_t)(-speed * pa_sin(angle) / 1000);
    s->phase = 2;
}

static void settle(pa_run_t *run, bool hit)
{
    pa_sling_t *s = &run->u.sling;
    s->hit = hit;
    s->phase = 3;
    s->hold_ms = HOLD_MS;
    if (!hit) return;
    s->hits++;
    int centre = s->target_x + s->target_w / 2;
    int miss = (int)(s->x_m / 1000) - centre;
    if (miss < 0) miss = -miss;
    run->score += 60 + (long)(s->target_w <= 20 ? 30 : 0) + (miss <= 3 ? 25 : 0);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_sling_t *s = &run->u.sling;
    if (s->phase == 0) {
        int angle = s->angle + s->angle_dir * (int)(150 * ms) / 1000;
        if (angle >= 100) { angle = 100; s->angle_dir = -1; }
        if (angle <= 0) { angle = 0; s->angle_dir = 1; }
        s->angle = (int16_t)angle;
        return;
    }
    if (s->phase == 1) {
        int power = s->power + s->power_dir * (int)(170 * ms) / 1000;
        if (power >= 100) { power = 100; s->power_dir = -1; }
        if (power <= 0) { power = 0; s->power_dir = 1; }
        s->power = (int16_t)power;
        return;
    }
    if (s->phase == 2) {
        s->vy = (int16_t)(s->vy + (int)(GRAVITY * ms) / 1000);
        s->vx = (int16_t)(s->vx + (int)(s->wind * (int)ms) / 1000);
        s->x_m += (int32_t)s->vx * (int32_t)ms;
        s->y_m += (int32_t)s->vy * (int32_t)ms;
        int x = (int)(s->x_m / 1000), y = (int)(s->y_m / 1000);
        bool in_target = (x >= s->target_x && x <= s->target_x + s->target_w);
        if (in_target && y >= GROUND - TARGET_H) { settle(run, true); return; }
        if (y >= GROUND) { settle(run, false); return; }
        if (x > PA_FIELD_W + 20 || x < -20 || y < -400) { settle(run, false); return; }
        return;
    }
    s->hold_ms = (uint16_t)(s->hold_ms > ms ? s->hold_ms - ms : 0);
    if (s->hold_ms) return;
    s->shot++;
    if (s->shot >= SHOTS) {
        run->over = true;
        run->note = "十发打完了";
        return;
    }
    place_target(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    (void)pressed;
    pa_sling_t *s = &run->u.sling;
    if (s->phase == 0) { s->phase = 1; return; }
    if (s->phase == 1) { launch(s); return; }
    if (s->phase == 3 && s->hold_ms > 200) s->hold_ms = 200;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_sling_t *s = &run->u.sling;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 发   中 %u 次", (unsigned)(s->shot + 1U), (unsigned)s->hits);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x8FD3F4, 4);
    pa_frect(scene, 0, GROUND, PA_FIELD_W, PA_FIELD_H - GROUND, PA_GRASS, 0);
    pa_frect(scene, 0, GROUND, PA_FIELD_W, 3, PA_GRASSD, 0);

    pa_frect(scene, s->target_x, GROUND - TARGET_H, s->target_w, TARGET_H,
             s->hit && s->phase == 3 ? PA_GREEN : PA_RED, 3);
    pa_frect(scene, s->target_x + s->target_w / 2 - 2, GROUND - TARGET_H / 2 - 2,
             4, 4, PA_WHITE, 2);

    pa_frect(scene, SLING_X, GROUND - 20, 5, 20, 0x8A5A2B, 1);
    pa_frect(scene, SLING_X - 5, GROUND - 26, 15, 6, 0x8A5A2B, 1);

    if (s->phase >= 2)
        pa_frect(scene, (int)(s->x_m / 1000) - 4, (int)(s->y_m / 1000) - 4, 8, 8,
                 PA_INK, 4);

    /* 风向条:向右吹画在右边,越长风越大。 */
    int wind = s->wind;
    pa_frect(scene, 95, 10, 2, 10, PA_SLATE, 0);
    if (wind)
        pa_frect(scene, wind > 0 ? 97 : 95 + wind * 2, 13, (wind > 0 ? wind : -wind) * 2, 4,
                 PA_SLATE, 1);

    if (s->phase <= 1) {
        pa_frect(scene, BAR_X, BAR_Y, BAR_W, BAR_H, PA_PAPER, 2);
        pa_frect(scene, BAR_X, BAR_Y, s->angle * BAR_W / 100, BAR_H,
                 s->phase == 0 ? PA_ORANGE : PA_GRAY, 2);
    }
    if (s->phase == 1) {
        pa_frect(scene, BAR_X, BAR_Y - 14, BAR_W, BAR_H, PA_PAPER, 2);
        pa_frect(scene, BAR_X, BAR_Y - 14, s->power * BAR_W / 100, BAR_H, PA_GREEN, 2);
    }

    const char *line = "确定键停角度条";
    uint32_t colour = PA_INK;
    if (s->phase == 1) line = "再按一下停力度";
    else if (s->phase == 2) { line = "飞出去了"; colour = PA_SLATE; }
    else if (s->phase == 3) {
        line = s->hit ? "正中靶子" : "偏了";
        colour = s->hit ? PA_GREEN : PA_RED;
    }
    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, line, colour, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "确定停角度　再确定停力度");
}

const pa_game_t pa_game_sling = {
    .id = 12,
    .name = "弹弓打靶", .genre = "手感",
    .hint = "先定角度再定力度",
    .rule = {"确定停角度条，再按一下停力度",
             "中间那条短线是风，会把弹丸吹偏",
             "十发一局，靶子越来越窄"},
    .keys = "确定停角度再停力度",
    .ok_mode = PA_OK_PRESS,
    .star = {200, 420, 700},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
