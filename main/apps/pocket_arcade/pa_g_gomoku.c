/* 五子棋 —— 九路小棋盘。上键换行、下键换列当二维光标用,确定键落子。
   电脑一步一算:自己能连多少,加上对手不拦就会连多少。 */
#include "pa_game.h"
#include <string.h>

enum { CELL = 16, BOARD_X = 27, BOARD_Y = 8, THINK_MS = 380, NEED = 5 };

static const int8_t DIR_X[4] = {1, 0, 1, 1};
static const int8_t DIR_Y[4] = {0, 1, 1, -1};

static bool inside(int x, int y)
{
    return x >= 0 && y >= 0 && x < PA_GOMOKU && y < PA_GOMOKU;
}

/* 往一个方向数连着的同色子,并回报那一头是不是空的。 */
static int run_length(const pa_gomoku_t *g, int x, int y, int dx, int dy,
                      uint8_t who, bool *open)
{
    int run = 0, cx = x + dx, cy = y + dy;
    while (inside(cx, cy) && g->cell[cy][cx] == who) {
        run++;
        cx += dx;
        cy += dy;
    }
    *open = inside(cx, cy) && !g->cell[cy][cx];
    return run;
}

static bool wins_at(const pa_gomoku_t *g, int x, int y, uint8_t who)
{
    for (unsigned d = 0; d < 4; d++) {
        bool ignore;
        int total = 1 + run_length(g, x, y, DIR_X[d], DIR_Y[d], who, &ignore)
                      + run_length(g, x, y, -DIR_X[d], -DIR_Y[d], who, &ignore);
        if (total >= NEED) return true;
    }
    return false;
}

/* 一个空点对某一方的价值。连得越长、两头越开阔,越值钱。 */
static int spot_value(const pa_gomoku_t *g, int x, int y, uint8_t who)
{
    int total = 0;
    for (unsigned d = 0; d < 4; d++) {
        bool open_a = false, open_b = false;
        int a = run_length(g, x, y, DIR_X[d], DIR_Y[d], who, &open_a);
        int b = run_length(g, x, y, -DIR_X[d], -DIR_Y[d], who, &open_b);
        int run = a + b + 1;
        int ends = (open_a ? 1 : 0) + (open_b ? 1 : 0);
        if (run >= NEED) return 1000000;
        if (run == 4) total += ends ? 20000 : 0;
        else if (run == 3) total += ends == 2 ? 4000 : ends ? 400 : 0;
        else if (run == 2) total += ends == 2 ? 300 : ends ? 40 : 0;
        else total += ends == 2 ? 20 : ends ? 5 : 0;
    }
    return total;
}

static bool near_stone(const pa_gomoku_t *g, int x, int y)
{
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++) {
            if (!inside(x + dx, y + dy)) continue;
            if (g->cell[y + dy][x + dx]) return true;
        }
    return false;
}

static bool board_full(const pa_gomoku_t *g)
{
    for (int y = 0; y < PA_GOMOKU; y++)
        for (int x = 0; x < PA_GOMOKU; x++)
            if (!g->cell[y][x]) return false;
    return true;
}

static void new_board(pa_run_t *run)
{
    pa_gomoku_t *g = &run->u.gomoku;
    memset(g->cell, 0, sizeof(g->cell));
    g->cx = g->cy = PA_GOMOKU / 2;
    g->turn = 0;
    g->result = 0;
    g->last = -1;
    g->think_ms = 0;
}

static void reset(pa_run_t *run)
{
    pa_gomoku_t *g = &run->u.gomoku;
    memset(g, 0, sizeof(*g));
    g->round = 1;
    new_board(run);
}

static void place(pa_run_t *run, int x, int y, uint8_t who)
{
    pa_gomoku_t *g = &run->u.gomoku;
    g->cell[y][x] = who;
    g->last = (int8_t)(y * PA_GOMOKU + x);
    if (wins_at(g, x, y, who)) {
        g->result = who;
        g->think_ms = 1300;
        return;
    }
    if (board_full(g)) {
        g->result = 3;
        g->think_ms = 1300;
        return;
    }
    g->turn = (uint8_t)(who == 1 ? 1 : 0);
    if (g->turn) g->think_ms = THINK_MS;
}

