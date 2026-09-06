#include "clean_sweep_state.h"
#include <string.h>

/* 七种四格方块的四个朝向,取自通用的 4x4 排布。
   行列从左上角数,bit = 0x8000 >> (行 * 4 + 列)。 */
const uint16_t CS_SHAPE[CS_PIECES][CS_ROTATIONS] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, /* 长条 */
    {0x8E00, 0x6440, 0x0E20, 0x44C0}, /* J */
    {0x2E00, 0x4460, 0x0E80, 0xC440}, /* L */
    {0x6600, 0x6600, 0x6600, 0x6600}, /* 方块 */
    {0x6C00, 0x4620, 0x06C0, 0x8C40}, /* S */
    {0xC600, 0x2640, 0x0C60, 0x4C80}, /* Z */
    {0x4E00, 0x4640, 0x0E40, 0x4C40}, /* T */
};

/* 每一级的下落间隔。等级 = 已消行数 / 10,最高 14 级。 */
const uint16_t CS_FALL_MS[CS_MAX_LEVEL + 1] = {
    800, 720, 630, 550, 470, 380, 300, 240, 190, 150, 120, 100, 90, 80, 70
};

static const uint16_t LINE_POINTS[CS_MAX_CLEAR + 1] = {0, 100, 300, 500, 800};

bool cs_shape_cell(uint8_t piece, uint8_t rot, int row, int col)
{
    if (piece >= CS_PIECES || rot >= CS_ROTATIONS) return false;
    if (row < 0 || row > 3 || col < 0 || col > 3) return false;
    return (CS_SHAPE[piece][rot] & (uint16_t)(0x8000u >> (row * 4 + col))) != 0;
}

bool cs_fits(const cs_state_t *s, uint8_t piece, uint8_t rot, int x, int y)
{
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            if (!cs_shape_cell(piece, rot, row, col)) continue;
            int bx = x + col, by = y + row;
            if (bx < 0 || bx >= CS_COLS || by < 0 || by >= CS_ROWS) return false;
            if (s->cell[by][bx]) return false;
        }
    return true;
}

/* 出生高度:让方块最上面一格正好落在第 0 行。 */
static int top_offset(uint8_t piece, uint8_t rot)
{
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            if (cs_shape_cell(piece, rot, row, col)) return row;
    return 0;
}

static uint32_t next_random(cs_state_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x ? x : 0x1234567u;
    return s->rng;
}

/* 七连袋:每七个方块里七种各出现一次,不会连着来五个长条,也不会久等一个。 */
static uint8_t draw_piece(cs_state_t *s)
{
    if (s->bag_pos >= CS_PIECES) {
        for (uint8_t i = 0; i < CS_PIECES; i++) s->bag[i] = i;
        for (uint8_t i = CS_PIECES - 1; i > 0; i--) {
            uint8_t j = (uint8_t)(next_random(s) % (uint32_t)(i + 1));
            uint8_t tmp = s->bag[i];
            s->bag[i] = s->bag[j];
            s->bag[j] = tmp;
        }
        s->bag_pos = 0;
    }
    return s->bag[s->bag_pos++];
}

uint32_t cs_fall_interval(const cs_state_t *s)
{
    uint32_t level = s->level > CS_MAX_LEVEL ? CS_MAX_LEVEL : s->level;
    return CS_FALL_MS[level];
}

uint32_t cs_fx_total(const cs_state_t *s)
{
    if (s->fx == CS_FX_FLASH) return CS_FLASH_MS;
    if (s->fx == CS_FX_BURST) return CS_BURST_MS;
    return CS_COLLAPSE_MS;
}

unsigned cs_fx_progress(const cs_state_t *s)
{
    uint32_t total = cs_fx_total(s);
    if (!total) return 1000;
    uint32_t done = s->fx_ms >= total ? total : s->fx_ms;
    return (unsigned)(done * 1000u / total);
}

bool cs_row_clearing(const cs_state_t *s, int row)
{
    for (unsigned i = 0; i < s->clear_count; i++)
        if (s->clear_rows[i] == row) return true;
    return false;
}

unsigned cs_row_shift(const cs_state_t *s, int row)
{
    unsigned shift = 0;
    for (unsigned i = 0; i < s->clear_count; i++)
        if (s->clear_rows[i] > row) shift++;
    return shift;
}

