/* 保龄球 —— 两段力度条:确定键先定落点,再定力度,然后球自己滚出去。
   十轮标准记分,全中和补中都按规则加算后面几球。 */
#include "pa_game.h"
#include <string.h>

enum {
    LANE_X = 34, LANE_W = 130, LANE_Y = 6, LANE_H = 118,
    BAR_Y = 134, BAR_H = 12, ROLL_MS = 900, HOLD_MS = 1100,
    PINS = 10, FRAMES = 10,
};

/* 十个瓶按四行摆开,坐标是球道内的像素。 */
static const int8_t PIN_X[PINS] = {-27, -9, 9, 27, -18, 0, 18, -9, 9, 0};
static const int8_t PIN_Y[PINS] = {12, 12, 12, 12, 30, 30, 30, 48, 48, 66};

static int lane_centre(void) { return LANE_X + LANE_W / 2; }

static unsigned standing(const pa_bowl_t *b)
{
    unsigned count = 0;
    for (unsigned i = 0; i < PINS; i++) count += (b->pins >> i) & 1U;
    return count;
}

/* 标准记分:全中加后两球,补中加后一球。 */
static unsigned tally(const uint8_t *rolls, uint8_t rolled)
{
    unsigned total = 0, i = 0;
    for (unsigned frame = 0; frame < FRAMES && i < rolled; frame++) {
        if (rolls[i] == PINS) {
            total += PINS + rolls[i + 1] + rolls[i + 2];
            i += 1;
        } else if ((unsigned)rolls[i] + rolls[i + 1] == PINS) {
            total += PINS + rolls[i + 2];
            i += 2;
        } else {
            total += (unsigned)rolls[i] + rolls[i + 1];
            i += 2;
        }
    }
    return total;
}

static void reset(pa_run_t *run)
{
    pa_bowl_t *b = &run->u.bowl;
    memset(b, 0, sizeof(*b));
    b->pins = (1U << PINS) - 1U;
    b->aim_dir = 1;
    b->power_dir = 1;
    b->power = 30;
}

static void next_ball(pa_run_t *run)
{
    pa_bowl_t *b = &run->u.bowl;
    run->score = (long)tally(b->rolls, b->rolled);
    bool last_frame = (b->frame + 1U >= FRAMES);
    unsigned left = standing(b);
    b->phase = 0;
    b->aim = 0;
    b->aim_dir = 1;
    b->power = 30;
    b->power_dir = 1;

    if (!last_frame) {
        if (b->ball == 0 && left) { b->ball = 1; return; }
        b->frame++;
        b->ball = 0;
        b->pins = (1U << PINS) - 1U;
        return;
    }
    /* 第十轮:全中或补中都多打一球,瓶重新摆好。 */
    if (b->ball == 0) {
        b->ball = 1;
        if (!left) b->pins = (1U << PINS) - 1U;
        return;
    }
    if (b->ball == 1 && b->rolled >= 2) {
        unsigned first = b->rolls[b->rolled - 2];
        unsigned second = b->rolls[b->rolled - 1];
        if (first == PINS || first + second == PINS) {
            b->ball = 2;
            if (!left) b->pins = (1U << PINS) - 1U;
            return;
        }
    }
    run->over = true;
    run->note = "十轮打完了";
}

