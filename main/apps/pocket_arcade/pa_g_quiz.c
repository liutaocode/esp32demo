/* 常识问答 —— 三个选项正好对上三个键:上键选第一行,确定键选第二行,
   下键选第三行,不用先挪光标再确认。三条命,答错一次少一条。 */
#include "pa_game.h"
#include "pa_quiz.h"
#include <string.h>

enum { ROW_Y = 48, ROW_H = 32, ROW_STEP = 36, CHIP_W = 22, HOLD_MS = 1600 };

static void ask(pa_run_t *run)
{
    pa_quiz_t *q = &run->u.quiz;
    q->question = (uint16_t)pa_below(&run->rng, PA_QUIZ_COUNT);
    for (unsigned i = 0; i < 3; i++) q->order[i] = (uint8_t)i;
    for (unsigned i = 2; i > 0; i--) {
        unsigned j = pa_below(&run->rng, i + 1U);
        uint8_t swap = q->order[i];
        q->order[i] = q->order[j];
        q->order[j] = swap;
    }
    q->phase = 0;
}

static void reset(pa_run_t *run)
{
    pa_quiz_t *q = &run->u.quiz;
    memset(q, 0, sizeof(*q));
    q->lives = 3;
    ask(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_quiz_t *q = &run->u.quiz;
    if (q->phase != 1) return;
    q->hold_ms = (uint16_t)(q->hold_ms > ms ? q->hold_ms - ms : 0);
    if (q->hold_ms) return;
    if (!q->lives) {
        run->over = true;
        run->note = "三道题答错了";
        return;
    }
    ask(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_quiz_t *q = &run->u.quiz;
    if (q->phase == 1) {
        if (q->hold_ms > 200) q->hold_ms = 200;
        return;
    }
    unsigned row = pa_row_of_key(pressed);
    q->pick = (uint8_t)row;
    q->asked++;
    q->correct = (q->order[row] == 0);
    if (q->correct) {
        q->right++;
        q->combo++;
        run->score += 100 + (long)q->combo * 10;
    } else {
        q->combo = 0;
        if (q->lives) q->lives--;
    }
    q->phase = 1;
    q->hold_ms = HOLD_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_quiz_t *q = &run->u.quiz;
    const pa_quiz_item_t *item = &PA_QUIZ[q->question];
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   连对 %u", run->score, (unsigned)q->combo);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x1F3347, 4);

    pa_text(scene, 4, PA_FIELD_Y + 6, PA_FIELD_W - 8, item->ask1, PA_WHITE,
            PA_FONT_ZH, PA_CENTER);
    pa_text(scene, 4, PA_FIELD_Y + 24, PA_FIELD_W - 8, item->ask2, PA_WHITE,
            PA_FONT_ZH, PA_CENTER);

    for (unsigned row = 0; row < 3; row++) {
        int y = ROW_Y + (int)row * ROW_STEP;
        bool truth = (q->order[row] == 0);
        bool picked = (q->phase == 1 && row == q->pick);
        uint32_t face = PA_PAPER;
        if (q->phase == 1 && truth) face = 0xBBE7C6;
        else if (picked) face = 0xF3B7B0;
        pa_frect(scene, 4, y, PA_FIELD_W - 8, ROW_H, face, 4);
        pa_frect(scene, 8, y + 6, CHIP_W, 20, PA_CREAM, 3);
        pa_text(scene, 8, PA_FIELD_Y + y + 7, CHIP_W, PA_KEY_LABEL[row], PA_INK,
                PA_FONT_ZH, PA_CENTER);
        pa_text(scene, 34, PA_FIELD_Y + y + 8, PA_FIELD_W - 42,
                item->choice[q->order[row]], PA_INK, PA_FONT_ZH, PA_LEFT);
    }

    if (q->phase == 1)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
                q->correct ? "答对了" : "答错了，绿的才对",
                q->correct ? PA_GREEN : PA_RED, PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
                 q->lives > 1 ? PA_INK : PA_RED, PA_FONT_ZH, PA_CENTER,
                 "剩 %u 条命   答对 %u 题", (unsigned)q->lives, (unsigned)q->right);
    pa_footer(scene, "三个选项对应三个键");
}

const pa_game_t pa_game_quiz = {
    .id = 27,
    .name = "常识问答", .genre = "问答",
    .hint = "三个选项对三个键",
    .rule = {"上键一行，下键二行，确定三行",
             "选项顺序每题都会打乱",
             "答错一次少一条命，一共三条"},
    .keys = "三个选项对应三个键",
    .ok_mode = PA_OK_PRESS,
    .star = {800, 2000, 4000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
