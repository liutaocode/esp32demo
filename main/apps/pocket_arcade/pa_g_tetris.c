/* 方块塔 —— 八列的俄罗斯方块。三个键分别是左移、右移和旋转,没有硬降,
   所以每一次落点都靠提前判断,而不是靠手速。 */
#include "pa_game.h"
#include <string.h>

enum { CELL = 11, WELL_X = 55, WELL_Y = 3, FALL_START = 780, FALL_MIN = 180 };

/* 七种方块画在 4x4 的格子里,位序是 y*4+x。旋转在运行时算,省一张表。 */
static const uint16_t SHAPE[7] = {0x00F0, 0x0660, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074};
static const uint32_t COLOR[8] = {
    0, 0x3FC7E0, PA_YELLOW, PA_PURPLE, PA_GRASS, PA_RED, PA_BLUE, PA_ORANGE
};
static const long CLEAR_SCORE[5] = {0, 100, 300, 600, 1000};

static uint16_t rotate_cw(uint16_t mask)
{
    uint16_t out = 0;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (mask & (1U << (y * 4 + x))) out |= (uint16_t)(1U << (x * 4 + (3 - y)));
    return out;
}

static uint16_t shape_of(uint8_t piece, uint8_t rot)
{
    uint16_t mask = SHAPE[piece % 7];
    for (uint8_t i = 0; i < rot % 4; i++) mask = rotate_cw(mask);
    return mask;
}

static bool blocked(const pa_tetris_t *t, uint8_t piece, uint8_t rot, int px, int py)
{
    uint16_t mask = shape_of(piece, rot);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (!(mask & (1U << (y * 4 + x)))) continue;
            int col = px + x, row = py + y;
            if (col < 0 || col >= PA_TET_COLS || row >= PA_TET_ROWS) return true;
            if (row >= 0 && t->cell[row][col]) return true;
        }
    }
    return false;
}

static void spawn(pa_run_t *run)
{
    pa_tetris_t *t = &run->u.tetris;
    t->piece = t->next;
    t->next = (uint8_t)pa_below(&run->rng, 7);
    t->rot = 0;
    t->px = 2;
    t->py = 0;
    if (blocked(t, t->piece, t->rot, t->px, t->py)) {
        run->over = true;
        run->note = "塔顶堵住了";
    }
}

static void reset(pa_run_t *run)
{
    pa_tetris_t *t = &run->u.tetris;
    memset(t, 0, sizeof(*t));
    t->fall_ms = FALL_START;
    t->next = (uint8_t)pa_below(&run->rng, 7);
    spawn(run);
}

static void clear_rows(pa_run_t *run)
{
    pa_tetris_t *t = &run->u.tetris;
    uint8_t cleared = 0;
    for (int row = PA_TET_ROWS - 1; row >= 0; row--) {
        bool full = true;
        for (int col = 0; col < PA_TET_COLS; col++)
            if (!t->cell[row][col]) { full = false; break; }
        if (!full) continue;
        for (int above = row; above > 0; above--)
            memcpy(t->cell[above], t->cell[above - 1], PA_TET_COLS);
        memset(t->cell[0], 0, PA_TET_COLS);
        cleared++;
        row++;   /* 同一行要重新检查,上面塌下来的可能也是满的 */
    }
    if (!cleared) return;
    t->lines = (uint16_t)(t->lines + cleared);
    t->level = t->lines / 8U;
    if (t->level > 10U) t->level = 10U;
    t->fall_ms = (uint16_t)(FALL_START - t->level * 55U);
    if (t->fall_ms < FALL_MIN) t->fall_ms = FALL_MIN;
    t->flash_rows = cleared;
    t->flash_ms = 900;
    run->score += CLEAR_SCORE[cleared] * (long)(t->level + 1U);
}

