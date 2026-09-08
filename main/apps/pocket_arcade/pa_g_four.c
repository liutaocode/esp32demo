/* 四子棋 —— 七列的四子连珠。上下键挪落子的列,确定键落子,
   电脑用带剪枝的极小极大搜索,赢一局它就多想一层。 */
#include "pa_game.h"
#include <string.h>

enum {
    CELL = 22, BOARD_X = 22, BOARD_Y = 20, THINK_MS = 420,
    DEPTH_START = 4, DEPTH_MAX = 5, WIN_SCORE = 100000,
};

static const int8_t DIR_X[4] = {1, 0, 1, 1};
static const int8_t DIR_Y[4] = {0, 1, 1, -1};
/* 中间的列先搜,剪枝效果最好。 */
static const uint8_t ORDER[PA_FOUR_W] = {3, 2, 4, 1, 5, 0, 6};

static int drop_row(const pa_four_t *f, unsigned col)
{
    for (int row = PA_FOUR_H - 1; row >= 0; row--)
        if (!f->cell[row][col]) return row;
    return -1;
}

static bool wins_at(const pa_four_t *f, int col, int row, uint8_t who)
{
    for (unsigned d = 0; d < 4; d++) {
        int run = 1;
        for (int sign = -1; sign <= 1; sign += 2)
            for (int step = 1; step < 4; step++) {
                int x = col + DIR_X[d] * step * sign;
                int y = row + DIR_Y[d] * step * sign;
                if (x < 0 || y < 0 || x >= PA_FOUR_W || y >= PA_FOUR_H) break;
                if (f->cell[y][x] != who) break;
                run++;
            }
        if (run >= 4) return true;
    }
    return false;
}

static bool board_full(const pa_four_t *f)
{
    for (unsigned col = 0; col < PA_FOUR_W; col++)
        if (!f->cell[0][col]) return false;
    return true;
}

/* 电脑视角的局面分:数每一条四连窗口里双方各有几子。 */
static int evaluate(const pa_four_t *f)
{
    static const int WEIGHT[4] = {0, 1, 14, 90};
    int total = 0;
    for (int row = 0; row < PA_FOUR_H; row++)
        for (int col = 0; col < PA_FOUR_W; col++)
            for (unsigned d = 0; d < 4; d++) {
                int last_x = col + DIR_X[d] * 3, last_y = row + DIR_Y[d] * 3;
                if (last_x < 0 || last_y < 0 || last_x >= PA_FOUR_W || last_y >= PA_FOUR_H)
                    continue;
                int mine = 0, yours = 0;
                for (int step = 0; step < 4; step++) {
                    uint8_t who = f->cell[row + DIR_Y[d] * step][col + DIR_X[d] * step];
                    if (who == 2) mine++;
                    else if (who == 1) yours++;
                }
                if (mine && yours) continue;
                /* 拦截对方比自己多连一子更值钱,不然它只顾自己排队。 */
                total += mine ? WEIGHT[mine] : -WEIGHT[yours] * 2;
            }
    return total;
}

static int search(pa_four_t *f, unsigned depth, int alpha, int beta, bool maximising)
{
    if (board_full(f)) return 0;
    if (!depth) return evaluate(f);
    int best = maximising ? -WIN_SCORE * 2 : WIN_SCORE * 2;
    for (unsigned i = 0; i < PA_FOUR_W; i++) {
        unsigned col = ORDER[i];
        int row = drop_row(f, col);
        if (row < 0) continue;
        uint8_t who = maximising ? 2 : 1;
        f->cell[row][col] = who;
        int value;
        if (wins_at(f, (int)col, row, who))
            value = maximising ? WIN_SCORE + (int)depth : -WIN_SCORE - (int)depth;
        else
            value = search(f, depth - 1, alpha, beta, !maximising);
        f->cell[row][col] = 0;
        if (maximising) {
            if (value > best) best = value;
            if (best > alpha) alpha = best;
        } else {
            if (value < best) best = value;
            if (best < beta) beta = best;
        }
        if (beta <= alpha) break;
    }
    return best;
}

static unsigned best_column(pa_four_t *f)
{
    int best = -WIN_SCORE * 2;
    unsigned choice = ORDER[0];
    for (unsigned i = 0; i < PA_FOUR_W; i++) {
        unsigned col = ORDER[i];
        int row = drop_row(f, col);
        if (row < 0) continue;
        f->cell[row][col] = 2;
        int value = wins_at(f, (int)col, row, 2)
                        ? WIN_SCORE
                        : search(f, f->depth - 1, -WIN_SCORE * 2, WIN_SCORE * 2, false);
        f->cell[row][col] = 0;
        if (value > best) { best = value; choice = col; }
    }
    return choice;
}

