/* 扫雷 —— 六乘六。两个键当一个二维光标用:上键换行,下键换列,
   所以最远六下就能走到任何一格。确定翻开,双击插旗。 */
#include "pa_game.h"
#include <string.h>

enum { CELL = 24, BOARD_X = 27, BOARD_Y = 8, MINES_START = 6, MINES_MAX = 10, CLEAR_MS = 1100 };

static const uint32_t NUMBER_COLOUR[9] = {
    PA_INK, 0x2F6FD0, 0x168B79, PA_RED, PA_PURPLE, 0x8A5A2B, 0x1F7A8C, PA_INK, PA_GRAY
};

static unsigned neighbours(const pa_mines_t *m, int x, int y)
{
    unsigned count = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || ny < 0 || nx >= PA_MINE_W || ny >= PA_MINE_H) continue;
            if (m->cell[ny][nx] & PA_MINE_BOMB) count++;
        }
    return count;
}

/* 第一下一定安全:雷是在第一次翻开之后才撒的,并且避开那一格和它周围。 */
static void seed(pa_run_t *run, int safe_x, int safe_y)
{
    pa_mines_t *m = &run->u.mines;
    unsigned placed = 0;
    while (placed < m->mines) {
        unsigned x = pa_below(&run->rng, PA_MINE_W);
        unsigned y = pa_below(&run->rng, PA_MINE_H);
        if (m->cell[y][x] & PA_MINE_BOMB) continue;
        int dx = (int)x - safe_x, dy = (int)y - safe_y;
        if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) continue;
        m->cell[y][x] |= PA_MINE_BOMB;
        placed++;
    }
    m->seeded = true;
}

static void open_cell(pa_run_t *run, int x, int y)
{
    pa_mines_t *m = &run->u.mines;
    if (x < 0 || y < 0 || x >= PA_MINE_W || y >= PA_MINE_H) return;
    if (m->cell[y][x] & (PA_MINE_OPEN | PA_MINE_FLAG)) return;
    m->cell[y][x] |= PA_MINE_OPEN;
    m->opened++;
    if (neighbours(m, x, y)) return;
    /* 周围一颗雷都没有就顺着摊开,省掉一堆无意义的点击。 */
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) open_cell(run, x + dx, y + dy);
}

static void new_board(pa_run_t *run)
{
    pa_mines_t *m = &run->u.mines;
    memset(m->cell, 0, sizeof(m->cell));
    m->cx = m->cy = 0;
    m->opened = 0;
    m->seeded = false;
    m->boom = false;
}

static void reset(pa_run_t *run)
{
    pa_mines_t *m = &run->u.mines;
    memset(m, 0, sizeof(*m));
    m->mines = MINES_START;
    m->board = 1;
    new_board(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_mines_t *m = &run->u.mines;
    if (!m->clear_ms) return;
    m->clear_ms = (uint16_t)(m->clear_ms > ms ? m->clear_ms - ms : 0);
    if (m->clear_ms) return;
    m->board++;
    if (m->mines < MINES_MAX) m->mines++;
    new_board(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_mines_t *m = &run->u.mines;
    if (m->clear_ms) return;
    if (pressed == PA_KEY_UP) {
        m->cy = (uint8_t)((m->cy + 1U) % PA_MINE_H);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        m->cx = (uint8_t)((m->cx + 1U) % PA_MINE_W);
        return;
    }
    uint8_t *cell = &m->cell[m->cy][m->cx];
    if (pressed == PA_KEY_OK2) {
        if (!(*cell & PA_MINE_OPEN)) *cell ^= PA_MINE_FLAG;
        return;
    }
    if (*cell & (PA_MINE_OPEN | PA_MINE_FLAG)) return;
    if (!m->seeded) seed(run, m->cx, m->cy);
    if (*cell & PA_MINE_BOMB) {
        m->boom = true;
        run->over = true;
        run->note = "踩到雷了";
        return;
    }
    open_cell(run, m->cx, m->cy);
    if (m->opened + m->mines < PA_MINE_W * PA_MINE_H) return;
    run->score += 200 + (long)m->mines * 20;
    m->clear_ms = CLEAR_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_mines_t *m = &run->u.mines;
    unsigned flags = 0;
    for (int y = 0; y < PA_MINE_H; y++)
        for (int x = 0; x < PA_MINE_W; x++)
            if (m->cell[y][x] & PA_MINE_FLAG) flags++;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 片   雷 %u   旗 %u", (unsigned)m->board, (unsigned)m->mines, flags);
    pa_frect(scene, BOARD_X - 4, BOARD_Y - 4, PA_MINE_W * CELL + 8, PA_MINE_H * CELL + 8,
             0x6B7A8A, 4);
    for (int y = 0; y < PA_MINE_H; y++)
        for (int x = 0; x < PA_MINE_W; x++) {
            uint8_t cell = m->cell[y][x];
            int cx = BOARD_X + x * CELL, cy = BOARD_Y + y * CELL;
            bool open = (cell & PA_MINE_OPEN) != 0;
            bool show_bomb = m->boom && (cell & PA_MINE_BOMB);
            uint32_t face = show_bomb ? PA_RED : open ? 0xD6DCE2 : 0xA9B4C0;
            pa_frect(scene, cx + 1, cy + 1, CELL - 2, CELL - 2, face, 3);
            if (cell & PA_MINE_FLAG) {
                pa_frect(scene, cx + 10, cy + 5, 3, 14, PA_INK, 0);
                pa_frect(scene, cx + 6, cy + 5, 7, 7, PA_RED, 1);
                continue;
            }
            if (show_bomb) {
                pa_frect(scene, cx + 8, cy + 8, 8, 8, PA_INK, 4);
                continue;
            }
            if (!open) continue;
            unsigned around = neighbours(m, x, y);
            if (around)
                pa_num(scene, cx, PA_FIELD_Y + cy + 5, CELL, (long)around,
                       NUMBER_COLOUR[around], PA_FONT_NUM14, PA_CENTER);
        }
    if (!m->clear_ms) {
        int cx = BOARD_X + (int)m->cx * CELL, cy = BOARD_Y + (int)m->cy * CELL;
        pa_frect(scene, cx - 1, cy - 1, CELL + 2, 3, PA_YELLOW, 0);
        pa_frect(scene, cx - 1, cy + CELL - 2, CELL + 2, 3, PA_YELLOW, 0);
        pa_frect(scene, cx - 1, cy - 1, 3, CELL + 2, PA_YELLOW, 0);
        pa_frect(scene, cx + CELL - 2, cy - 1, 3, CELL + 2, PA_YELLOW, 0);
    }

    if (m->clear_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "这一片扫干净了", PA_GREEN,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   还剩 %u 格", run->score,
                 (unsigned)(PA_MINE_W * PA_MINE_H - m->mines - m->opened));
    pa_footer(scene, "上换行　下换列　确定翻开");
}

const pa_game_t pa_game_mines = {
    .id = 23,
    .name = "扫雷", .genre = "推理",
    .hint = "上换行下换列插旗",
    .rule = {"上键换一行，下键换一列",
             "确定翻开，双击确定插旗",
             "第一下一定安全，扫完换更难的"},
    .keys = "上换行　下换列　确定翻开",
    .ok_mode = PA_OK_CLICK,
    .star = {300, 800, 1500},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
