/* 黑白棋 —— 八乘八翻转棋。三个键装不下一个自由光标,所以上下键只在
   "合法落子点"之间循环,确定键落子:能走的地方本来就不多。 */
#include "pa_game.h"
#include <string.h>

enum { CELL = 18, BOARD_X = 27, BOARD_Y = 8, THINK_MS = 480 };

static const int8_t STEP_X[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int8_t STEP_Y[8] = {0, 1, 1, 1, 0, -1, -1, -1};

/* 角最值钱,角旁边最危险,这张表就是电脑的全部棋感。 */
static const int8_t WEIGHT[8][8] = {
    {100, -20, 10,  5,  5, 10, -20, 100},
    {-20, -40, -2, -2, -2, -2, -40, -20},
    { 10,  -2,  3,  1,  1,  3,  -2,  10},
    {  5,  -2,  1,  1,  1,  1,  -2,   5},
    {  5,  -2,  1,  1,  1,  1,  -2,   5},
    { 10,  -2,  3,  1,  1,  3,  -2,  10},
    {-20, -40, -2, -2, -2, -2, -40, -20},
    {100, -20, 10,  5,  5, 10, -20, 100},
};

static uint8_t other(uint8_t who) { return (uint8_t)(who == 1 ? 2 : 1); }

/* 落在 (x,y) 会翻掉几颗。apply 为真时顺便翻过来。 */
static unsigned flips(uint8_t cell[8][8], int x, int y, uint8_t who, bool apply)
{
    if (cell[y][x]) return 0;
    unsigned total = 0;
    for (unsigned d = 0; d < 8; d++) {
        int run = 0, cx = x + STEP_X[d], cy = y + STEP_Y[d];
        while (cx >= 0 && cy >= 0 && cx < 8 && cy < 8 && cell[cy][cx] == other(who)) {
            run++;
            cx += STEP_X[d];
            cy += STEP_Y[d];
        }
        if (!run || cx < 0 || cy < 0 || cx >= 8 || cy >= 8 || cell[cy][cx] != who) continue;
        total += (unsigned)run;
        if (!apply) continue;
        cx = x + STEP_X[d];
        cy = y + STEP_Y[d];
        for (int i = 0; i < run; i++) {
            cell[cy][cx] = who;
            cx += STEP_X[d];
            cy += STEP_Y[d];
        }
    }
    if (apply && total) cell[y][x] = who;
    return total;
}

static uint8_t list_moves(uint8_t cell[8][8], uint8_t who, uint8_t *out)
{
    uint8_t count = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (flips(cell, x, y, who, false)) out[count++] = (uint8_t)(y * 8 + x);
    return count;
}

static unsigned discs(const uint8_t cell[8][8], uint8_t who)
{
    unsigned count = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) count += (cell[y][x] == who) ? 1U : 0U;
    return count;
}

static int value_for(const uint8_t cell[8][8], uint8_t who)
{
    int total = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            if (cell[y][x] == who) total += WEIGHT[y][x];
            else if (cell[y][x] == other(who)) total -= WEIGHT[y][x];
        }
    return total;
}

/* 一步自己加一步对手:够它避开角旁边的坑,又不至于慢到卡住一帧。 */
static uint8_t choose(uint8_t cell[8][8], uint8_t who)
{
    uint8_t moves[64];
    uint8_t count = list_moves(cell, who, moves);
    if (!count) return 64;
    int best = -100000;
    uint8_t choice = moves[0];
    for (uint8_t i = 0; i < count; i++) {
        uint8_t mine[8][8];
        memcpy(mine, cell, sizeof(mine));
        flips(mine, moves[i] % 8, moves[i] / 8, who, true);
        uint8_t replies[64];
        uint8_t reply_count = list_moves(mine, other(who), replies);
        int worst = value_for(mine, who);
        for (uint8_t j = 0; j < reply_count; j++) {
            uint8_t after[8][8];
            memcpy(after, mine, sizeof(after));
            flips(after, replies[j] % 8, replies[j] / 8, other(who), true);
            int value = value_for(after, who);
            if (j == 0 || value < worst) worst = value;
        }
        if (worst > best) { best = worst; choice = moves[i]; }
    }
    return choice;
}

static void refresh_moves(pa_reversi_t *r)
{
    r->count = list_moves(r->cell, 1, r->legal);
    if (r->pick >= r->count) r->pick = 0;
}

static void new_board(pa_run_t *run)
{
    pa_reversi_t *r = &run->u.reversi;
    memset(r->cell, 0, sizeof(r->cell));
    r->cell[3][3] = r->cell[4][4] = 2;
    r->cell[3][4] = r->cell[4][3] = 1;
    r->turn = 0;
    r->passes = 0;
    r->pick = 0;
    r->last = -1;
    r->think_ms = 0;
    refresh_moves(r);
}

