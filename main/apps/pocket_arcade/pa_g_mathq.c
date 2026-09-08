/* 快速判断 —— 六十秒里判断算式对不对。上键说对,下键说错,
   确定键跳过。连对越多每题越值钱。 */
#include "pa_game.h"
#include <stdio.h>
#include <string.h>

enum { ROUND_MS = 60000, FLASH_MS = 320 };

static const char *const OP_SIGN[3] = {"+", "-", "×"};

static void next_question(pa_run_t *run)
{
    pa_mathq_t *m = &run->u.mathq;
    unsigned hard = (unsigned)(m->hits / 6U);
    if (hard > 4U) hard = 4U;
    m->op = (uint8_t)pa_below(&run->rng, 3);
    unsigned answer;
    if (m->op == 2) {
        m->left = (uint16_t)(2U + pa_below(&run->rng, 7U + hard * 2U));
        m->right = (uint16_t)(2U + pa_below(&run->rng, 7U + hard * 2U));
        answer = (unsigned)m->left * m->right;
    } else if (m->op == 0) {
        m->left = (uint16_t)(5U + pa_below(&run->rng, 40U + hard * 20U));
        m->right = (uint16_t)(5U + pa_below(&run->rng, 40U + hard * 20U));
        answer = (unsigned)m->left + m->right;
    } else {
        m->left = (uint16_t)(20U + pa_below(&run->rng, 60U + hard * 20U));
        m->right = (uint16_t)(2U + pa_below(&run->rng, (unsigned)m->left - 2U));
        answer = (unsigned)m->left - m->right;
    }
    /* 一半的题是错的,错法偏小,才需要真算一遍。 */
    m->correct = (uint8_t)(pa_below(&run->rng, 2) == 0);
    if (m->correct) {
        m->shown = (uint16_t)answer;
        return;
    }
    unsigned slip = 1U + pa_below(&run->rng, 3U + hard);
    if (pa_below(&run->rng, 2) && answer > slip) m->shown = (uint16_t)(answer - slip);
    else m->shown = (uint16_t)(answer + slip);
}

static void reset(pa_run_t *run)
{
    pa_mathq_t *m = &run->u.mathq;
    memset(m, 0, sizeof(*m));
    m->time_ms = ROUND_MS;
    m->flash = -1;
    next_question(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_mathq_t *m = &run->u.mathq;
    if (m->flash_ms) m->flash_ms = (uint16_t)(m->flash_ms > ms ? m->flash_ms - ms : 0);
    m->time_ms = (m->time_ms > ms) ? m->time_ms - ms : 0;
    if (m->time_ms) return;
    run->over = true;
    run->note = "六十秒到了";
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_mathq_t *m = &run->u.mathq;
    if (pressed == PA_KEY_OK || pressed == PA_KEY_OK2) {
        m->combo = 0;
        m->flash = 0;
        m->flash_ms = FLASH_MS;
        next_question(run);
        return;
    }
    bool said_yes = (pressed == PA_KEY_UP);
    if (said_yes == (m->correct != 0)) {
        m->hits++;
        m->combo++;
        if (m->combo > m->best_combo) m->best_combo = m->combo;
        run->score += 10 + (long)(m->combo / 3);
        m->flash = 1;
    } else {
        m->misses++;
        m->combo = 0;
        run->score -= 8;
        if (run->score < 0) run->score = 0;
        m->flash = -1;
    }
    m->flash_ms = FLASH_MS;
    next_question(run);
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_mathq_t *m = &run->u.mathq;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   剩 %u 秒", run->score, (unsigned)((m->time_ms + 999U) / 1000U));
    uint32_t field = 0x24313F;
    if (m->flash_ms && m->flash > 0) field = 0x1F5B45;
    else if (m->flash_ms && m->flash < 0) field = 0x5B2A24;
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, field, 4);

    char question[24];
    snprintf(question, sizeof(question), "%u %s %u = %u", (unsigned)m->left,
             OP_SIGN[m->op], (unsigned)m->right, (unsigned)m->shown);
    pa_text(scene, 4, PA_FIELD_Y + 48, PA_FIELD_W - 8, question, PA_WHITE,
            PA_FONT_ZH, PA_CENTER);

    pa_frect(scene, 16, 96, 74, 34, 0x2E7D5B, 4);
    pa_text(scene, 16, PA_FIELD_Y + 104, 74, "上：对", PA_WHITE, PA_FONT_ZH, PA_CENTER);
    pa_frect(scene, 108, 96, 74, 34, 0x9E3B30, 4);
    pa_text(scene, 108, PA_FIELD_Y + 104, 74, "下：错", PA_WHITE, PA_FONT_ZH, PA_CENTER);

    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "连对 %u   最高 %u", (unsigned)m->combo, (unsigned)m->best_combo);
    pa_footer(scene, "上对　下错　确定跳过");
}

const pa_game_t pa_game_mathq = {
    .id = 15,
    .name = "快速判断", .genre = "心算",
    .hint = "上说对下说错",
    .rule = {"上键说这道算式对，下键说错",
             "确定键跳过，但会断掉连对",
             "六十秒一局，连对越多每题越值钱"},
    .keys = "上对　下错　确定跳过",
    .ok_mode = PA_OK_PRESS,
    .star = {300, 600, 1000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
