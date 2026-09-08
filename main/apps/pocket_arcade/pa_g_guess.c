/* 猜数字 —— 四位不重复的密码。上下键改当前位,确定键换到下一位,
   在最后一位按确定就提交。A 是位置和数字都对,B 是数字对位置不对。 */
#include "pa_game.h"
#include <stdio.h>
#include <string.h>

enum { SLOT_W = 30, SLOT_GAP = 6, ENTRY_Y = 136, ROW_H = 16, CLEAR_MS = 1200 };

static void new_secret(pa_run_t *run)
{
    pa_guess_t *g = &run->u.guess;
    uint8_t pool[10];
    for (uint8_t i = 0; i < 10; i++) pool[i] = i;
    for (uint8_t i = 9; i > 0; i--) {
        uint8_t j = (uint8_t)pa_below(&run->rng, i + 1U);
        uint8_t swap = pool[i];
        pool[i] = pool[j];
        pool[j] = swap;
    }
    for (uint8_t i = 0; i < PA_GUESS_LEN; i++) g->secret[i] = pool[i];
    memset(g->entry, 0, sizeof(g->entry));
    memset(g->guess, 0, sizeof(g->guess));
    memset(g->bulls, 0, sizeof(g->bulls));
    memset(g->cows, 0, sizeof(g->cows));
    g->digit = 0;
    g->tries = 0;
    g->solved = false;
}

static void reset(pa_run_t *run)
{
    pa_guess_t *g = &run->u.guess;
    memset(g, 0, sizeof(*g));
    g->round = 1;
    new_secret(run);
}

static void judge(pa_run_t *run)
{
    pa_guess_t *g = &run->u.guess;
    uint8_t left[10] = {0}, mine[10] = {0};
    uint8_t bulls = 0, cows = 0;
    for (uint8_t i = 0; i < PA_GUESS_LEN; i++) {
        if (g->secret[i] == g->entry[i]) { bulls++; continue; }
        left[g->secret[i]]++;
        mine[g->entry[i]]++;
    }
    for (uint8_t d = 0; d < 10; d++) cows = (uint8_t)(cows + (left[d] < mine[d] ? left[d] : mine[d]));

    memcpy(g->guess[g->tries], g->entry, PA_GUESS_LEN);
    g->bulls[g->tries] = bulls;
    g->cows[g->tries] = cows;
    g->tries++;
    g->digit = 0;
    if (bulls == PA_GUESS_LEN) {
        g->solved = true;
        run->score += 300 + (long)(PA_GUESS_TRIES - g->tries) * 60;
        g->clear_ms = CLEAR_MS;
        return;
    }
    if (g->tries < PA_GUESS_TRIES) return;
    run->over = true;
    run->note = "八次没猜出来";
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_guess_t *g = &run->u.guess;
    if (!g->clear_ms) return;
    g->clear_ms = (uint16_t)(g->clear_ms > ms ? g->clear_ms - ms : 0);
    if (g->clear_ms) return;
    g->round++;
    new_secret(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_guess_t *g = &run->u.guess;
    if (g->clear_ms) return;
    if (pressed == PA_KEY_UP) {
        g->entry[g->digit] = (uint8_t)((g->entry[g->digit] + 1U) % 10U);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        g->entry[g->digit] = (uint8_t)((g->entry[g->digit] + 9U) % 10U);
        return;
    }
    if (g->digit + 1U < PA_GUESS_LEN) { g->digit++; return; }
    judge(run);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_guess_t *g = &run->u.guess;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 关   还有 %u 次", (unsigned)g->round,
             (unsigned)(PA_GUESS_TRIES - g->tries));
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x1E2A38, 4);

    for (uint8_t i = 0; i < PA_GUESS_TRIES; i++) {
        int y = 4 + (int)i * ROW_H;
        if (i >= g->tries) {
            pa_frect(scene, 30, y + 6, 138, 1, 0x33455A, 0);
            continue;
        }
        char digits[PA_GUESS_LEN + 1];
        for (uint8_t d = 0; d < PA_GUESS_LEN; d++)
            digits[d] = (char)('0' + g->guess[i][d]);
        digits[PA_GUESS_LEN] = '\0';
        pa_text(scene, 30, PA_FIELD_Y + y, 60, digits, PA_CREAM, PA_FONT_NUM14, PA_LEFT);
        char verdict[12];
        snprintf(verdict, sizeof(verdict), "%uA %uB", (unsigned)g->bulls[i],
                 (unsigned)g->cows[i]);
        pa_text(scene, 100, PA_FIELD_Y + y, 68, verdict,
                g->bulls[i] == PA_GUESS_LEN ? PA_GREEN : PA_YELLOW, PA_FONT_NUM14, PA_LEFT);
    }

    int start = (PA_FIELD_W - (PA_GUESS_LEN * SLOT_W + (PA_GUESS_LEN - 1) * SLOT_GAP)) / 2;
    for (uint8_t d = 0; d < PA_GUESS_LEN; d++) {
        int x = start + (int)d * (SLOT_W + SLOT_GAP);
        bool here = (d == g->digit && !g->clear_ms);
        pa_frect(scene, x, ENTRY_Y, SLOT_W, 20, here ? PA_YELLOW : PA_PAPER, 3);
        pa_num(scene, x, PA_FIELD_Y + ENTRY_Y + 3, SLOT_W, (long)g->entry[d], PA_INK,
               PA_FONT_NUM14, PA_CENTER);
    }

    if (g->clear_ms)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "猜中了，换一个密码", PA_GREEN,
                PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
                 "得分 %ld   密码四位不重复", run->score);
    pa_footer(scene, "上下改数字　确定换位提交");
}

const pa_game_t pa_game_guess = {
    .id = 25,
    .name = "猜数字", .genre = "推理",
    .hint = "八次机会推出密码",
    .rule = {"上下改当前位，确定换到下一位",
             "在最后一位按确定就提交猜测",
             "A 是数字位置都对，B 是只对数字"},
    .keys = "上下改数字　确定换位",
    .ok_mode = PA_OK_PRESS,
    .star = {400, 900, 1600},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
