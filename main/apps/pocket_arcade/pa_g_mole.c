/* 打地鼠 —— 三个洞正好对上三个键。金地鼠加分,炸弹扣分,
   六十秒一局,越到后面冒头越快、停留越短。 */
#include "pa_game.h"
#include <string.h>

enum {
    HOLE_X = 40, HOLE_W = 118, HOLE_H = 44, HOLE_TOP = 8, HOLE_GAP = 8,
    ROUND_MS = 60000, GAP_START = 900, GAP_MIN = 420,
    LIVE_START = 1150, LIVE_MIN = 560,
};

static int hole_y(unsigned hole) { return HOLE_TOP + (int)hole * (HOLE_H + HOLE_GAP); }

static void reset(pa_run_t *run)
{
    pa_mole_t *m = &run->u.mole;
    memset(m, 0, sizeof(*m));
    m->left_ms = ROUND_MS;
    m->gap_ms = GAP_START;
    m->spawn_ms = GAP_START;
    m->flash_hole = -1;
}

/* 局面越往后越紧:冒头间隔和停留时间都随已过去的时间收窄。 */
static uint16_t live_window(const pa_mole_t *m)
{
    uint32_t gone = ROUND_MS - m->left_ms;
    uint32_t shrink = gone * (LIVE_START - LIVE_MIN) / ROUND_MS;
    return (uint16_t)(LIVE_START - shrink);
}

static void spawn(pa_run_t *run)
{
    pa_mole_t *m = &run->u.mole;
    unsigned empty = 0;
    for (unsigned i = 0; i < 3; i++) if (!m->kind[i]) empty++;
    if (!empty) return;
    unsigned pick = pa_below(&run->rng, empty);
    for (unsigned i = 0; i < 3; i++) {
        if (m->kind[i]) continue;
        if (pick--) continue;
        unsigned roll = pa_below(&run->rng, 100);
        m->kind[i] = (uint8_t)((roll < 60) ? 1 : (roll < 76) ? 2 : 3);
        m->show_ms[i] = live_window(m);
        m->live_ms[i] = m->show_ms[i];
        return;
    }
}

static void award(pa_run_t *run, unsigned hole, int delta)
{
    pa_mole_t *m = &run->u.mole;
    run->score += delta;
    if (run->score < 0) run->score = 0;
    m->flash_hole = (int8_t)hole;
    m->flash_delta = (int16_t)delta;
    m->flash_ms = 420;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_mole_t *m = &run->u.mole;
    if (m->flash_ms) m->flash_ms = (uint16_t)(m->flash_ms > ms ? m->flash_ms - ms : 0);
    m->left_ms = (m->left_ms > ms) ? m->left_ms - ms : 0;
    if (!m->left_ms) {
        run->over = true;
        run->note = "六十秒到了";
        return;
    }
    m->gap_ms = (uint16_t)(GAP_START - (ROUND_MS - m->left_ms) * (GAP_START - GAP_MIN) / ROUND_MS);
    m->spawn_ms += ms;
    if (m->spawn_ms >= m->gap_ms) {
        m->spawn_ms -= m->gap_ms;
        spawn(run);
    }
    for (unsigned i = 0; i < 3; i++) {
        if (!m->kind[i]) continue;
        if (m->live_ms[i] > ms) { m->live_ms[i] = (uint16_t)(m->live_ms[i] - ms); continue; }
        /* 炸弹自己缩回去不算漏,只有普通地鼠跑掉才断连击。 */
        if (m->kind[i] != 3) { m->misses++; m->combo = 0; }
        m->kind[i] = 0;
        m->live_ms[i] = 0;
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_mole_t *m = &run->u.mole;
    unsigned hole = pa_row_of_key(pressed);
    uint8_t kind = m->kind[hole];
    if (!kind) {
        m->combo = 0;
        award(run, hole, 0);
        return;
    }
    m->kind[hole] = 0;
    if (kind == 3) {
        m->combo = 0;
        award(run, hole, -20);
        return;
    }
    m->hits++;
    m->combo++;
    if (m->combo > m->best_combo) m->best_combo = m->combo;
    award(run, hole, (kind == 2 ? 30 : 10) + (int)(m->combo / 3));
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_mole_t *m = &run->u.mole;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   剩 %u 秒", run->score, (unsigned)((m->left_ms + 999U) / 1000U));
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x6E4A2C, 4);

    for (unsigned hole = 0; hole < 3; hole++) {
        int y = hole_y(hole);
        pa_frect(scene, 4, y + HOLE_H / 2 - 11, 26, 22, PA_CREAM, 4);
        pa_text(scene, 4, PA_FIELD_Y + y + HOLE_H / 2 - 10, 26, PA_KEY_LABEL[hole],
                PA_INK, PA_FONT_ZH, PA_CENTER);
        pa_frect(scene, HOLE_X, y, HOLE_W, HOLE_H, 0x4A3220, 12);

        uint8_t kind = m->kind[hole];
        if (kind) {
            /* 冒头的高度按剩余时间走,快缩回去时只露出一点点。 */
            unsigned span = m->show_ms[hole] ? m->show_ms[hole] : 1U;
            unsigned up = m->live_ms[hole] * 26U / span;
            int h = 8 + (int)up;
            uint32_t body = (kind == 3) ? PA_SLATE : (kind == 2) ? PA_YELLOW : 0xB98A5E;
            pa_frect(scene, HOLE_X + 34, y + HOLE_H - h, 50, h, body, 10);
            if (kind == 3) {
                pa_frect(scene, HOLE_X + 55, y + HOLE_H - h - 5, 6, 6, PA_RED, 3);
            } else if (h > 18) {
                pa_frect(scene, HOLE_X + 44, y + HOLE_H - h + 7, 5, 5, PA_INK, 0);
                pa_frect(scene, HOLE_X + 68, y + HOLE_H - h + 7, 5, 5, PA_INK, 0);
            }
        }
        if (m->flash_ms && m->flash_hole == (int8_t)hole && m->flash_delta)
            pa_textf(scene, HOLE_X + HOLE_W - 40, PA_FIELD_Y + y + 4, 38,
                     m->flash_delta > 0 ? PA_GREEN : PA_RED, PA_FONT_NUM14, PA_RIGHT,
                     "%+d", (int)m->flash_delta);
    }

    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "连击 %u   别打黑色炸弹", (unsigned)m->combo);
    pa_footer(scene, "上洞上键　中洞下键　下洞确定");
}

const pa_game_t pa_game_mole = {
    .id = 2,
    .name = "打地鼠", .genre = "反应",
    .hint = "六十秒比手速",
    .rule = {"上键上洞，下键中洞，确定下洞",
             "普通十分，金色三十，炸弹扣二十",
             "连击越长，每一下的加成越高"},
    .keys = "上中下洞对应三个键",
    .ok_mode = PA_OK_PRESS,
    .star = {350, 700, 1200},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
