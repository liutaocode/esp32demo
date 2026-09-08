/* 记忆音阶 —— 三个键就是三个音。看它亮一遍,再照着按一遍,
   每过一关序列长一位,错一下就结束。 */
#include "pa_game.h"
#include <string.h>

enum {
    PAD_W = 176, PAD_H = 42, PAD_X = 11, PAD_TOP = 8, PAD_GAP = 7,
    SHOW_MS = 420, DARK_MS = 200, GOOD_MS = 600, BAD_MS = 900, START_LEN = 3,
};

static const uint32_t PAD_COLOUR[3] = {0x2F6FD0, 0x168B79, 0xE8639B};
static const uint32_t PAD_LIT[3] = {0x8CC7FF, 0x6BE3C6, 0xFFB0D2};

static int pad_y(unsigned pad) { return PAD_TOP + (int)pad * (PAD_H + PAD_GAP); }

static void extend(pa_run_t *run)
{
    pa_simon_t *s = &run->u.simon;
    if (s->length < PA_SIMON_MAX) s->seq[s->length++] = (uint8_t)pa_below(&run->rng, 3);
    s->step = 0;
    s->phase = 0;
    s->timer_ms = 600;
    s->lit = -1;
}

static void reset(pa_run_t *run)
{
    pa_simon_t *s = &run->u.simon;
    memset(s, 0, sizeof(*s));
    s->round = 1;
    for (unsigned i = 0; i < START_LEN - 1U; i++)
        s->seq[s->length++] = (uint8_t)pa_below(&run->rng, 3);
    extend(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_simon_t *s = &run->u.simon;
    if (s->timer_ms > ms) { s->timer_ms = (uint16_t)(s->timer_ms - ms); return; }
    s->timer_ms = 0;
    if (s->phase == 0) {
        /* 展示:亮一个、灭一下、再亮下一个,最后交给玩家。 */
        if (s->lit >= 0) {
            s->lit = -1;
            s->step++;
            s->timer_ms = DARK_MS;
            if (s->step < s->length) return;
            s->phase = 1;
            s->step = 0;
            s->timer_ms = 0;
            return;
        }
        if (s->step >= s->length) { s->phase = 1; s->step = 0; return; }
        s->lit = (int8_t)s->seq[s->step];
        s->timer_ms = (uint16_t)(SHOW_MS - (s->length > 12U ? 120U : s->length * 10U));
        return;
    }
    if (s->phase == 2) {
        s->round++;
        extend(run);
        return;
    }
    if (s->phase == 3) {
        run->over = true;
        run->note = "按错了一个音";
    }
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_simon_t *s = &run->u.simon;
    if (s->phase != 1) return;
    uint8_t pad = (uint8_t)pa_row_of_key(pressed);
    s->lit = (int8_t)pad;
    if (pad != s->seq[s->step]) {
        s->phase = 3;
        s->timer_ms = BAD_MS;
        return;
    }
    run->score += 10 + (long)s->length;
    s->step++;
    if (s->step < s->length) return;
    s->phase = 2;
    s->timer_ms = GOOD_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_simon_t *s = &run->u.simon;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 关   %u 个音", (unsigned)s->round, (unsigned)s->length);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x141C26, 4);
    for (unsigned pad = 0; pad < 3; pad++) {
        bool lit = (s->lit == (int8_t)pad);
        pa_frect(scene, PAD_X, pad_y(pad), PAD_W, PAD_H,
                 lit ? PAD_LIT[pad] : PAD_COLOUR[pad], 5);
        pa_text(scene, PAD_X, PA_FIELD_Y + pad_y(pad) + PAD_H / 2 - 9, PAD_W,
                PA_KEY_LABEL[pad], lit ? PA_INK : PA_WHITE, PA_FONT_ZH, PA_CENTER);
    }
    const char *line = "看它亮一遍";
    uint32_t colour = PA_SLATE;
    if (s->phase == 1) {
        line = "该你按了";
        colour = PA_INK;
    } else if (s->phase == 2) {
        line = "全对，加长一位";
        colour = PA_GREEN;
    } else if (s->phase == 3) {
        line = "按错了";
        colour = PA_RED;
    }
    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, line, colour, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "三个键就是三个音");
}

const pa_game_t pa_game_simon = {
    .id = 13,
    .name = "记忆音阶", .genre = "记忆",
    .hint = "看一遍再照着按",
    .rule = {"三块灯板从上到下对应上下确定",
             "先看它亮一遍，再照着按一遍",
             "每过一关序列长一位，错一下结束"},
    .keys = "三个键就是三个音",
    .ok_mode = PA_OK_PRESS,
    .star = {150, 350, 650},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
