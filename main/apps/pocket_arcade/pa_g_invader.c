/* 太空守卫 —— 五个停机位对五列外星人。上下键换位置,确定键开火,
   一次只有一发子弹在天上,所以每一枪都得挑好列。 */
#include "pa_game.h"
#include <string.h>

enum {
    COL_STEP = 36, ALIEN_W = 24, ALIEN_H = 15, ROW_STEP = 22,
    FLEET_LEFT = 9, SHIP_X = 14, SHIP_W = 26, SHIP_Y = 136, SHIP_H = 16,
    SHOT_SPEED = 330, MARCH_MS = 240, SWAY = 8,
};

static int alien_x(const pa_invader_t *v, unsigned col)
{
    return v->fleet_x + FLEET_LEFT + (int)col * COL_STEP;
}

static int alien_y(const pa_invader_t *v, unsigned row)
{
    return v->fleet_y + (int)row * ROW_STEP;
}

static int column_centre(const pa_invader_t *v, unsigned col)
{
    return alien_x(v, col) + ALIEN_W / 2;
}

static bool column_alive(const pa_invader_t *v, unsigned col)
{
    for (unsigned row = 0; row < PA_INV_ROWS; row++)
        if (v->alive[row][col]) return true;
    return false;
}

static unsigned remaining(const pa_invader_t *v)
{
    unsigned count = 0;
    for (unsigned row = 0; row < PA_INV_ROWS; row++)
        for (unsigned col = 0; col < PA_INV_COLS; col++)
            count += v->alive[row][col] ? 1U : 0U;
    return count;
}

static void start_wave(pa_run_t *run)
{
    pa_invader_t *v = &run->u.invader;
    memset(v->alive, 1, sizeof(v->alive));
    v->fleet_x = 0;
    v->fleet_y = 6;
    v->fleet_dir = 1;
    v->shot_col = -1;
    v->march_ms = 0;
    v->drop_ms = 0;
    v->bomb_ms = 0;
    v->bomb_gap_ms = (uint16_t)(1500 - (v->wave > 6 ? 6 : v->wave) * 150);
    for (unsigned i = 0; i < PA_INV_BOMBS; i++) v->bomb_col[i] = -1;
}

static void reset(pa_run_t *run)
{
    pa_invader_t *v = &run->u.invader;
    memset(v, 0, sizeof(*v));
    v->lives = 3;
    v->ship_col = 2;
    v->wave = 1;
    start_wave(run);
}

