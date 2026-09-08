/* 诗词接句 —— 给出上句,从四句里挑出下句。干扰项优先挑字数相同的,
   所以看长短是猜不出来的。三条命,答错一次少一条。 */
#include "pa_game.h"
#include "pa_verse.h"
#include <string.h>

enum { OPTION_H = 24, OPTION_Y = 48, OPTION_STEP = 26, HOLD_MS = 1500 };

static bool already_used(const pa_poem_t *p, unsigned count, uint16_t candidate)
{
    for (unsigned i = 0; i < count; i++)
        if (p->option[i] == candidate) return true;
    return false;
}

static void ask(pa_run_t *run)
{
    pa_poem_t *p = &run->u.poem;
    p->verse = (uint16_t)pa_below(&run->rng, PA_VERSE_COUNT);
    uint8_t want = PA_VERSE[p->verse].len;

    /* 先数一数同字数的句子够不够摆三个干扰项,不够就放宽到任意字数。 */
    unsigned same = 0;
    for (unsigned i = 0; i < PA_VERSE_COUNT; i++)
        if (PA_VERSE[i].len == want && i != p->verse) same++;
    bool strict = (same >= PA_POEM_OPTIONS - 1U);

    p->option[0] = p->verse;
    unsigned filled = 1;
    for (unsigned guard = 0; filled < PA_POEM_OPTIONS && guard < 4000; guard++) {
        uint16_t pick = (uint16_t)pa_below(&run->rng, PA_VERSE_COUNT);
        if (strict && PA_VERSE[pick].len != want) continue;
        if (already_used(p, filled, pick)) continue;
        /* 同一首诗的另一句也不能当干扰项:它同样接得上,题就没有唯一答案了。 */
        if (strcmp(PA_VERSE[pick].down, PA_VERSE[p->verse].down) == 0) continue;
        p->option[filled++] = pick;
    }
    for (unsigned i = filled; i < PA_POEM_OPTIONS; i++) p->option[i] = p->verse;

    /* 洗牌,把正确答案洗到随机一行。 */
    for (unsigned i = PA_POEM_OPTIONS - 1; i > 0; i--) {
        unsigned j = pa_below(&run->rng, i + 1U);
        uint16_t swap = p->option[i];
        p->option[i] = p->option[j];
        p->option[j] = swap;
    }
    for (unsigned i = 0; i < PA_POEM_OPTIONS; i++)
        if (p->option[i] == p->verse) p->answer = (uint8_t)i;
    p->pick = 0;
    p->phase = 0;
}

static void reset(pa_run_t *run)
{
    pa_poem_t *p = &run->u.poem;
    memset(p, 0, sizeof(*p));
    p->lives = 3;
    ask(run);
}

static void tick(pa_run_t *run, uint32_t ms)
{
    pa_poem_t *p = &run->u.poem;
    if (p->phase != 1) return;
    p->hold_ms = (uint16_t)(p->hold_ms > ms ? p->hold_ms - ms : 0);
    if (p->hold_ms) return;
    if (!p->lives) {
        run->over = true;
        run->note = "三次接错了";
        return;
    }
    ask(run);
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_poem_t *p = &run->u.poem;
    if (p->phase == 1) {
        if (p->hold_ms > 200) p->hold_ms = 200;
        return;
    }
    if (pressed == PA_KEY_UP) {
        p->pick = (uint8_t)((p->pick + PA_POEM_OPTIONS - 1U) % PA_POEM_OPTIONS);
        return;
    }
    if (pressed == PA_KEY_DOWN) {
        p->pick = (uint8_t)((p->pick + 1U) % PA_POEM_OPTIONS);
        return;
    }
    p->asked++;
    p->correct = (p->pick == p->answer);
    if (p->correct) {
        p->right++;
        p->combo++;
        run->score += 100 + (long)p->combo * 10;
    } else {
        p->combo = 0;
        if (p->lives) p->lives--;
    }
    p->phase = 1;
    p->hold_ms = HOLD_MS;
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_poem_t *p = &run->u.poem;
    const pa_verse_t *verse = &PA_VERSE[p->verse];
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "得分 %ld   连对 %u", run->score, (unsigned)p->combo);
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0xF3ECD9, 4);

    pa_textf(scene, 4, PA_FIELD_Y + 4, PA_FIELD_W - 8, PA_SLATE, PA_FONT_ZH12, PA_CENTER,
             "%s   %s", verse->title, verse->author);
    pa_text(scene, 4, PA_FIELD_Y + 20, PA_FIELD_W - 8, verse->up, PA_INK,
            PA_FONT_ZH, PA_CENTER);

    for (unsigned i = 0; i < PA_POEM_OPTIONS; i++) {
        int y = OPTION_Y + (int)i * OPTION_STEP;
        bool picked = (i == p->pick);
        bool truth = (i == p->answer);
        uint32_t face = PA_PAPER;
        if (p->phase == 1 && truth) face = 0xBBE7C6;
        else if (p->phase == 1 && picked) face = 0xF3B7B0;
        else if (picked) face = PA_YELLOW;
        pa_frect(scene, 4, y, PA_FIELD_W - 8, OPTION_H, face, 4);
        if (picked && p->phase == 0) pa_frect(scene, 4, y, 4, OPTION_H, PA_RED, 0);
        pa_text(scene, 8, PA_FIELD_Y + y + 4, PA_FIELD_W - 16,
                PA_VERSE[p->option[i]].down, PA_INK, PA_FONT_ZH, PA_CENTER);
    }

    if (p->phase == 1)
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
                p->correct ? "接对了" : "接错了，绿的才是",
                p->correct ? PA_GREEN : PA_RED, PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
                 p->lives > 1 ? PA_INK : PA_RED, PA_FONT_ZH, PA_CENTER,
                 "剩 %u 条命   答对 %u 题", (unsigned)p->lives, (unsigned)p->right);
    pa_footer(scene, "上下选一句　确定确认");
}

const pa_game_t pa_game_poem = {
    .id = 26,
    .name = "诗词接句", .genre = "诗词",
    .hint = "给上句挑下句",
    .rule = {"上下键选一句，确定键确认",
             "干扰项字数和答案相同，猜不出来",
             "答错一次少一条命，一共三条"},
    .keys = "上下选　确定确认",
    .ok_mode = PA_OK_PRESS,
    .star = {600, 1500, 3000},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