static void reset(pa_run_t *run)
{
    pa_reversi_t *r = &run->u.reversi;
    memset(r, 0, sizeof(*r));
    r->round = 1;
    new_board(run);
}

static void finish_round(pa_run_t *run)
{
    pa_reversi_t *r = &run->u.reversi;
    unsigned mine = discs(r->cell, 1), theirs = discs(r->cell, 2);
    if (mine <= theirs) {
        run->over = true;
        run->note = (mine == theirs) ? "子数打平，这局算输" : "白子比黑子多";
        return;
    }
    run->score += 200 + (long)(mine - theirs) * 5;
    r->round++;
    new_board(run);
}

/* 双方都没地方下就结束这一局。 */
static void hand_over(pa_run_t *run)
{
    pa_reversi_t *r = &run->u.reversi;
    for (unsigned guard = 0; guard < 4; guard++) {
        uint8_t moves[64];
        uint8_t who = (uint8_t)(r->turn ? 2 : 1);
        if (list_moves(r->cell, who, moves)) {
            r->passes = 0;
            if (r->turn) r->think_ms = THINK_MS;
            refresh_moves(r);
            return;
        }
        r->passes++;
        if (r->passes >= 2) { finish_round(run); return; }
        r->turn = (uint8_t)!r->turn;
    }
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_reversi_t *r = &run->u.reversi;
    if (!r->turn) return;
    if (r->think_ms > ms) { r->think_ms = (uint16_t)(r->think_ms - ms); return; }
    r->think_ms = 0;
    uint8_t move = choose(r->cell, 2);
    if (move >= 64) { hand_over(run); return; }
    flips(r->cell, move % 8, move / 8, 2, true);
    r->last = (int8_t)move;
    r->turn = 0;
    hand_over(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_reversi_t *r = &run->u.reversi;
    if (r->turn || !r->count) return;
    if (pressed == PA_KEY_UP) {
        r->pick = (uint8_t)((r->pick + r->count - 1U) % r->count);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        r->pick = (uint8_t)((r->pick + 1U) % r->count);
        return;
    }
    uint8_t move = r->legal[r->pick];
    flips(r->cell, move % 8, move / 8, 1, true);
    r->last = (int8_t)move;
    r->turn = 1;
    hand_over(run);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_reversi_t *r = &run->u.reversi;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "黑 %u 比 %u 白   第 %u 局", discs(r->cell, 1), discs(r->cell, 2),
             (unsigned)r->round);
    pa_frect(scene, BOARD_X - 4, BOARD_Y - 4, 8 * CELL + 8, 8 * CELL + 8, 0x0C5E3A, 4);
    for (int i = 1; i < 8; i++) {
        pa_frect(scene, BOARD_X + i * CELL - 1, BOARD_Y, 1, 8 * CELL, 0x0A4E30, 0);
        pa_frect(scene, BOARD_X, BOARD_Y + i * CELL - 1, 8 * CELL, 1, 0x0A4E30, 0);
    }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            if (!r->cell[y][x]) continue;
            pa_frect(scene, BOARD_X + x * CELL + 2, BOARD_Y + y * CELL + 2,
                     CELL - 4, CELL - 4, r->cell[y][x] == 1 ? PA_INK : PA_WHITE, 7);
        }
    if (r->last >= 0)
        pa_frect(scene, BOARD_X + (r->last % 8) * CELL + 7,
                 BOARD_Y + (r->last / 8) * CELL + 7, 4, 4, PA_RED, 2);

    if (!r->turn)
        for (uint8_t i = 0; i < r->count; i++) {
            uint8_t move = r->legal[i];
            bool picked = (i == r->pick);
            pa_frect(scene, BOARD_X + (move % 8) * CELL + (picked ? 2 : 6),
                     BOARD_Y + (move / 8) * CELL + (picked ? 2 : 6),
                     picked ? CELL - 4 : 6, picked ? CELL - 4 : 6,
                     picked ? PA_YELLOW : 0x2E8B60, picked ? 7 : 3);
        }

    if (r->turn)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "电脑正在想", PA_SLATE,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "能下 %u 处   角最值钱", (unsigned)r->count);
    pa_footer(scene, "上下换落子点　确定落子");
}

const pa_game_t pa_game_reversi = {
    .id = 30,
    .name = "黑白棋", .genre = "棋类",
    .hint = "只在能下的点间切换",
    .rule = {"上下键在能落子的点之间切换",
             "夹住对方的子就把它们翻成自己的",
             "四个角最值钱，角旁边最危险"},
    .keys = "上下换点　确定落子",
    .ok_mode = PA_OK_PRESS,
    .star = {250, 700, 1400},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