static void ship_hit(pa_run_t *run, const char *reason)
{
    pa_invader_t *v = &run->u.invader;
    v->hit_ms = 600;
    if (v->lives) v->lives--;
    if (!v->lives) {
        run->over = true;
        run->note = reason;
    }
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_invader_t *v = &run->u.invader;
    if (v->hit_ms) v->hit_ms = (uint16_t)(v->hit_ms > ms ? v->hit_ms - ms : 0);

    /* 舰队左右晃动,并定期整体下压一格。 */
    v->march_ms += ms;
    unsigned march_gap = (unsigned)(MARCH_MS - (v->wave > 5 ? 5 : v->wave) * 22U);
    while (v->march_ms >= march_gap) {
        v->march_ms -= march_gap;
        v->fleet_x = (int16_t)(v->fleet_x + v->fleet_dir * 2);
        if (v->fleet_x >= SWAY || v->fleet_x <= -SWAY) v->fleet_dir = (int8_t)-v->fleet_dir;
    }
    v->drop_ms += ms;
    unsigned drop_gap = (unsigned)(3400 - (v->wave > 8 ? 8 : v->wave) * 260U);
    if (v->drop_ms >= drop_gap) {
        v->drop_ms -= drop_gap;
        v->fleet_y = (int16_t)(v->fleet_y + 6);
    }
    if (alien_y(v, PA_INV_ROWS - 1) + ALIEN_H >= SHIP_Y) {
        run->over = true;
        run->note = "外星人压到了地面";
        return;
    }

    if (v->shot_col >= 0) {
        v->shot_y_m -= (int32_t)SHOT_SPEED * (int32_t)ms;
        int y = (int)(v->shot_y_m / 1000);
        if (y < 0) {
            v->shot_col = -1;
        } else {
            for (int row = PA_INV_ROWS - 1; row >= 0; row--) {
                if (!v->alive[row][v->shot_col]) continue;
                if (y > alien_y(v, (unsigned)row) + ALIEN_H) break;
                v->alive[row][v->shot_col] = 0;
                run->score += 10 + (PA_INV_ROWS - 1 - row) * 5;
                v->shot_col = -1;
                break;
            }
        }
    }

    v->bomb_ms += ms;
    if (v->bomb_ms >= v->bomb_gap_ms) {
        v->bomb_ms -= v->bomb_gap_ms;
        for (unsigned i = 0; i < PA_INV_BOMBS; i++) {
            if (v->bomb_col[i] >= 0) continue;
            unsigned col = pa_below(&run->rng, PA_INV_COLS);
            for (unsigned attempt = 0; attempt < PA_INV_COLS; attempt++) {
                if (column_alive(v, col)) break;
                col = (col + 1U) % PA_INV_COLS;
            }
            if (!column_alive(v, col)) break;
            v->bomb_col[i] = (int8_t)col;
            v->bomb_y_m[i] = (int32_t)(alien_y(v, PA_INV_ROWS - 1) + ALIEN_H) * 1000;
            break;
        }
    }
    int bomb_speed = 96 + (int)v->wave * 12;
    for (unsigned i = 0; i < PA_INV_BOMBS && !run->over; i++) {
        if (v->bomb_col[i] < 0) continue;
        v->bomb_y_m[i] += (int32_t)bomb_speed * (int32_t)ms;
        int y = (int)(v->bomb_y_m[i] / 1000);
        if (y < SHIP_Y) continue;
        bool caught = (v->bomb_col[i] == (int8_t)v->ship_col);
        v->bomb_col[i] = -1;
        if (caught) ship_hit(run, "护盾被打穿了");
    }

    if (!remaining(v)) {
        run->score += 200;
        if (v->wave < 250) v->wave++;
        start_wave(run);
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_invader_t *v = &run->u.invader;
    if (pressed == PA_KEY_UP) {
        if (v->ship_col) v->ship_col--;
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        if (v->ship_col + 1U < PA_INV_COLS) v->ship_col++;
        return;
    }
    if (v->shot_col >= 0) return;      /* 天上只允许有一发 */
    v->shot_col = (int8_t)v->ship_col;
    v->shot_y_m = (int32_t)SHIP_Y * 1000;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_invader_t *v = &run->u.invader;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   第 %u 波", run->score, (unsigned)v->wave);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x101A2B, 4);
    for (int x = 12; x < PA_FIELD_W; x += 47)
        pa_frect(scene, x, 12 + (x % 3) * 9, 2, 2, 0x4C5C77, 0);

    for (unsigned row = 0; row < PA_INV_ROWS; row++)
        for (unsigned col = 0; col < PA_INV_COLS; col++) {
            if (!v->alive[row][col]) continue;
            int x = alien_x(v, col), y = alien_y(v, row);
            uint32_t body = (row == 0) ? PA_PINK : (row == 1) ? PA_PURPLE : 0x3FC7E0;
            pa_frect(scene, x, y, ALIEN_W, ALIEN_H, body, 4);
            pa_frect(scene, x + 5, y + 4, 4, 4, PA_INK, 0);
            pa_frect(scene, x + ALIEN_W - 9, y + 4, 4, 4, PA_INK, 0);
        }

    if (v->shot_col >= 0)
        pa_frect(scene, column_centre(v, (unsigned)v->shot_col) - 1,
                 (int)(v->shot_y_m / 1000), 3, 9, PA_YELLOW, 1);
    for (unsigned i = 0; i < PA_INV_BOMBS; i++)
        if (v->bomb_col[i] >= 0)
            pa_frect(scene, column_centre(v, (unsigned)v->bomb_col[i]) - 2,
                     (int)(v->bomb_y_m[i] / 1000), 5, 9, PA_RED, 2);

    int sx = SHIP_X + (int)v->ship_col * COL_STEP;
    uint32_t hull = v->hit_ms ? PA_RED : PA_GRASS;
    pa_frect(scene, sx, SHIP_Y + 6, SHIP_W, SHIP_H - 6, hull, 3);
    pa_frect(scene, sx + SHIP_W / 2 - 3, SHIP_Y, 6, 8, hull, 2);
    pa_frect(scene, 0, PA_FIELD_H - 4, PA_FIELD_W, 4, 0x2C3E57, 0);

    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
             v->lives > 1 ? PA_INK : PA_RED, PA_FONT_ZH, PA_CENTER,
             "护盾 %u   剩 %u 个", (unsigned)v->lives, remaining(v));
    pa_footer(scene, "上左移　下右移　确定开火");
}

const pa_game_t pa_game_invader = {
    .id = 10,
    .name = "太空守卫", .genre = "射击",
    .hint = "五个停机位五列",
    .rule = {"上键左移，下键右移，确定开火",
             "天上一次只有一发子弹，先挑列",
             "外星人压到地面就结束，护盾三层"},
    .keys = "上左移　下右移　确定开火",
    .ok_mode = PA_OK_PRESS,
    .star = {600, 1500, 3000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