static void roll(pa_run_t *run)
{
    pa_bowl_t *b = &run->u.bowl;
    /* 落点由瞄准条决定,力度决定扫倒的宽度。 */
    int impact = b->aim * 46 / 100;
    int spread = 15 + b->power / 5;
    unsigned before = standing(b);
    for (unsigned i = 0; i < PINS; i++) {
        if (!((b->pins >> i) & 1U)) continue;
        int distance = PIN_X[i] - impact;
        if (distance < 0) distance = -distance;
        /* 后排的瓶要靠前排带倒,所以越靠后判定越窄。 */
        int reach = spread - PIN_Y[i] / 6;
        if (distance <= reach) b->pins = (uint16_t)(b->pins & ~(1U << i));
    }
    unsigned knocked = before - standing(b);
    if (b->rolled < 21) b->rolls[b->rolled++] = (uint8_t)knocked;
    b->landed = (int16_t)impact;
    b->phase = 3;
    b->hold_ms = HOLD_MS;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_bowl_t *b = &run->u.bowl;
    if (b->phase == 0) {
        int aim = b->aim + b->aim_dir * (int)(160 * ms) / 1000;
        if (aim >= 100) { aim = 100; b->aim_dir = -1; }
        if (aim <= -100) { aim = -100; b->aim_dir = 1; }
        b->aim = (int16_t)aim;
        return;
    }
    if (b->phase == 1) {
        int power = b->power + b->power_dir * (int)(150 * ms) / 1000;
        if (power >= 100) { power = 100; b->power_dir = -1; }
        if (power <= 0) { power = 0; b->power_dir = 1; }
        b->power = (int16_t)power;
        return;
    }
    if (b->phase == 2) {
        b->roll_ms = (uint16_t)(b->roll_ms + ms);
        if (b->roll_ms < ROLL_MS) return;
        roll(run);
        return;
    }
    b->hold_ms = (uint16_t)(b->hold_ms > ms ? b->hold_ms - ms : 0);
    if (!b->hold_ms) next_ball(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    (void)pressed;
    pa_bowl_t *b = &run->u.bowl;
    if (b->phase == 0) { b->phase = 1; return; }
    if (b->phase == 1) { b->phase = 2; b->roll_ms = 0; return; }
    if (b->phase == 3 && b->hold_ms > 200) b->hold_ms = 200;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_bowl_t *b = &run->u.bowl;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "第 %u 轮   总分 %ld", (unsigned)(b->frame + 1U), run->score);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x2B2118, 4);
    pa_frect(scene, LANE_X, LANE_Y, LANE_W, LANE_H, 0xC9A66B, 3);
    pa_frect(scene, LANE_X - 6, LANE_Y, 6, LANE_H, 0x4A3A28, 0);
    pa_frect(scene, LANE_X + LANE_W, LANE_Y, 6, LANE_H, 0x4A3A28, 0);

    for (unsigned i = 0; i < PINS; i++) {
        if (!((b->pins >> i) & 1U)) continue;
        pa_frect(scene, lane_centre() + PIN_X[i] - 4, LANE_Y + PIN_Y[i], 8, 12, PA_WHITE, 3);
        pa_frect(scene, lane_centre() + PIN_X[i] - 4, LANE_Y + PIN_Y[i] + 3, 8, 2, PA_RED, 0);
    }

    int ball_x = lane_centre() + b->aim * 46 / 100;
    int ball_y = LANE_Y + LANE_H - 16;
    if (b->phase == 2) {
        ball_y = LANE_Y + LANE_H - 16 -
                 (LANE_H - 30) * (int)b->roll_ms / ROLL_MS;
    } else if (b->phase == 3) {
        ball_x = lane_centre() + b->landed;
        ball_y = LANE_Y + 4;
    }
    pa_frect(scene, ball_x - 6, ball_y, 12, 12, PA_INK, 6);

    if (b->phase <= 1) {
        pa_frect(scene, LANE_X, BAR_Y, LANE_W, BAR_H, PA_PAPER, 2);
        pa_frect(scene, lane_centre() + b->aim * (LANE_W / 2 - 4) / 100 - 2, BAR_Y - 3,
                 5, BAR_H + 6, PA_RED, 1);
        pa_frect(scene, lane_centre() - 2, BAR_Y + 4, 4, 4, PA_GRAY, 0);
    }
    if (b->phase == 1) {
        pa_frect(scene, LANE_X, BAR_Y + 16, LANE_W, 8, PA_PAPER, 2);
        pa_frect(scene, LANE_X, BAR_Y + 16, b->power * LANE_W / 100, 8, PA_GREEN, 2);
    }

    const char *line = "确定键先定落点";
    uint32_t colour = PA_INK;
    if (b->phase == 1) line = "再按一下定力度";
    else if (b->phase == 2) { line = "球出去了"; colour = PA_SLATE; }
    else if (b->phase == 3) {
        unsigned left = standing(b);
        if (!left) { line = "全倒了"; colour = PA_GREEN; }
        else { line = "还有瓶站着"; colour = PA_ORANGE; }
    }
    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, line, colour, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "确定定落点　再确定定力度");
}

const pa_game_t pa_game_bowl = {
    .id = 11,
    .name = "保龄球", .genre = "手感",
    .hint = "落点加力度两下定",
    .rule = {"确定停落点条，再按一下停力度",
             "落点决定撞哪里，力度决定扫多宽",
             "十轮标准记分，全中会加后面两球"},
    .keys = "确定定落点再定力度",
    .ok_mode = PA_OK_PRESS,
    .star = {70, 110, 160},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
