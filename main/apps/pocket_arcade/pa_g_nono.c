/* 数织 —— 七乘七的数字填色,一共三十六张手绘小图。边上的数字是那一行或
   那一列里连续涂几格,按顺序排列。只要满足所有数字就算过,不必和原图
   一模一样;涂完之后会告诉你画的是什么。 */
#include "pa_game.h"
#include "pa_picture.h"
#include <string.h>

enum {
    CELL = 15, GRID_X = 48, GRID_Y = 50, CLUE_STEP = 12, CLEAR_MS = 1600,
};

static bool filled(const uint8_t *bits, unsigned x, unsigned y)
{
    return (bits[y] >> x) & 1U;
}

/* 把一条线上的连续段长度依次写进 out。图案包保证不会超过三段。 */
static void runs_of(uint8_t line, uint8_t *out)
{
    unsigned index = 0, run = 0;
    memset(out, 0, PA_NONO_CLUES);
    for (unsigned i = 0; i < PA_NONO; i++) {
        if ((line >> i) & 1U) { run++; continue; }
        if (run && index < PA_NONO_CLUES) out[index++] = (uint8_t)run;
        run = 0;
    }
    if (run && index < PA_NONO_CLUES) out[index] = (uint8_t)run;
}

static uint8_t column_bits(const uint8_t *rows, unsigned x)
{
    uint8_t line = 0;
    for (unsigned y = 0; y < PA_NONO; y++)
        if (filled(rows, x, y)) line |= (uint8_t)(1U << y);
    return line;
}

static bool matches(const pa_nono_t *n)
{
    uint8_t got[PA_NONO_CLUES];
    for (unsigned y = 0; y < PA_NONO; y++) {
        runs_of(n->fill[y], got);
        if (memcmp(got, n->row_clue[y], PA_NONO_CLUES) != 0) return false;
    }
    for (unsigned x = 0; x < PA_NONO; x++) {
        runs_of(column_bits(n->fill, x), got);
        if (memcmp(got, n->col_clue[x], PA_NONO_CLUES) != 0) return false;
    }
    return true;
}

static void new_board(pa_run_t *run)
{
    pa_nono_t *n = &run->u.nono;
    uint8_t next = (uint8_t)pa_below(&run->rng, PA_PICTURE_COUNT);
    if (next == n->picture) next = (uint8_t)((next + 1U) % PA_PICTURE_COUNT);
    n->picture = next;
    unsigned marked = 0;
    for (unsigned y = 0; y < PA_NONO; y++) {
        n->target[y] = (uint8_t)(PA_PICTURE[n->picture].row[y] & 0x7FU);
        for (unsigned x = 0; x < PA_NONO; x++) marked += filled(n->target, x, y) ? 1U : 0U;
    }
    for (unsigned y = 0; y < PA_NONO; y++) runs_of(n->target[y], n->row_clue[y]);
    for (unsigned x = 0; x < PA_NONO; x++) runs_of(column_bits(n->target, x), n->col_clue[x]);
    memset(n->fill, 0, sizeof(n->fill));
    memset(n->cross, 0, sizeof(n->cross));
    n->cx = n->cy = 0;
    n->paints = 0;
    /* 只有涂格算次数,打叉不算:做记号本来就该是免费的。 */
    n->budget = (uint16_t)(marked * 2U + 16U);
}

