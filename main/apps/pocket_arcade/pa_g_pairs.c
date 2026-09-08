/* 记忆翻牌 —— 四乘四配对。和扫雷一样,上键换行、下键换列当二维光标用。 */
#include "pa_game.h"
#include <string.h>

enum {
    TILE = 34, GAP = 3, BOARD_X = 26, BOARD_Y = 7,
    PAIRS = 8, BOARDS = 3, FLIP_MS = 900, CLEAR_MS = 1100,
};

static const char *const FACE[PAIRS] = {"花", "草", "木", "石", "水", "火", "山", "云"};
static const uint32_t FACE_COLOUR[PAIRS] = {
    PA_PINK, PA_GRASS, PA_GREEN, PA_GRAY, PA_BLUE, PA_RED, 0x8A5A2B, PA_SLATE
};

static void new_board(pa_run_t *run)
{
    pa_pairs_t *p = &run->u.pairs;
    for (uint8_t i = 0; i < 16; i++) {
        p->face[i] = (uint8_t)(i / 2);
        p->state[i] = 0;
    }
    for (uint8_t i = 15; i > 0; i--) {
        uint8_t j = (uint8_t)pa_below(&run->rng, i + 1U);
        uint8_t swap = p->face[i];
        p->face[i] = p->face[j];
        p->face[j] = swap;
    }
    p->cx = p->cy = 0;
    p->first = p->second = -1;
    p->matched = 0;
    p->moves = 0;
    p->flip_ms = 0;
}

static void reset(pa_run_t *run)
{
    pa_pairs_t *p = &run->u.pairs;
    memset(p, 0, sizeof(*p));
    p->board = 1;
    new_board(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_pairs_t *p = &run->u.pairs;
    if (p->flip_ms) {
        p->flip_ms = (uint16_t)(p->flip_ms > ms ? p->flip_ms - ms : 0);
        if (!p->flip_ms && p->first >= 0 && p->second >= 0) {
            p->state[p->first] = 0;
            p->state[p->second] = 0;
            p->first = p->second = -1;
        }
        return;
    }
    if (!p->clear_ms) return;
    p->clear_ms = (uint16_t)(p->clear_ms > ms ? p->clear_ms - ms : 0);
    if (p->clear_ms) return;
    if (p->board >= BOARDS) {
        run->over = true;
        run->note = "三副牌都配完了";
        return;
    }
    p->board++;
    new_board(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_pairs_t *p = &run->u.pairs;
    if (p->flip_ms || p->clear_ms) return;
    if (pressed == PA_KEY_UP) {
        p->cy = (uint8_t)((p->cy + 1U) % 4U);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        p->cx = (uint8_t)((p->cx + 1U) % 4U);
        return;
    }
    int8_t index = (int8_t)(p->cy * 4U + p->cx);
    if (p->state[index]) return;
    p->state[index] = 1;
    if (p->first < 0) { p->first = index; return; }
    p->second = index;
    p->moves++;
    if (p->face[p->first] != p->face[p->second]) {
        p->flip_ms = FLIP_MS;
        return;
    }
    p->state[p->first] = 2;
    p->state[p->second] = 2;
    p->first = p->second = -1;
    p->matched++;
    run->score += 40;
    if (p->matched < PAIRS) return;
    long bonus = 200 - (long)p->moves * 6;
    run->score += 100 + (bonus > 0 ? bonus : 0);
    p->clear_ms = CLEAR_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_pairs_t *p = &run->u.pairs;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 副   翻了 %u 次", (unsigned)p->board, (unsigned)p->moves);
    pa_frect(scene, BOARD_X - 4, BOARD_Y - 4, 4 * TILE + 3 * GAP + 8,
             4 * TILE + 3 * GAP + 8, 0x4A5A6B, 5);
    for (uint8_t i = 0; i < 16; i++) {
        int x = BOARD_X + (i % 4) * (TILE + GAP);
        int y = BOARD_Y + (i / 4) * (TILE + GAP);
        bool here = (!p->clear_ms && i == p->cy * 4U + p->cx);
        if (p->state[i] == 0) {
            pa_frect(scene, x, y, TILE, TILE, here ? PA_YELLOW : 0x8FA3B8, 4);
            pa_frect(scene, x + 8, y + 8, TILE - 16, TILE - 16,
                     here ? 0xFFE98A : 0x7B8FA6, 3);
            continue;
        }
        pa_frect(scene, x, y, TILE, TILE, p->state[i] == 2 ? 0xD9E7D0 : PA_PAPER, 4);
        if (here) pa_frect(scene, x, y, TILE, 3, PA_YELLOW, 0);
        pa_text(scene, x, PA_FIELD_Y + y + 9, TILE, FACE[p->face[i]],
                FACE_COLOUR[p->face[i]], PA_FONT_ZH, PA_CENTER);
    }
    if (p->clear_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "这副全配上了", PA_GREEN,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   还差 %u 对", run->score, (unsigned)(PAIRS - p->matched));
    pa_footer(scene, "上换行　下换列　确定翻牌");
}

const pa_game_t pa_game_pairs = {
    .id = 14,
    .name = "记忆翻牌", .genre = "记忆",
    .hint = "上换行下换列翻牌",
    .rule = {"上键换一行，下键换一列",
             "确定翻开，两张一样就留下",
             "翻的次数越少分越高，三副牌"},
    .keys = "上换行　下换列　确定翻牌",
    .ok_mode = PA_OK_PRESS,
    .star = {450, 750, 1050},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
