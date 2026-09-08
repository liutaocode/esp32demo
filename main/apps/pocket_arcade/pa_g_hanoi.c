/* 汉诺塔 —— 三根柱子。上下键挪光标,确定键在"拿起"和"放下"之间切换,
   一次只能拿最上面那片,也只能放在更大的片上。 */
#include "pa_game.h"
#include <string.h>

enum {
    PEG_W = 58, PEG_X = 8, BASE_Y = 132, DISK_H = 14,
    START_DISKS = 3, TOWERS = 4, CLEAR_MS = 1200,
};

static const uint32_t DISK_COLOUR[PA_HANOI_MAX] = {
    PA_RED, PA_ORANGE, PA_YELLOW, PA_GRASS, PA_GREEN, PA_BLUE, PA_PURPLE
};

static int peg_centre(unsigned peg) { return PEG_X + (int)peg * (PEG_W + 6) + PEG_W / 2; }

static void new_tower(pa_run_t *run)
{
    pa_hanoi_t *h = &run->u.hanoi;
    memset(h->peg, 0, sizeof(h->peg));
    memset(h->height, 0, sizeof(h->height));
    for (uint8_t i = 0; i < h->disks; i++)
        h->peg[0][i] = (uint8_t)(h->disks - i);   /* 底下最大 */
    h->height[0] = h->disks;
    h->cursor = 0;
    h->held = 0;
    h->moves = 0;
}

static void reset(pa_run_t *run)
{
    pa_hanoi_t *h = &run->u.hanoi;
    memset(h, 0, sizeof(*h));
    h->disks = START_DISKS;
    new_tower(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_hanoi_t *h = &run->u.hanoi;
    if (!h->clear_ms) return;
    h->clear_ms = (uint16_t)(h->clear_ms > ms ? h->clear_ms - ms : 0);
    if (h->clear_ms) return;
    if (h->cleared >= TOWERS) {
        run->over = true;
        run->note = "四座塔都搬完了";
        return;
    }
    h->disks++;
    new_tower(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_hanoi_t *h = &run->u.hanoi;
    if (h->clear_ms) return;
    if (pressed == PA_KEY_UP) {
        h->cursor = (uint8_t)((h->cursor + 2U) % 3U);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        h->cursor = (uint8_t)((h->cursor + 1U) % 3U);
        return;
    }
    if (!h->held) {
        if (!h->height[h->cursor]) return;
        h->held = h->peg[h->cursor][h->height[h->cursor] - 1U];
        h->height[h->cursor]--;
        return;
    }
    uint8_t top = h->height[h->cursor] ? h->peg[h->cursor][h->height[h->cursor] - 1U] : 255;
    if (h->held > top) return;                 /* 大片不能压在小片上 */
    h->peg[h->cursor][h->height[h->cursor]++] = h->held;
    h->held = 0;
    h->moves++;
    if (h->height[2] != h->disks) return;

    /* 最少步数是 2 的 n 次方减一,给出四倍的宽容度再按步数扣分。 */
    unsigned optimal = (1U << h->disks) - 1U;
    long bonus = (long)optimal * 4 - (long)h->moves * 2;
    run->score += 120 + (long)h->disks * 20 + (bonus > 0 ? bonus : 0);
    h->cleared++;
    h->clear_ms = CLEAR_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_hanoi_t *h = &run->u.hanoi;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "%u 片   走了 %u 步", (unsigned)h->disks, (unsigned)h->moves);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0xF0E6D2, 4);
    for (unsigned peg = 0; peg < 3; peg++) {
        int centre = peg_centre(peg);
        bool here = (peg == h->cursor);
        pa_frect(scene, centre - PEG_W / 2, BASE_Y, PEG_W, 8,
                 here ? PA_ORANGE : 0x8A6B4B, 2);
        pa_frect(scene, centre - 3, BASE_Y - 106, 6, 106, 0x8A6B4B, 2);
        for (uint8_t i = 0; i < h->height[peg]; i++) {
            uint8_t size = h->peg[peg][i];
            int width = 16 + (int)size * 6;
            pa_frect(scene, centre - width / 2, BASE_Y - (int)(i + 1) * DISK_H,
                     width, DISK_H - 2, DISK_COLOUR[(size - 1) % PA_HANOI_MAX], 3);
        }
        if (here && !h->held)
            pa_frect(scene, centre - 5, BASE_Y + 12, 10, 8, PA_RED, 2);
    }
    if (h->held) {
        int centre = peg_centre(h->cursor);
        int width = 16 + (int)h->held * 6;
        pa_frect(scene, centre - width / 2, 22, width, DISK_H - 2,
                 DISK_COLOUR[(h->held - 1) % PA_HANOI_MAX], 3);
        pa_frect(scene, centre - 2, 22 + DISK_H, 4, 10, PA_RED, 0);
    }

    if (h->clear_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "这座塔搬完了", PA_GREEN,
                PA_FONT_ZH, PA_CENTER);
    else if (h->held)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "手上有一片，找根柱子放下",
                PA_ORANGE, PA_FONT_ZH, PA_CENTER);
    else
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "全部搬到最右边那根",
                PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "上下选柱子　确定拿起或放下");
}

const pa_game_t pa_game_hanoi = {
    .id = 19,
    .name = "汉诺塔", .genre = "益智",
    .hint = "拿起放下，搬到最右",
    .rule = {"上下键选柱子，确定键拿起或放下",
             "一次只能动最上面一片",
             "大片不能压在小片上，四座塔连打"},
    .keys = "上下选柱　确定拿放",
    .ok_mode = PA_OK_PRESS,
    .star = {300, 700, 1300},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
