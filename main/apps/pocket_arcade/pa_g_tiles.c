/* 三键节拍 —— 三条横轨正好对上三个键:上键打上轨,确定打中轨,下键打下轨。
   方块从右边滑过来,进入左边的判定区时按对应的键。 */
#include "pa_game.h"
#include <string.h>

enum {
    ROW_H = 46, ROW_GAP = 6, ROW_TOP = 6, TILE_W = 30,
    ZONE_X = PA_TILE_HIT_X, ZONE_W = PA_TILE_HIT_W,
    SPEED_START = 112, SPEED_MAX = 268, GAP_START = 920, GAP_MIN = 380,
};

static int row_y(unsigned row) { return ROW_TOP + (int)row * (ROW_H + ROW_GAP); }

static int tile_px(const pa_tiles_t *t, unsigned index)
{
    return (int)(t->x[index] / 1000);
}

static void reset(pa_run_t *run)
{
    pa_tiles_t *t = &run->u.tiles;
    memset(t, 0, sizeof(*t));
    t->speed = SPEED_START;
    t->gap_ms = GAP_START;
    t->lives = 3;
    t->flash_row = -1;
    t->spawn_ms = GAP_START;   /* 开局立刻来第一块 */
}

static void lose_life(pa_run_t *run, int row, const char *reason)
{
    pa_tiles_t *t = &run->u.tiles;
    t->combo = 0;
    t->flash_row = (int8_t)row;
    t->flash_good = false;
    t->flash_ms = 320;
    if (t->lives) t->lives--;
    if (!t->lives) {
        run->over = true;
        run->note = reason;
    }
}

static void spawn(pa_run_t *run)
{
    pa_tiles_t *t = &run->u.tiles;
    unsigned slot = PA_TILE_MAX;
    for (unsigned i = 0; i < PA_TILE_MAX; i++)
        if (!t->live[i]) { slot = i; break; }
    if (slot == PA_TILE_MAX) return;

    unsigned row = pa_below(&run->rng, PA_TILE_ROWS);
    for (unsigned attempt = 0; attempt < PA_TILE_ROWS; attempt++) {
        bool crowded = false;
        for (unsigned i = 0; i < PA_TILE_MAX; i++)
            if (t->live[i] && t->row[i] == row && tile_px(t, i) > PA_FIELD_W - 62)
                crowded = true;
        if (!crowded) break;
        row = (row + 1U) % PA_TILE_ROWS;
    }
    t->x[slot] = (int32_t)PA_FIELD_W * 1000;
    t->row[slot] = (uint8_t)row;
    t->kind[slot] = (uint8_t)(pa_below(&run->rng, 6) == 0);
    t->live[slot] = 1;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_tiles_t *t = &run->u.tiles;
    if (t->flash_ms) t->flash_ms = (uint16_t)(t->flash_ms > ms ? t->flash_ms - ms : 0);
    t->spawn_ms += ms;
    if (t->spawn_ms >= t->gap_ms) {
        t->spawn_ms -= t->gap_ms;
        spawn(run);
    }
    for (unsigned i = 0; i < PA_TILE_MAX && !run->over; i++) {
        if (!t->live[i]) continue;
        t->x[i] -= (int32_t)t->speed * (int32_t)ms;
        if (tile_px(t, i) + TILE_W >= 0) continue;
        t->live[i] = 0;
        lose_life(run, t->row[i], "漏掉了三块");
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_tiles_t *t = &run->u.tiles;
    unsigned row = pa_row_of_key(pressed);
    int found = -1, best_x = 0;
    for (unsigned i = 0; i < PA_TILE_MAX; i++) {
        if (!t->live[i] || t->row[i] != row) continue;
        int px = tile_px(t, i);
        if (found < 0 || px < best_x) { found = (int)i; best_x = px; }
    }
    if (found < 0 || best_x > ZONE_X + ZONE_W) {
        t->combo = 0;
        t->flash_row = (int8_t)row;
        t->flash_good = false;
        t->flash_ms = 220;
        return;
    }
    t->live[found] = 0;
    t->combo++;
    if (t->combo > t->best_combo) t->best_combo = t->combo;
    long base = t->kind[found] ? 25 : 10;
    int centre = ZONE_X + (ZONE_W - TILE_W) / 2;
    bool perfect = (best_x >= centre - 6 && best_x <= centre + 6);
    run->score += base + (long)(t->combo / 5) + (perfect ? 5 : 0);
    if (t->speed < SPEED_MAX) t->speed += 2;
    if (t->gap_ms > GAP_MIN + 8) t->gap_ms -= 8;
    t->flash_row = (int8_t)row;
    t->flash_good = true;
    t->flash_ms = 220;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_tiles_t *t = &run->u.tiles;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   连击 %u", run->score, (unsigned)t->combo);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x1B2733, 4);

    for (unsigned row = 0; row < PA_TILE_ROWS; row++) {
        int y = row_y(row);
        bool lit = (t->flash_ms && t->flash_row == (int8_t)row);
        pa_frect(scene, 0, y, PA_FIELD_W, ROW_H,
                 lit ? (t->flash_good ? 0x1F4F45 : 0x5A2320) : 0x27364A, 3);
        pa_frect(scene, ZONE_X, y, ZONE_W, ROW_H, 0x35506E, 3);
        pa_frect(scene, 2, y + ROW_H / 2 - 10, 20, 20, PA_CREAM, 3);
        pa_text(scene, 2, PA_FIELD_Y + y + ROW_H / 2 - 9, 20, PA_KEY_LABEL[row],
                PA_INK, PA_FONT_ZH, PA_CENTER);
    }

    for (unsigned i = 0; i < PA_TILE_MAX; i++) {
        if (!t->live[i]) continue;
        int y = row_y(t->row[i]);
        pa_frect(scene, tile_px(t, i), y + 6, TILE_W, ROW_H - 12,
                 t->kind[i] ? PA_YELLOW : PA_PAPER, 3);
    }

    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
             t->lives > 1 ? PA_INK : PA_RED, PA_FONT_ZH, PA_CENTER,
             "剩 %u 条命   最高连击 %u", (unsigned)t->lives, (unsigned)t->best_combo);
    pa_footer(scene, "上轨上键　中轨下键　下轨确定");
}

const pa_game_t pa_game_tiles = {
    .id = 3,
    .name = "三键节拍", .genre = "音游",
    .hint = "三条轨对三个键",
    .rule = {"上键上轨，下键中轨，确定下轨",
             "方块滑到左边的判定框时按下去",
             "漏三块就结束，黄块值两倍半"},
    .keys = "上中下轨对应三个键",
    .ok_mode = PA_OK_PRESS,
    .star = {400, 900, 1800},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
