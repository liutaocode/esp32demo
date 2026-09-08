/* 跳一跳 —— 力度条自己来回摆,确定键在合适的时候松手。
   踩得越正,连击越高;台子会越来越窄、越来越远。 */
#include "pa_game.h"
#include <string.h>

enum {
    PLAT_Y = 112, PLAT_H = 16, MAN_W = 14, MAN_H = 20,
    BAR_X = 14, BAR_Y = 142, BAR_W = 170, BAR_H = 12,
    FLY_MS = 420, HOLD_MS = 460,
    REACH_MIN = 46, REACH_MAX = 122,
};

static int here_centre(const pa_jump_t *j) { return PA_JUMP_HOME_X + j->here_w / 2; }
static int next_centre(const pa_jump_t *j) { return j->next_x + j->next_w / 2; }

/* 台子随分数变窄、变远,但始终留在力度条能够到的范围里。 */
static void make_next(pa_run_t *run)
{
    pa_jump_t *j = &run->u.jump;
    unsigned level = (unsigned)(run->score / 60);
    if (level > 6U) level = 6U;
    unsigned width = 44U - level * 4U + pa_below(&run->rng, 6);
    j->next_w = (uint8_t)(width < 20U ? 20U : width);
    unsigned span = REACH_MAX - REACH_MIN;
    unsigned reach = REACH_MIN + 8U * level + pa_below(&run->rng, span - 8U * level);
    j->next_x = (int16_t)(here_centre(j) + (int)reach - j->next_w / 2);
    if (j->next_x + j->next_w > PA_FIELD_W - 6)
        j->next_x = (int16_t)(PA_FIELD_W - 6 - j->next_w);
}

static void reset(pa_run_t *run)
{
    pa_jump_t *j = &run->u.jump;
    memset(j, 0, sizeof(*j));
    j->here_w = 44;
    j->power_dir = 1;
    make_next(run);
}

static unsigned sweep_rate(const pa_run_t *run)
{
    unsigned rate = 130U + (unsigned)(run->score / 12);
    return rate > 320U ? 320U : rate;
}

static void land(pa_run_t *run)
{
    pa_jump_t *j = &run->u.jump;
    int centre = next_centre(j);
    int off = j->land_x - centre;
    if (off < 0) off = -off;
    j->perfect = (off <= 4);
    j->missed = (off > j->next_w / 2);
    if (j->missed) {
        j->combo = 0;
        return;
    }
    j->combo++;
    run->score += 10 + (j->perfect ? 20 : 0) + (long)(j->combo / 3);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_jump_t *j = &run->u.jump;
    if (j->phase == 0) {
        int move = (int)(sweep_rate(run) * ms) / 1000;
        if (move < 1) move = 1;
        int power = (int)j->power + j->power_dir * move;
        if (power >= 100) { power = 100; j->power_dir = -1; }
        if (power <= 0) { power = 0; j->power_dir = 1; }
        j->power = (uint16_t)power;
        return;
    }
    if (j->phase == 1) {
        j->fly_ms = (uint16_t)(j->fly_ms + ms);
        if (j->fly_ms < j->fly_total) return;
        j->fly_ms = j->fly_total;
        j->phase = 2;
        j->hold_ms = 0;
        land(run);
        return;
    }
    j->hold_ms = (uint16_t)(j->hold_ms + ms);
    if (j->hold_ms < HOLD_MS) return;
    if (j->missed) {
        run->over = true;
        run->note = "踩空掉了下去";
        return;
    }
    /* 落稳了:把下一块搬到起跳位,再生成新的目标。 */
    j->here_w = j->next_w;
    j->phase = 0;
    j->power = 0;
    j->power_dir = 1;
    j->fly_ms = 0;
    make_next(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    (void)pressed;
    pa_jump_t *j = &run->u.jump;
    if (j->phase != 0) return;
    j->land_x = (int16_t)(here_centre(j) + 34 + (int)j->power);
    j->fly_total = FLY_MS;
    j->fly_ms = 0;
    j->phase = 1;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_jump_t *j = &run->u.jump;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   连击 %u", run->score, (unsigned)j->combo);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x9CD8F5, 4);
    pa_frect(scene, 0, PLAT_Y + PLAT_H, PA_FIELD_W, PA_FIELD_H - PLAT_Y - PLAT_H, 0x7FC0E4, 0);

    pa_frect(scene, PA_JUMP_HOME_X, PLAT_Y, j->here_w, PLAT_H, PA_ORANGE, 3);
    pa_frect(scene, PA_JUMP_HOME_X, PLAT_Y, j->here_w, 4, PA_YELLOW, 3);
    pa_frect(scene, j->next_x, PLAT_Y, j->next_w, PLAT_H, PA_PURPLE, 3);
    pa_frect(scene, next_centre(j) - 2, PLAT_Y, 4, 4, PA_WHITE, 0);

    int man_x = here_centre(j) - MAN_W / 2;
    int man_y = PLAT_Y - MAN_H;
    if (j->phase >= 1) {
        int from = here_centre(j);
        int to = j->land_x;
        int step = (int)j->fly_ms;
        int total = j->fly_total ? (int)j->fly_total : 1;
        man_x = from + (to - from) * step / total - MAN_W / 2;
        /* 抛物线:中点最高 34 像素,两端归零。 */
        int arc = 34 * 4 * step * (total - step) / (total * total);
        man_y = PLAT_Y - MAN_H - arc;
        if (j->phase == 2 && j->missed) man_y = PLAT_Y + 6;
    }
    pa_frect(scene, man_x, man_y, MAN_W, MAN_H, PA_RED, 3);
    pa_frect(scene, man_x + 3, man_y + 4, 3, 3, PA_WHITE, 0);

    pa_frect(scene, BAR_X - 2, BAR_Y - 2, BAR_W + 4, BAR_H + 4, PA_INK, 3);
    pa_frect(scene, BAR_X, BAR_Y, BAR_W, BAR_H, PA_PAPER, 2);
    pa_frect(scene, BAR_X, BAR_Y, (int)j->power * BAR_W / 100, BAR_H, PA_GREEN, 2);
    int mark = BAR_X + (next_centre(j) - here_centre(j) - 34) * BAR_W / 100;
    pa_frect(scene, mark - 1, BAR_Y - 4, 3, BAR_H + 8, PA_RED, 1);

    if (j->phase == 2 && j->missed)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "踩空了", PA_RED, PA_FONT_ZH, PA_CENTER);
    else if (j->phase == 2 && j->perfect)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "正中红点，加二十分",
                PA_GREEN, PA_FONT_ZH, PA_CENTER);
    else
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "红线就是刚好的力度",
                PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "任意键起跳　对准红线");
}

const pa_game_t pa_game_jump = {
    .id = 5,
    .name = "跳一跳", .genre = "手感",
    .hint = "看准红线松手",
    .rule = {"力度条自己来回摆动，按键就起跳",
             "红色竖线是这一跳刚好的力度",
             "正中台子中点加二十分"},
    .keys = "任意键起跳对红线",
    .ok_mode = PA_OK_PRESS,
    .star = {150, 400, 800},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