unsigned cs_top_row(const cs_state_t *s)
{
    for (int row = 0; row < CS_ROWS; row++)
        for (int col = 0; col < CS_COLS; col++)
            if (s->cell[row][col]) return (unsigned)row;
    return CS_ROWS;
}

int cs_ghost_y(const cs_state_t *s)
{
    int y = s->py;
    while (cs_fits(s, s->piece, s->rot, s->px, y + 1)) y++;
    return y;
}

static void finish_game(cs_state_t *s, bool won)
{
    s->page = CS_RESULT;
    s->won = won;
    s->new_best = false;
    if (s->lines > s->best_lines) s->best_lines = s->lines;
    if (s->mode == CS_SPRINT) {
        if (won && (!s->best_sprint_ms || s->elapsed_ms < s->best_sprint_ms)) {
            s->best_sprint_ms = s->elapsed_ms;
            s->new_best = true;
        }
    } else if (s->score > s->best_score) {
        s->best_score = s->score;
        s->new_best = s->score > 0;
    }
}

static void spawn(cs_state_t *s)
{
    s->piece = s->next;
    s->next = draw_piece(s);
    s->rot = 0;
    s->px = CS_SPAWN_X;
    s->py = (int8_t)(-top_offset(s->piece, 0));
    s->drop_ms = 0;
    s->lock_ms = 0;
    s->lock_resets = 0;
    s->piece_serial++;
    if (!cs_fits(s, s->piece, s->rot, s->px, s->py)) finish_game(s, false);
}

/* 把当前方块写进棋盘,找出被填满的行。有整行就进入消除动画。 */
static void lock_piece(cs_state_t *s)
{
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            if (!cs_shape_cell(s->piece, s->rot, row, col)) continue;
            int bx = s->px + col, by = s->py + row;
            if (bx >= 0 && bx < CS_COLS && by >= 0 && by < CS_ROWS)
                s->cell[by][bx] = (uint8_t)(s->piece + 1);
        }
    s->pieces++;
    s->clear_count = 0;
    for (int row = 0; row < CS_ROWS; row++) {
        bool full = true;
        for (int col = 0; col < CS_COLS && full; col++)
            if (!s->cell[row][col]) full = false;
        if (full && s->clear_count < CS_MAX_CLEAR) s->clear_rows[s->clear_count++] = (uint8_t)row;
    }
    if (!s->clear_count) {
        s->combo = 0;
        spawn(s);
        return;
    }

    uint32_t multiplier = s->level + 1;
    s->combo++;
    if (s->combo > s->best_combo) s->best_combo = s->combo;
    s->gain = LINE_POINTS[s->clear_count] * multiplier;
    if (s->combo > 1) s->gain += (s->combo - 1) * 50 * multiplier;
    s->perfect = true;
    for (int row = 0; row < CS_ROWS && s->perfect; row++) {
        if (cs_row_clearing(s, row)) continue;
        for (int col = 0; col < CS_COLS; col++)
            if (s->cell[row][col]) { s->perfect = false; break; }
    }
    if (s->perfect) s->gain += 1000 * multiplier;
    s->score += s->gain;
    s->lines += s->clear_count;
    s->level = s->lines / 10;
    if (s->level > CS_MAX_LEVEL) s->level = CS_MAX_LEVEL;
    s->page = CS_CLEARING;
    s->fx = CS_FX_FLASH;
    s->fx_ms = 0;
}

/* 动画播完:整行消失,上方落下,然后出下一个方块。 */
static void apply_clear(cs_state_t *s)
{
    int write = CS_ROWS - 1;
    for (int row = CS_ROWS - 1; row >= 0; row--) {
        if (cs_row_clearing(s, row)) continue;
        if (write != row) memcpy(s->cell[write], s->cell[row], CS_COLS);
        write--;
    }
    for (int row = write; row >= 0; row--) memset(s->cell[row], 0, CS_COLS);
    s->clear_count = 0;
    s->fx_ms = 0;
    s->page = CS_PLAY;
    if (s->mode == CS_SPRINT && s->lines >= CS_SPRINT_LINES) {
        finish_game(s, true);
        return;
    }
    spawn(s);
}

void cs_home(cs_state_t *s)
{
    s->page = CS_HOME;
    s->clear_count = 0;
    s->combo = 0;
}