static void lock_piece(pa_run_t *run)
{
    pa_tetris_t *t = &run->u.tetris;
    uint16_t mask = shape_of(t->piece, t->rot);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (mask & (1U << (y * 4 + x))) {
                int row = t->py + y, col = t->px + x;
                if (row >= 0 && row < PA_TET_ROWS && col >= 0 && col < PA_TET_COLS)
                    t->cell[row][col] = (uint8_t)(t->piece + 1U);
            }
    clear_rows(run);
    spawn(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_tetris_t *t = &run->u.tetris;
    if (t->flash_ms) t->flash_ms = (uint16_t)(t->flash_ms > ms ? t->flash_ms - ms : 0);
    t->acc_ms += ms;
    while (!run->over && t->acc_ms >= t->fall_ms) {
        t->acc_ms -= t->fall_ms;
        if (!blocked(t, t->piece, t->rot, t->px, t->py + 1)) {
            t->py++;
            run->score += 1;
        } else {
            lock_piece(run);
        }
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_tetris_t *t = &run->u.tetris;
    if (pressed == PA_KEY_OK) {
        uint8_t rot = (uint8_t)((t->rot + 1U) % 4U);
        /* 贴墙旋转时先原地试,再左右各让一格,让转向不至于卡死。 */
        static const int8_t KICK[3] = {0, -1, 1};
        for (unsigned i = 0; i < 3; i++)
            if (!blocked(t, t->piece, rot, t->px + KICK[i], t->py)) {
                t->rot = rot;
                t->px = (int8_t)(t->px + KICK[i]);
                return;
            }
        return;
    }
    int dx = (pressed == PA_KEY_UP) ? -1 : 1;
    if (!blocked(t, t->piece, t->rot, t->px + dx, t->py)) t->px = (int8_t)(t->px + dx);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_tetris_t *t = &run->u.tetris;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   消 %u 行", run->score, (unsigned)t->lines);

    pa_frect(scene, WELL_X - 3, WELL_Y - 3, PA_TET_COLS * CELL + 6, PA_TET_ROWS * CELL + 6,
             PA_SLATE, 4);
    for (int row = 0; row < PA_TET_ROWS; row++)
        for (int col = 0; col < PA_TET_COLS; col++)
            if (t->cell[row][col])
                pa_frect(scene, WELL_X + col * CELL + 1, WELL_Y + row * CELL + 1, 9, 9,
                         COLOR[t->cell[row][col]], 2);

    if (!run->over) {
        uint16_t mask = shape_of(t->piece, t->rot);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                if (mask & (1U << (y * 4 + x)))
                    pa_frect(scene, WELL_X + (t->px + x) * CELL + 1,
                             WELL_Y + (t->py + y) * CELL + 1, 9, 9,
                             COLOR[t->piece + 1], 2);
    }

    /* 井右边留出下一块预览,让玩家能提前决定往哪边挪。 */
    pa_frect(scene, 150, 6, 44, 44, 0x0F5FA6, 4);
    uint16_t next = shape_of(t->next, 0);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (next & (1U << (y * 4 + x)))
                pa_frect(scene, 152 + x * 10, 8 + y * 10, 9, 9, COLOR[t->next + 1], 2);
    pa_text(scene, 146, PA_FIELD_Y + 52, 52, "下一块", PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_textf(scene, 146, PA_FIELD_Y + 74, 52, PA_INK, PA_FONT_ZH, PA_CENTER,
             "速度 %u", (unsigned)(t->level + 1U));

    if (t->flash_ms)
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_GREEN, PA_FONT_ZH, PA_CENTER,
                 "一次消掉 %u 行", (unsigned)t->flash_rows);
    else
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "四行一起消，分数最高",
                PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "上左移　下右移　确定旋转");
}

const pa_game_t pa_game_tetris = {
    .id = 16,
    .name = "方块塔", .genre = "益智",
    .hint = "八列窄井靠预判",
    .rule = {"上键左移，下键右移，确定旋转",
             "右上角是下一块，提前想好落点",
             "一次消掉四行拿一千分"},
    .keys = "上左移　下右移　确定旋转",
    .ok_mode = PA_OK_PRESS,
    .star = {800, 1800, 3500},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