static void computer_move(pa_run_t *run)
{
    pa_gomoku_t *g = &run->u.gomoku;
    int best = -1, bx = PA_GOMOKU / 2, by = PA_GOMOKU / 2;
    for (int y = 0; y < PA_GOMOKU; y++)
        for (int x = 0; x < PA_GOMOKU; x++) {
            if (g->cell[y][x] || !near_stone(g, x, y)) continue;
            /* 拦对手比自己多排一子稍微重要一点,不然它会只顾进攻。 */
            int value = spot_value(g, x, y, 2) + spot_value(g, x, y, 1) * 11 / 10;
            if (value > best) { best = value; bx = x; by = y; }
        }
    place(run, bx, by, 2);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_gomoku_t *g = &run->u.gomoku;
    if (g->think_ms > ms) { g->think_ms = (uint16_t)(g->think_ms - ms); return; }
    g->think_ms = 0;
    if (g->result) {
        if (g->result == 1) {
            run->score += 250 + (long)g->round * 30;
            g->round++;
            new_board(run);
            return;
        }
        if (g->result == 3) {
            run->score += 80;
            g->round++;
            new_board(run);
            return;
        }
        run->over = true;
        run->note = "被电脑连成五子";
        return;
    }
    if (g->turn) computer_move(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_gomoku_t *g = &run->u.gomoku;
    if (g->turn || g->result) return;
    if (pressed == PA_KEY_UP) { g->cy = (uint8_t)((g->cy + 1U) % PA_GOMOKU); return; }
    if (pressed == PA_KEY_DOWN) { g->cx = (uint8_t)((g->cx + 1U) % PA_GOMOKU); return; }
    if (g->cell[g->cy][g->cx]) return;
    place(run, g->cx, g->cy, 1);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_gomoku_t *g = &run->u.gomoku;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   第 %u 局", run->score, (unsigned)g->round);
    pa_frect(scene, BOARD_X - 5, BOARD_Y - 5, PA_GOMOKU * CELL + 10,
             PA_GOMOKU * CELL + 10, 0xC9A66B, 4);
    for (int i = 0; i < PA_GOMOKU; i++) {
        pa_frect(scene, BOARD_X + i * CELL + CELL / 2, BOARD_Y + CELL / 2, 1,
                 (PA_GOMOKU - 1) * CELL, 0x8A6B4B, 0);
        pa_frect(scene, BOARD_X + CELL / 2, BOARD_Y + i * CELL + CELL / 2,
                 (PA_GOMOKU - 1) * CELL, 1, 0x8A6B4B, 0);
    }
    for (int y = 0; y < PA_GOMOKU; y++)
        for (int x = 0; x < PA_GOMOKU; x++) {
            if (!g->cell[y][x]) continue;
            pa_frect(scene, BOARD_X + x * CELL + 2, BOARD_Y + y * CELL + 2,
                     CELL - 4, CELL - 4, g->cell[y][x] == 1 ? PA_INK : PA_WHITE, 6);
        }
    if (g->last >= 0)
        pa_frect(scene, BOARD_X + (g->last % PA_GOMOKU) * CELL + CELL / 2 - 1,
                 BOARD_Y + (g->last / PA_GOMOKU) * CELL + CELL / 2 - 1, 3, 3, PA_RED, 1);
    if (!g->turn && !g->result) {
        int cx = BOARD_X + (int)g->cx * CELL, cy = BOARD_Y + (int)g->cy * CELL;
        pa_frect(scene, cx, cy, CELL, 2, PA_RED, 0);
        pa_frect(scene, cx, cy + CELL - 2, CELL, 2, PA_RED, 0);
        pa_frect(scene, cx, cy, 2, CELL, PA_RED, 0);
        pa_frect(scene, cx + CELL - 2, cy, 2, CELL, PA_RED, 0);
    }

    const char *line = "你是黑子，先手";
    uint32_t colour = PA_INK;
    if (g->result == 1) { line = "五子连成，赢了"; colour = PA_GREEN; }
    else if (g->result == 2) { line = "被连成五子了"; colour = PA_RED; }
    else if (g->result == 3) { line = "棋盘满了，平局"; colour = PA_ORANGE; }
    else if (g->turn) { line = "电脑正在想"; colour = PA_SLATE; }
    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, line, colour, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "上换行　下换列　确定落子");
}

const pa_game_t pa_game_gomoku = {
    .id = 29,
    .name = "五子棋", .genre = "棋类",
    .hint = "九路盘，先连五子",
    .rule = {"上键换行，下键换列，确定落子",
             "横竖斜任意方向先连成五子就赢",
             "赢一局换下一盘，输一局结束"},
    .keys = "上换行　下换列　确定落子",
    .ok_mode = PA_OK_PRESS,
    .star = {300, 700, 1200},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