static void new_board(pa_run_t *run)
{
    pa_four_t *f = &run->u.four;
    memset(f->cell, 0, sizeof(f->cell));
    f->col = PA_FOUR_W / 2;
    f->turn = 0;
    f->result = 0;
    f->last_col = -1;
    f->think_ms = 0;
}

static void reset(pa_run_t *run)
{
    pa_four_t *f = &run->u.four;
    memset(f, 0, sizeof(*f));
    f->depth = DEPTH_START;
    f->round = 1;
    new_board(run);
}

static void place(pa_run_t *run, unsigned col, uint8_t who)
{
    pa_four_t *f = &run->u.four;
    int row = drop_row(f, col);
    if (row < 0) return;
    f->cell[row][col] = who;
    f->last_col = (int8_t)col;
    if (wins_at(f, (int)col, row, who)) {
        f->result = who;
        f->think_ms = 1200;
        return;
    }
    if (board_full(f)) {
        f->result = 3;
        f->think_ms = 1200;
        return;
    }
    f->turn = (uint8_t)(who == 1 ? 1 : 0);
    if (f->turn) f->think_ms = THINK_MS;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_four_t *f = &run->u.four;
    if (f->think_ms > ms) { f->think_ms = (uint16_t)(f->think_ms - ms); return; }
    f->think_ms = 0;
    if (f->result) {
        if (f->result == 1) {
            run->score += 150 + (long)f->round * 20;
            if (f->depth < DEPTH_MAX) f->depth++;
            f->round++;
            new_board(run);
            return;
        }
        if (f->result == 3) {
            run->score += 60;
            f->round++;
            new_board(run);
            return;
        }
        run->over = true;
        run->note = "被电脑连成四子";
        return;
    }
    if (f->turn) place(run, best_column(f), 2);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_four_t *f = &run->u.four;
    if (f->turn || f->result) return;
    if (pressed == PA_KEY_UP) {
        f->col = (uint8_t)((f->col + PA_FOUR_W - 1U) % PA_FOUR_W);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        f->col = (uint8_t)((f->col + 1U) % PA_FOUR_W);
        return;
    }
    if (drop_row(f, f->col) < 0) return;
    place(run, f->col, 1);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_four_t *f = &run->u.four;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   第 %u 局", run->score, (unsigned)f->round);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x123C6B, 4);
    if (!f->turn && !f->result)
        pa_frect(scene, BOARD_X + (int)f->col * CELL + 6, 4, 10, 10, PA_YELLOW, 2);

    pa_frect(scene, BOARD_X - 3, BOARD_Y - 3, PA_FOUR_W * CELL + 6, PA_FOUR_H * CELL + 6,
             0x1F5FA8, 4);
    for (int row = 0; row < PA_FOUR_H; row++)
        for (int col = 0; col < PA_FOUR_W; col++) {
            uint8_t who = f->cell[row][col];
            uint32_t colour = who == 1 ? PA_YELLOW : who == 2 ? PA_RED : 0x0E3560;
            pa_frect(scene, BOARD_X + col * CELL + 2, BOARD_Y + row * CELL + 2,
                     CELL - 4, CELL - 4, colour, 9);
        }

    const char *line = "你是黄子，先手";
    uint32_t colour = PA_INK;
    if (f->result == 1) { line = "四子连成，赢下这局"; colour = PA_GREEN; }
    else if (f->result == 2) { line = "被连成四子了"; colour = PA_RED; }
    else if (f->result == 3) { line = "棋盘满了，平局"; colour = PA_ORANGE; }
    else if (f->turn) { line = "电脑正在想"; colour = PA_SLATE; }
    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, line, colour, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "上下选列　确定落子");
}

const pa_game_t pa_game_four = {
    .id = 28,
    .name = "四子棋", .genre = "棋类",
    .hint = "赢一局它多想一层",
    .rule = {"上下键挪落子的列，确定键落下",
             "横竖斜任意方向连成四子就赢",
             "赢一局电脑多想一层，输一局结束"},
    .keys = "上下选列　确定落子",
    .ok_mode = PA_OK_PRESS,
    .star = {200, 500, 900},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
