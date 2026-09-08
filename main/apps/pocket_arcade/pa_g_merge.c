/* 数字合并 —— 四阶的合并方块。三个键要覆盖四个方向,
   所以上下两键直接对应上下,确定键单击向左、双击向右。 */
#include "pa_game.h"
#include <string.h>

enum { TILE = 35, GAP = 3, BOARD_X = 24, BOARD_Y = 5 };

static const uint32_t TILE_COLOR[12] = {
    0xCDC1B4, 0xEEE4DA, 0xEDE0C8, 0xF2B179, 0xF59563, 0xF67C5F,
    0xF65E3B, 0xEDCF72, 0xEDCC61, 0xEDC850, 0xEDC53F, 0xEDC22E
};

static uint8_t *at(pa_merge_t *m, int dir, int line, int idx)
{
    switch (dir) {
    case 0:  return &m->cell[line][idx];        /* 向左收拢 */
    case 1:  return &m->cell[line][3 - idx];    /* 向右收拢 */
    case 2:  return &m->cell[idx][line];        /* 向上收拢 */
    default: return &m->cell[3 - idx][line];    /* 向下收拢 */
    }
}

static void spawn(pa_run_t *run)
{
    pa_merge_t *m = &run->u.merge;
    unsigned empty = 0;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (!m->cell[y][x]) empty++;
    if (!empty) return;
    unsigned pick = pa_below(&run->rng, empty);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            if (m->cell[y][x]) continue;
            if (pick--) continue;
            m->cell[y][x] = (uint8_t)(pa_below(&run->rng, 10) ? 1 : 2);
            return;
        }
}

static bool stuck(const pa_merge_t *m)
{
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            if (!m->cell[y][x]) return false;
            if (x < 3 && m->cell[y][x] == m->cell[y][x + 1]) return false;
            if (y < 3 && m->cell[y][x] == m->cell[y + 1][x]) return false;
        }
    return true;
}

static void reset(pa_run_t *run)
{
    pa_merge_t *m = &run->u.merge;
    memset(m, 0, sizeof(*m));
    spawn(run);
    spawn(run);
}

static void slide(pa_run_t *run, int dir)
{
    pa_merge_t *m = &run->u.merge;
    bool moved = false;
    for (int line = 0; line < 4; line++) {
        uint8_t packed[4] = {0, 0, 0, 0};
        uint8_t count = 0;
        for (int idx = 0; idx < 4; idx++) {
            uint8_t value = *at(m, dir, line, idx);
            if (value) packed[count++] = value;
        }
        uint8_t merged[4] = {0, 0, 0, 0};
        uint8_t out = 0;
        for (uint8_t i = 0; i < count; i++) {
            if (i + 1 < count && packed[i] == packed[i + 1]) {
                uint8_t value = (uint8_t)(packed[i] + 1);
                merged[out++] = value;
                run->score += 1L << value;
                if (value > m->top) m->top = value;
                if (value >= 11) m->reached = true;
                i++;
            } else {
                merged[out++] = packed[i];
                if (packed[i] > m->top) m->top = packed[i];
            }
        }
        for (int idx = 0; idx < 4; idx++) {
            uint8_t *slot = at(m, dir, line, idx);
            if (*slot != merged[idx]) moved = true;
            *slot = merged[idx];
        }
    }
    if (!moved) return;
    m->moves++;
    spawn(run);
    if (stuck(m)) {
        run->over = true;
        run->note = "没有能合的方块了";
    }
}

static void tick(pa_run_t *run, uint32_t ms)
{
    (void)run;
    (void)ms;   /* 回合制,不随时间推进 */
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    switch (pressed) {
    case PA_KEY_UP:   slide(run, 2); break;
    case PA_KEY_DOWN: slide(run, 3); break;
    case PA_KEY_OK:   slide(run, 0); break;
    default:          slide(run, 1); break;
    }
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_merge_t *m = &run->u.merge;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   步数 %u", run->score, (unsigned)m->moves);
    pa_frect(scene, BOARD_X - 4, BOARD_Y - 4, 4 * TILE + 3 * GAP + 8,
             4 * TILE + 3 * GAP + 8, 0xBBADA0, 6);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            uint8_t exponent = m->cell[y][x];
            int tx = BOARD_X + x * (TILE + GAP);
            int ty = BOARD_Y + y * (TILE + GAP);
            pa_frect(scene, tx, ty, TILE, TILE,
                     TILE_COLOR[exponent > 11 ? 11 : exponent], 4);
            if (!exponent) continue;
            pa_num(scene, tx, PA_FIELD_Y + ty + 10, TILE, 1L << exponent,
                   exponent >= 3 ? PA_WHITE : 0x776E65, PA_FONT_NUM14, PA_CENTER);
        }
    if (m->reached)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "已经合出两千零四十八",
                PA_GREEN, PA_FONT_ZH, PA_CENTER);
    else
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "大数留在角落，别打散",
                PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "上下移动　确定左移　双击右移");
}

const pa_game_t pa_game_merge = {
    .id = 17,
    .name = "数字合并", .genre = "益智",
    .hint = "撞在一起就翻倍",
    .rule = {"上键上移，下键下移",
             "确定键单击左移，双击右移",
             "把最大的数字养在一个角上"},
    .keys = "上下移动　确定左右移",
    .ok_mode = PA_OK_CLICK,
    .star = {1500, 4000, 9000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
