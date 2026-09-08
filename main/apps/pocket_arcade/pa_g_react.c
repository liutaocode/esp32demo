/* 反应力 —— 红灯变绿就按。抢跑这一轮作废,五轮之后给总成绩。 */
#include "pa_game.h"
#include <string.h>

enum { WAIT_MIN = 1100, WAIT_SPAN = 2600, LATE_MS = 2000, HOLD_MS = 1100 };

static void reset(pa_run_t *run)
{
    pa_react_t *r = &run->u.react;
    memset(r, 0, sizeof(*r));
}

static void record(pa_run_t *run, unsigned ms, bool jumped)
{
    pa_react_t *r = &run->u.react;
    r->jumped = jumped;
    r->result[r->round] = (uint16_t)(jumped ? 0 : ms);
    if (!jumped && ms < 600) run->score += (long)(600 - ms);
    r->phase = 3;
    r->hold_ms = HOLD_MS;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_react_t *r = &run->u.react;
    if (r->phase == 1) {
        if (r->wait_ms > ms) { r->wait_ms = (uint16_t)(r->wait_ms - ms); return; }
        r->wait_ms = 0;
        r->phase = 2;
        r->spent_ms = 0;
        return;
    }
    if (r->phase == 2) {
        r->spent_ms = (uint16_t)(r->spent_ms + ms);
        if (r->spent_ms >= LATE_MS) record(run, LATE_MS, false);
        return;
    }
    if (r->phase != 3) return;
    r->hold_ms = (uint16_t)(r->hold_ms > ms ? r->hold_ms - ms : 0);
    if (r->hold_ms) return;
    r->round++;
    if (r->round >= PA_REACT_ROUNDS) {
        run->over = true;
        run->note = "五轮都测完了";
        return;
    }
    r->phase = 0;
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    (void)pressed;
    pa_react_t *r = &run->u.react;
    if (r->phase == 0) {
        r->phase = 1;
        r->wait_ms = (uint16_t)(WAIT_MIN + pa_below(&run->rng, WAIT_SPAN));
        return;
    }
    if (r->phase == 1) { record(run, 0, true); return; }
    if (r->phase == 2) { record(run, r->spent_ms, false); return; }
    if (r->hold_ms > 200) r->hold_ms = 200;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_react_t *r = &run->u.react;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 轮   得分 %ld", (unsigned)(r->round + 1U), run->score);

    uint32_t field = 0x4A5A6B;
    if (r->phase == 1) field = 0xC0392B;
    else if (r->phase == 2) field = 0x1E9E5A;
    else if (r->phase == 3) field = r->jumped ? 0x8A3A2F : 0x2B4B6B;
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, field, 4);

    const char *line = "按任意键开始这一轮";
    if (r->phase == 1) line = "等绿灯，别抢";
    else if (r->phase == 2) line = "按！";
    else if (r->phase == 3) line = r->jumped ? "抢跑，这轮不算" : "这轮成绩";
    pa_text(scene, 0, PA_FIELD_Y + 26, PA_VIEW_W, line, PA_WHITE, PA_FONT_ZH, PA_CENTER);

    if (r->phase == 3 && !r->jumped) {
        pa_num(scene, 0, PA_FIELD_Y + 58, PA_VIEW_W, (long)r->result[r->round],
               PA_WHITE, PA_FONT_NUM20, PA_CENTER);
        pa_text(scene, 0, PA_FIELD_Y + 86, PA_VIEW_W, "毫秒", PA_WHITE,
                PA_FONT_ZH, PA_CENTER);
    }

    /* 已经打完的几轮排成一列,方便跟自己比。 */
    for (unsigned i = 0; i < PA_REACT_ROUNDS; i++) {
        int x = 12 + (int)i * 36;
        bool done = (i < r->round) || (i == r->round && r->phase == 3);
        pa_frect(scene, x, 118, 32, 30, done ? PA_PAPER : 0x33455A, 3);
        if (!done) continue;
        if (r->result[i])
            pa_num(scene, x, PA_FIELD_Y + 126, 32, (long)r->result[i], PA_INK,
                   PA_FONT_NUM14, PA_CENTER);
        else
            pa_text(scene, x, PA_FIELD_Y + 124, 32, "抢", PA_RED, PA_FONT_ZH, PA_CENTER);
    }

    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "越快分越高，六百毫秒起算",
            PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "任意键，绿了就按");
}

const pa_game_t pa_game_react = {
    .id = 6,
    .name = "反应力", .genre = "反应",
    .hint = "绿了就按五轮总分",
    .rule = {"按任意键开始，屏幕变绿再按",
             "抢跑这一轮作废，不加分",
             "五轮一局，越快分越高"},
    .keys = "任意键，绿了就按",
    .ok_mode = PA_OK_PRESS,
    .star = {800, 1300, 1800},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