static void reset(pa_run_t *run)
{
    pa_nono_t *n = &run->u.nono;
    memset(n, 0, sizeof(*n));
    n->picture = (uint8_t)PA_PICTURE_COUNT;
    n->board = 1;
    new_board(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_nono_t *n = &run->u.nono;
    if (!n->clear_ms) return;
    n->clear_ms = (uint16_t)(n->clear_ms > ms ? n->clear_ms - ms : 0);
    if (n->clear_ms) return;
    if (n->board >= PA_NONO_BOARDS) {
        run->over = true;
        run->note = "八张图都填完了";
        return;
    }
    n->board++;
    new_board(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_nono_t *n = &run->u.nono;
    if (n->clear_ms) return;
    if (pressed == PA_KEY_UP) { n->cy = (uint8_t)((n->cy + 1U) % PA_NONO); return; }
    if (pressed == PA_KEY_DOWN) { n->cx = (uint8_t)((n->cx + 1U) % PA_NONO); return; }
    uint8_t bit = (uint8_t)(1U << n->cx);
    if (pressed == PA_KEY_OK2) {
        n->fill[n->cy] = (uint8_t)(n->fill[n->cy] & ~bit);
        n->cross[n->cy] ^= bit;
        return;
    }
    n->cross[n->cy] = (uint8_t)(n->cross[n->cy] & ~bit);
    n->fill[n->cy] ^= bit;
    n->paints++;
    if (matches(n)) {
        long spare = (long)n->budget - (long)n->paints;
        run->score += 200 + (long)n->board * 20 + (spare > 0 ? spare * 10 : 0);
        n->clear_ms = CLEAR_MS;
        return;
    }
    if (n->paints < n->budget) return;
    run->over = true;
    run->note = "涂的次数用完了";
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_nono_t *n = &run->u.nono;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 张   还能涂 %u 次", (unsigned)n->board,
             (unsigned)(n->budget > n->paints ? n->budget - n->paints : 0));
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0xE7E2D6, 4);
    pa_frect(scene, GRID_X - 3, GRID_Y - 3, PA_NONO * CELL + 6, PA_NONO * CELL + 6,
             0x4A5A6B, 3);

    for (unsigned y = 0; y < PA_NONO; y++)
        for (unsigned k = 0; k < PA_NONO_CLUES; k++) {
            if (!n->row_clue[y][k]) continue;
            pa_num(scene, (int)k * CLUE_STEP + 6, PA_FIELD_Y + GRID_Y + (int)y * CELL,
                   CLUE_STEP, (long)n->row_clue[y][k], PA_INK, PA_FONT_NUM14, PA_CENTER);
        }
    for (unsigned x = 0; x < PA_NONO; x++)
        for (unsigned k = 0; k < PA_NONO_CLUES; k++) {
            if (!n->col_clue[x][k]) continue;
            pa_num(scene, GRID_X + (int)x * CELL, PA_FIELD_Y + 6 + (int)k * CLUE_STEP,
                   CELL, (long)n->col_clue[x][k], PA_INK, PA_FONT_NUM14, PA_CENTER);
        }

    for (unsigned y = 0; y < PA_NONO; y++)
        for (unsigned x = 0; x < PA_NONO; x++) {
            int cx = GRID_X + (int)x * CELL, cy = GRID_Y + (int)y * CELL;
            bool here = (!n->clear_ms && x == n->cx && y == n->cy);
            bool on = filled(n->fill, x, y);
            pa_frect(scene, cx + 1, cy + 1, CELL - 2, CELL - 2,
                     on ? PA_INK : here ? PA_YELLOW : PA_PAPER, 2);
            if (!filled(n->cross, x, y)) continue;
            pa_frect(scene, cx + 5, cy + 6, CELL - 10, 2, PA_RED, 0);
            pa_frect(scene, cx + 7, cy + 4, 2, CELL - 8, PA_RED, 0);
        }

    if (n->clear_ms)
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_GREEN, PA_FONT_ZH, PA_CENTER,
                 "填出来了，这是%s", PA_PICTURE[n->picture].name);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   数字是连续几格", run->score);
    pa_footer(scene, "上换行　下换列　确定涂格");
}

const pa_game_t pa_game_nono = {
    .id = 24,
    .name = "数织", .genre = "推理",
    .hint = "按数字涂出图案",
    .rule = {"边上的数字是那一行连续涂几格",
             "上换行，下换列，确定涂或取消",
             "双击打叉做记号不算次数，八张图"},
    .keys = "上换行　下换列　确定涂格",
    .ok_mode = PA_OK_CLICK,
    .star = {900, 1800, 3000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
