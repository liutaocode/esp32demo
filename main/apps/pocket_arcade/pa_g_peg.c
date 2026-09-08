/* 孔明棋 —— 三十三个孔的十字盘,中间空一格。一次跳过一颗子把它吃掉。
   能走的招法从来不多,所以上下键直接在招法之间循环。 */
#include "pa_game.h"
#include <string.h>

enum { CELL = 20, BOARD_X = 29, BOARD_Y = 8, CLEAR_MS = 1400 };

static const int8_t STEP_X[4] = {1, 0, -1, 0};
static const int8_t STEP_Y[4] = {0, 1, 0, -1};

static bool on_board(int x, int y)
{
    if (x < 0 || y < 0 || x >= PA_PEG_W || y >= PA_PEG_H) return false;
    /* 十字形:四个角上的三乘三方块挖掉。 */
    return (x >= 2 && x <= 4) || (y >= 2 && y <= 4);
}

static void list_moves(pa_peg_t *p)
{
    p->count = 0;
    for (int y = 0; y < PA_PEG_H; y++)
        for (int x = 0; x < PA_PEG_W; x++) {
            if (p->cell[y][x] != PA_PEG_STONE) continue;
            for (unsigned d = 0; d < 4; d++) {
                int mx = x + STEP_X[d], my = y + STEP_Y[d];
                int tx = x + STEP_X[d] * 2, ty = y + STEP_Y[d] * 2;
                if (!on_board(mx, my) || !on_board(tx, ty)) continue;
                if (p->cell[my][mx] != PA_PEG_STONE) continue;
                if (p->cell[ty][tx] != PA_PEG_HOLE) continue;
                if (p->count >= PA_PEG_MOVES) continue;
                p->from[p->count] = (uint8_t)(y * PA_PEG_W + x);
                p->dir[p->count] = (uint8_t)d;
                p->count++;
            }
        }
    if (p->pick >= p->count) p->pick = 0;
}

static void reset(pa_run_t *run)
{
    pa_peg_t *p = &run->u.peg;
    memset(p, 0, sizeof(*p));
    p->pegs = 0;
    for (int y = 0; y < PA_PEG_H; y++)
        for (int x = 0; x < PA_PEG_W; x++) {
            if (!on_board(x, y)) { p->cell[y][x] = PA_PEG_VOID; continue; }
            bool centre = (x == 3 && y == 3);
            p->cell[y][x] = centre ? PA_PEG_HOLE : PA_PEG_STONE;
            if (!centre) p->pegs++;
        }
    list_moves(p);
}

static void finish(pa_run_t *run)
{
    pa_peg_t *p = &run->u.peg;
    if (p->pegs == 1) run->score += 400;
    else if (p->pegs == 2) run->score += 200;
    else if (p->pegs == 3) run->score += 100;
    run->over = true;
    run->note = (p->pegs == 1) ? "只剩一颗，完美收官" : "没有能跳的了";
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_peg_t *p = &run->u.peg;
    if (!p->clear_ms) return;
    p->clear_ms = (uint16_t)(p->clear_ms > ms ? p->clear_ms - ms : 0);
    if (!p->clear_ms) finish(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_peg_t *p = &run->u.peg;
    if (p->clear_ms || !p->count) return;
    if (pressed == PA_KEY_UP) {
        p->pick = (uint8_t)((p->pick + p->count - 1U) % p->count);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        p->pick = (uint8_t)((p->pick + 1U) % p->count);
        return;
    }
    uint8_t from = p->from[p->pick], d = p->dir[p->pick];
    int x = from % PA_PEG_W, y = from / PA_PEG_W;
    p->cell[y][x] = PA_PEG_HOLE;
    p->cell[y + STEP_Y[d]][x + STEP_X[d]] = PA_PEG_HOLE;
    p->cell[y + STEP_Y[d] * 2][x + STEP_X[d] * 2] = PA_PEG_STONE;
    p->pegs--;
    p->jumps++;
    run->score += 10;
    list_moves(p);
    if (!p->count) p->clear_ms = CLEAR_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_peg_t *p = &run->u.peg;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "还剩 %u 颗   跳了 %u 次", (unsigned)p->pegs, (unsigned)p->jumps);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0xEFE3CC, 4);
    for (int y = 0; y < PA_PEG_H; y++)
        for (int x = 0; x < PA_PEG_W; x++) {
            if (p->cell[y][x] == PA_PEG_VOID) continue;
            int cx = BOARD_X + x * CELL, cy = BOARD_Y + y * CELL;
            pa_frect(scene, cx + 3, cy + 3, CELL - 6, CELL - 6, 0xC8B48E, 7);
            if (p->cell[y][x] == PA_PEG_STONE)
                pa_frect(scene, cx + 2, cy + 2, CELL - 4, CELL - 4, 0x6B4A2C, 8);
        }
    if (p->count && !p->clear_ms) {
        uint8_t from = p->from[p->pick], d = p->dir[p->pick];
        int x = from % PA_PEG_W, y = from / PA_PEG_W;
        int tx = x + STEP_X[d] * 2, ty = y + STEP_Y[d] * 2;
        pa_frect(scene, BOARD_X + x * CELL + 1, BOARD_Y + y * CELL + 1, CELL - 2,
                 CELL - 2, PA_RED, 9);
        pa_frect(scene, BOARD_X + x * CELL + 4, BOARD_Y + y * CELL + 4, CELL - 8,
                 CELL - 8, 0x6B4A2C, 8);
        pa_frect(scene, BOARD_X + tx * CELL + 6, BOARD_Y + ty * CELL + 6, CELL - 12,
                 CELL - 12, PA_GREEN, 5);
    }
    if (p->clear_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "没有能跳的了", PA_ORANGE,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   能跳 %u 种", run->score, (unsigned)p->count);
    pa_footer(scene, "上下换招法　确定跳过去");
}

const pa_game_t pa_game_peg = {
    .id = 21,
    .name = "孔明棋", .genre = "益智",
    .hint = "跳一颗吃一颗",
    .rule = {"上下键在能走的招法之间切换",
             "确定跳过去，被跨过的那颗被吃掉",
             "跳到没招为止，剩得越少分越高"},
    .keys = "上下换招　确定跳",
    .ok_mode = PA_OK_PRESS,
    .star = {350, 500, 700},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
