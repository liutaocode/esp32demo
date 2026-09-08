/* 飞天小鸟 —— 一个键的经典躲柱子。位置用毫像素定点,
   所以速度写成"像素/秒"时,乘毫秒正好是位移。 */
#include "pa_game.h"
#include <string.h>

enum {
    BIRD_X = 44, BIRD_W = 14, BIRD_H = 12,
    GROUND_Y = 148, PIPE_W = 22, PIPE_GAP_MIN = 18,
    SPACING = 92, GRAVITY = 1050, FLAP_VY = -240,
    SPEED_START = 56, SPEED_MAX = 100, FALL_MAX = 350,
    BOB_MS = 1400, BOB_RANGE = 5,
};

static void place_pipe(pa_run_t *run, unsigned index, int32_t x_m)
{
    pa_bird_t *b = &run->u.bird;
    unsigned span = (unsigned)(GROUND_Y - PA_BIRD_GAP - 2 * PIPE_GAP_MIN);
    b->pipe_x_m[index] = x_m;
    b->gap_y[index] = (int16_t)(PIPE_GAP_MIN + pa_below(&run->rng, span));
    b->passed[index] = 0;
}

static void reset(pa_run_t *run)
{
    pa_bird_t *b = &run->u.bird;
    memset(b, 0, sizeof(*b));
    b->y_m = (GROUND_Y / 2 - BIRD_H / 2) * 1000;
    b->speed = SPEED_START;
    for (unsigned i = 0; i < PA_BIRD_PIPES; i++)
        place_pipe(run, i, (int32_t)(PA_FIELD_W + 60 + (int)i * SPACING) * 1000);
}

static bool overlaps(int ax, int aw, int bx, int bw)
{
    return ax < bx + bw && bx < ax + aw;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_bird_t *b = &run->u.bird;
    if (b->flap_ms) b->flap_ms = (uint16_t)(b->flap_ms > ms ? b->flap_ms - ms : 0);

    /* 起飞之前只是原地上下浮着:重力和柱子都不动,也撞不死。开局那 300 毫秒
       的按键屏蔽窗口比这只鸟落地还长,没有这一段就成了"一进去必死"。 */
    if (!b->flying) {
        b->bob_ms = (uint16_t)((b->bob_ms + ms) % BOB_MS);
        int phase = (int)b->bob_ms * 360 / BOB_MS;
        b->y_m = (int32_t)(GROUND_Y / 2 - BIRD_H / 2) * 1000 +
                 pa_sin(phase) * BOB_RANGE;
        return;
    }

    b->vy = (int16_t)(b->vy + (int)(GRAVITY * ms) / 1000);
    if (b->vy > FALL_MAX) b->vy = FALL_MAX;
    b->y_m += (int32_t)b->vy * (int32_t)ms;
    if (b->y_m < 0) { b->y_m = 0; b->vy = 0; }

    int y = (int)(b->y_m / 1000);
    if (y + BIRD_H >= GROUND_Y) {
        run->over = true;
        run->note = "掉到地上了";
        return;
    }

    for (unsigned i = 0; i < PA_BIRD_PIPES; i++) {
        b->pipe_x_m[i] -= (int32_t)b->speed * (int32_t)ms;
        int px = (int)(b->pipe_x_m[i] / 1000);
        if (px + PIPE_W < 0) {
            int32_t furthest = b->pipe_x_m[0];
            for (unsigned j = 1; j < PA_BIRD_PIPES; j++)
                if (b->pipe_x_m[j] > furthest) furthest = b->pipe_x_m[j];
            place_pipe(run, i, furthest + SPACING * 1000);
            continue;
        }
        if (!b->passed[i] && px + PIPE_W < BIRD_X) {
            b->passed[i] = 1;
            run->score += 1;
            if (b->speed < SPEED_MAX) b->speed += 2;
        }
        if (!overlaps(BIRD_X, BIRD_W, px, PIPE_W)) continue;
        if (y < b->gap_y[i] || y + BIRD_H > b->gap_y[i] + PA_BIRD_GAP) {
            run->over = true;
            run->note = "撞上了柱子";
            return;
        }
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    (void)pressed;   /* 三个键都拍翅膀,谁也不会按错 */
    pa_bird_t *b = &run->u.bird;
    b->flying = true;
    b->vy = FLAP_VY;
    b->flap_ms = 180;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_bird_t *b = &run->u.bird;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "穿过 %ld 根柱子", run->score);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x74C7F0, 4);
    pa_frect(scene, 0, GROUND_Y, PA_FIELD_W, PA_FIELD_H - GROUND_Y, PA_SAND, 0);
    pa_frect(scene, 0, GROUND_Y, PA_FIELD_W, 3, PA_GRASSD, 0);

    for (unsigned i = 0; i < PA_BIRD_PIPES; i++) {
        int px = (int)(b->pipe_x_m[i] / 1000);
        int gap = b->gap_y[i];
        pa_frect(scene, px, 0, PIPE_W, gap, PA_GRASS, 2);
        pa_frect(scene, px - 2, gap - 8, PIPE_W + 4, 8, PA_GRASSD, 2);
        pa_frect(scene, px, gap + PA_BIRD_GAP, PIPE_W, GROUND_Y - gap - PA_BIRD_GAP,
                 PA_GRASS, 2);
        pa_frect(scene, px - 2, gap + PA_BIRD_GAP, PIPE_W + 4, 8, PA_GRASSD, 2);
    }

    int y = (int)(b->y_m / 1000);
    pa_frect(scene, BIRD_X, y, BIRD_W, BIRD_H, PA_YELLOW, 4);
    pa_frect(scene, BIRD_X + 9, y + 3, 3, 3, PA_INK, 0);
    pa_frect(scene, BIRD_X + BIRD_W - 1, y + 6, 5, 3, PA_ORANGE, 0);
    pa_frect(scene, BIRD_X + 2, b->flap_ms ? y + 2 : y + 7, 7, 4, PA_ORANGE, 2);

    if (!b->flying)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "按任意键起飞",
                PA_GREEN, PA_FONT_ZH, PA_CENTER);
    else
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "轻点两下比按住更稳",
                PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "任意键拍一下翅膀");
}

const pa_game_t pa_game_bird = {
    .id = 1,
    .name = "飞天小鸟", .genre = "反应",
    .hint = "一个键穿过柱子",
    .rule = {"进去先浮着，按任意键才起飞",
             "三个键都是拍翅膀，从缝里穿过去",
             "每穿一根柱子，速度就快一点"},
    .keys = "任意键拍翅膀",
    .ok_mode = PA_OK_PRESS,
    .star = {8, 20, 40},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
