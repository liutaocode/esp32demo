/* 点灯 —— 五乘五,按一格会连它上下左右一起翻面。局面是从全灭倒着乱按出来的,
   所以每一关都保证有解,只是步数有限。 */
#include "pa_game.h"
#include <string.h>

enum { CELL = 28, BOARD_X = 29, BOARD_Y = 8, CLEAR_MS = 1100, SLACK = 6 };

static void toggle(pa_lights_t *l, int x, int y)
{
    if (x < 0 || y < 0 || x >= 5 || y >= 5) return;
    l->cell[y] ^= (uint8_t)(1U << x);
}

static void press_cell(pa_lights_t *l, int x, int y)
{
    toggle(l, x, y);
    toggle(l, x - 1, y);
    toggle(l, x + 1, y);
    toggle(l, x, y - 1);
    toggle(l, x, y + 1);
}

static bool dark(const pa_lights_t *l)
{
    for (unsigned y = 0; y < 5; y++)
        if (l->cell[y]) return false;
    return true;
}

static void new_board(pa_run_t *run)
{
    pa_lights_t *l = &run->u.lights;
    unsigned scramble = 3U + l->level;
    if (scramble > 12U) scramble = 12U;
    do {
        memset(l->cell, 0, sizeof(l->cell));
        for (unsigned i = 0; i < scramble; i++)
            press_cell(l, (int)pa_below(&run->rng, 5), (int)pa_below(&run->rng, 5));
    } while (dark(l));
    l->cx = l->cy = 2;
    l->moves = 0;
    l->budget = (uint16_t)(scramble + SLACK);
}

static void reset(pa_run_t *run)
{
    pa_lights_t *l = &run->u.lights;
    memset(l, 0, sizeof(*l));
    l->level = 1;
    new_board(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_lights_t *l = &run->u.lights;
    if (!l->clear_ms) return;
    l->clear_ms = (uint16_t)(l->clear_ms > ms ? l->clear_ms - ms : 0);
    if (l->clear_ms) return;
    l->level++;
    new_board(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_lights_t *l = &run->u.lights;
    if (l->clear_ms) return;
    if (pressed == PA_KEY_UP) { l->cy = (uint8_t)((l->cy + 1U) % 5U); return; }
    if (pressed == PA_KEY_DOWN) { l->cx = (uint8_t)((l->cx + 1U) % 5U); return; }
    press_cell(l, l->cx, l->cy);
    l->moves++;
    if (dark(l)) {
        long spare = (long)l->budget - (long)l->moves;
        run->score += 120 + (long)l->level * 20 + (spare > 0 ? spare * 15 : 0);
        l->clear_ms = CLEAR_MS;
        return;
    }
    if (l->moves < l->budget) return;
    run->over = true;
    run->note = "步数用完了";
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_lights_t *l = &run->u.lights;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 关   还剩 %u 步", (unsigned)l->level,
             (unsigned)(l->budget > l->moves ? l->budget - l->moves : 0));
    pa_frect(scene, BOARD_X - 4, BOARD_Y - 4, 5 * CELL + 8, 5 * CELL + 8, 0x2B3947, 5);
    for (unsigned y = 0; y < 5; y++)
        for (unsigned x = 0; x < 5; x++) {
            bool on = (l->cell[y] >> x) & 1U;
            bool here = (!l->clear_ms && x == l->cx && y == l->cy);
            int cx = BOARD_X + (int)x * CELL, cy = BOARD_Y + (int)y * CELL;
            pa_frect(scene, cx + 2, cy + 2, CELL - 4, CELL - 4,
                     on ? PA_YELLOW : 0x40546B, 4);
            if (here) pa_frect(scene, cx + 9, cy + 9, 6, 6, on ? PA_INK : PA_WHITE, 3);
        }
    if (l->clear_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "全灭了", PA_GREEN,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   按一格连翻五格", run->score);
    pa_footer(scene, "上换行　下换列　确定按下");
}

const pa_game_t pa_game_lights = {
    .id = 20,
    .name = "点灯", .genre = "益智",
    .hint = "按一格连翻五格",
    .rule = {"上键换行，下键换列，确定按下",
             "按一格会把它和上下左右一起翻面",
             "把灯全部熄灭，步数有限"},
    .keys = "上换行　下换列　确定按下",
    .ok_mode = PA_OK_PRESS,
    .star = {400, 900, 1600},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