void cs_start(cs_state_t *s, uint32_t challenge)
{
    uint32_t best_score = s->best_score, best_lines = s->best_lines;
    uint32_t best_sprint = s->best_sprint_ms;
    uint8_t mode = s->mode;

    memset(s, 0, sizeof(*s));
    s->mode = mode;
    s->best_score = best_score;
    s->best_lines = best_lines;
    s->best_sprint_ms = best_sprint;
    s->challenge = challenge % CS_CHALLENGE_MAX;
    s->rng = s->challenge * 2654435761u + 12345u;
    if (!s->rng) s->rng = 0x1234567u;
    s->bag_pos = CS_PIECES;
    s->next = draw_piece(s);
    s->page = CS_PLAY;
    spawn(s);
}

bool cs_move(cs_state_t *s, int dir)
{
    if (s->page != CS_PLAY || (dir != -1 && dir != 1)) return false;
    if (!cs_fits(s, s->piece, s->rot, s->px + dir, s->py)) return false;
    s->px = (int8_t)(s->px + dir);
    if (!cs_fits(s, s->piece, s->rot, s->px, s->py + 1) && s->lock_resets < CS_LOCK_RESET_MAX) {
        s->lock_resets++;
        s->lock_ms = 0;
    }
    return true;
}

bool cs_rotate(cs_state_t *s)
{
    if (s->page != CS_PLAY) return false;
    /* 踢墙:先原地,再左右挪一到两格,最后上下各让一行。
       长条刚出生时顶在第 0 行,只有向下让一行才能立起来。 */
    static const int8_t KICK_X[] = {0, -1, 1, -2, 2};
    static const int8_t KICK_Y[] = {0, 1, -1};
    uint8_t rot = (uint8_t)((s->rot + 1) % CS_ROTATIONS);
    for (unsigned k = 0; k < sizeof(KICK_Y) / sizeof(KICK_Y[0]); k++)
        for (unsigned i = 0; i < sizeof(KICK_X) / sizeof(KICK_X[0]); i++) {
            int x = s->px + KICK_X[i], y = s->py + KICK_Y[k];
            if (!cs_fits(s, s->piece, rot, x, y)) continue;
            s->rot = rot;
            s->px = (int8_t)x;
            s->py = (int8_t)y;
            if (!cs_fits(s, s->piece, s->rot, s->px, s->py + 1) &&
                s->lock_resets < CS_LOCK_RESET_MAX) {
                s->lock_resets++;
                s->lock_ms = 0;
            }
            return true;
        }
    return false;
}

bool cs_drop(cs_state_t *s)
{
    if (s->page != CS_PLAY) return false;
    uint32_t rows = 0;
    while (cs_fits(s, s->piece, s->rot, s->px, s->py + 1)) {
        s->py++;
        rows++;
    }
    s->score += rows * 2;
    lock_piece(s);
    return true;
}

bool cs_pause(cs_state_t *s)
{
    if (s->page != CS_PLAY) return false;
    s->page = CS_PAUSED;
    return true;
}

bool cs_resume(cs_state_t *s)
{
    if (s->page != CS_PAUSED) return false;
    s->page = CS_PLAY;
    s->lock_ms = 0;
    s->drop_ms = 0;
    return true;
}

void cs_tick(cs_state_t *s, uint32_t ms)
{
    if (ms > CS_TICK_MAX_MS) ms = CS_TICK_MAX_MS;
    if (!ms) return;

    if (s->page == CS_PLAY) {
        s->elapsed_ms += ms;
        if (cs_fits(s, s->piece, s->rot, s->px, s->py + 1)) {
            s->lock_ms = 0;
            s->drop_ms += ms;
            uint32_t interval = cs_fall_interval(s);
            while (s->drop_ms >= interval &&
                   cs_fits(s, s->piece, s->rot, s->px, s->py + 1)) {
                s->drop_ms -= interval;
                s->py++;
            }
        }
        if (!cs_fits(s, s->piece, s->rot, s->px, s->py + 1)) {
            s->drop_ms = 0;
            s->lock_ms += ms;
            if (s->lock_ms >= CS_LOCK_MS) lock_piece(s);
        }
        return;
    }

    if (s->page != CS_CLEARING) return;
    s->fx_ms += ms;
    while (s->page == CS_CLEARING && s->fx_ms >= cs_fx_total(s)) {
        uint32_t total = cs_fx_total(s);
        if (s->fx == CS_FX_COLLAPSE) {
            apply_clear(s);
            break;
        }
        s->fx_ms -= total;
        s->fx = s->fx == CS_FX_FLASH ? CS_FX_BURST : CS_FX_COLLAPSE;
    }
}
