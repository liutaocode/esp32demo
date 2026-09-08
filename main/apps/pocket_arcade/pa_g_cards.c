/* 二十一点 —— 十二局定输赢。下注阶段上下键改注额,确定发牌;
   拿牌阶段确定要牌、下键停牌。庄家满十七点必须停。 */
#include "pa_game.h"
#include <string.h>

enum {
    CHIPS_START = 100, CARD_W = 24, CARD_H = 32, CARD_STEP = 26,
    ROW_DEALER = 16, ROW_PLAYER = 92, SHOW_MAX = 6,
};

static const uint16_t BET_STEP[3] = {10, 20, 50};
/* 牌面 0..12 依次是 A 到 K;A 先按十一点算,爆了再退回一点。 */
static const char *const RANK[13] = {
    "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
};

static uint8_t card_points(uint8_t rank)
{
    if (rank == 0) return 11;
    return (uint8_t)(rank >= 9 ? 10 : rank + 1);
}

static unsigned hand_value(const uint8_t *cards, uint8_t count)
{
    unsigned total = 0, aces = 0;
    for (uint8_t i = 0; i < count; i++) {
        total += card_points(cards[i]);
        if (cards[i] == 0) aces++;
    }
    while (total > 21 && aces) { total -= 10; aces--; }
    return total;
}

static uint8_t deal(pa_run_t *run) { return (uint8_t)pa_below(&run->rng, 13); }

static void reset(pa_run_t *run)
{
    pa_cards_t *c = &run->u.cards;
    memset(c, 0, sizeof(*c));
    c->chips = CHIPS_START;
    c->bet = BET_STEP[0];
    run->score = CHIPS_START;
}

static void finish(pa_run_t *run, uint8_t result, int delta)
{
    pa_cards_t *c = &run->u.cards;
    c->result = result;
    c->delta = (int16_t)delta;
    c->phase = 2;
    c->hidden = false;
    int chips = (int)c->chips + delta;
    c->chips = (uint16_t)(chips < 0 ? 0 : chips);
    run->score = c->chips;
    c->hand++;
}

static void dealer_turn(pa_run_t *run)
{
    pa_cards_t *c = &run->u.cards;
    while (hand_value(c->dealer, c->dn) < 17 && c->dn < PA_CARD_HAND)
        c->dealer[c->dn++] = deal(run);
    unsigned mine = hand_value(c->player, c->pn);
    unsigned theirs = hand_value(c->dealer, c->dn);
    if (theirs > 21 || mine > theirs) finish(run, 1, c->bet);
    else if (mine == theirs) finish(run, 0, 0);
    else finish(run, 2, -(int)c->bet);
}

static void start_hand(pa_run_t *run)
{
    pa_cards_t *c = &run->u.cards;
    if (c->bet > c->chips) c->bet = c->chips;
    c->pn = c->dn = 0;
    c->player[c->pn++] = deal(run);
    c->player[c->pn++] = deal(run);
    c->dealer[c->dn++] = deal(run);
    c->dealer[c->dn++] = deal(run);
    c->hidden = true;
    c->phase = 1;
    c->delta = 0;
    c->result = 0;
    if (hand_value(c->player, c->pn) == 21) {
        if (hand_value(c->dealer, c->dn) == 21) finish(run, 0, 0);
        else finish(run, 3, (int)c->bet + (int)c->bet / 2);
    }
}

static void next_hand(pa_run_t *run)
{
    pa_cards_t *c = &run->u.cards;
    if (!c->chips) {
        run->over = true;
        run->note = "筹码输光了";
        return;
    }
    if (c->hand >= PA_CARD_HANDS) {
        run->over = true;
        run->note = "十二局打完了";
        return;
    }
    c->phase = 0;
    if (c->bet > c->chips) c->bet = c->chips;
}

static void tick(pa_run_t *run, uint32_t ms)
{
    (void)run;
    (void)ms;   /* 回合制,不随时间推进 */
}

static void change_bet(pa_run_t *run, int direction)
{
    pa_cards_t *c = &run->u.cards;
    unsigned index = 0;
    for (unsigned i = 0; i < 3; i++)
        if (BET_STEP[i] == c->bet) index = i;
    index = (unsigned)(((int)index + direction + 3) % 3);
    c->bet = BET_STEP[index];
    if (c->bet > c->chips) c->bet = c->chips;
}

static void key(pa_run_t *run, pa_key_t pressed)
{
    pa_cards_t *c = &run->u.cards;
    if (c->phase == 0) {
        if (pressed == PA_KEY_OK) start_hand(run);
        else change_bet(run, pressed == PA_KEY_UP ? 1 : -1);
        return;
    }
    if (c->phase == 2) {
        if (pressed == PA_KEY_OK) next_hand(run);
        return;
    }
    if (pressed == PA_KEY_DOWN) { dealer_turn(run); return; }
    if (pressed != PA_KEY_OK) return;
    if (c->pn >= PA_CARD_HAND) return;
    c->player[c->pn++] = deal(run);
    if (hand_value(c->player, c->pn) > 21) finish(run, 2, -(int)c->bet);
}

static void draw_hand(pa_scene_t *scene, const uint8_t *cards, uint8_t count,
                      int y, bool hide_second)
{
    uint8_t shown = count > SHOW_MAX ? SHOW_MAX : count;
    for (uint8_t i = 0; i < shown; i++) {
        int x = 32 + (int)i * CARD_STEP;
        bool covered = hide_second && i == 1;
        pa_frect(scene, x, y, CARD_W, CARD_H, covered ? PA_BLUE : PA_WHITE, 3);
        if (covered) {
            pa_frect(scene, x + 5, y + 7, CARD_W - 10, CARD_H - 14, 0x1E4F9C, 2);
            continue;
        }
        pa_text(scene, x, PA_FIELD_Y + y + 9, CARD_W, RANK[cards[i]],
                (cards[i] % 2) ? PA_RED : PA_INK, PA_FONT_NUM14, PA_CENTER);
    }
}

static void draw(const pa_run_t *run, pa_scene_t *scene)
{
    const pa_cards_t *c = &run->u.cards;
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "筹码 %u   第 %u 局", (unsigned)c->chips,
             (unsigned)(c->phase == 2 ? c->hand : c->hand + 1U));
    pa_frect(scene, 0, 0, PA_FIELD_W, PA_FIELD_H, 0x146B4A, 4);

    pa_text(scene, 1, PA_FIELD_Y + ROW_DEALER + 1, 28, "庄", PA_CREAM, PA_FONT_ZH, PA_CENTER);
    draw_hand(scene, c->dealer, c->dn, ROW_DEALER, c->hidden);
    if (!c->hidden && c->dn)
        pa_num(scene, 1, PA_FIELD_Y + ROW_DEALER + 18, 28,
               (long)hand_value(c->dealer, c->dn), PA_YELLOW, PA_FONT_NUM14, PA_CENTER);

    pa_text(scene, 1, PA_FIELD_Y + ROW_PLAYER + 1, 28, "你", PA_CREAM, PA_FONT_ZH, PA_CENTER);
    draw_hand(scene, c->player, c->pn, ROW_PLAYER, false);
    if (c->pn)
        pa_num(scene, 1, PA_FIELD_Y + ROW_PLAYER + 18, 28,
               (long)hand_value(c->player, c->pn), PA_YELLOW, PA_FONT_NUM14, PA_CENTER);

    if (c->phase == 0) {
        pa_textf(scene, 0, PA_FIELD_Y + 62, PA_VIEW_W, PA_YELLOW, PA_FONT_ZH, PA_CENTER,
                 "本局下注 %u", (unsigned)c->bet);
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "上下改注额，确定发牌",
                PA_INK, PA_FONT_ZH, PA_CENTER);
        pa_footer(scene, "上下改注额　确定发牌");
        return;
    }
    if (c->phase == 1) {
        pa_textf(scene, 0, PA_FIELD_Y + 62, PA_VIEW_W, PA_CREAM, PA_FONT_ZH, PA_CENTER,
                 "已押 %u   庄家暗一张", (unsigned)c->bet);
        pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "十七点以上就该考虑停牌",
                PA_INK, PA_FONT_ZH, PA_CENTER);
        pa_footer(scene, "下键停牌　确定要牌");
        return;
    }
    static const char *const RESULT[4] = {"平局，退回本金", "你赢了这一局", "这一局输掉了", "黑杰克，一点五倍"};
    pa_text(scene, 0, PA_FIELD_Y + 62, PA_VIEW_W, RESULT[c->result],
            c->result == 2 ? PA_RED : PA_YELLOW, PA_FONT_ZH, PA_CENTER);
    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W,
             c->delta >= 0 ? PA_GREEN : PA_RED, PA_FONT_ZH, PA_CENTER,
             "筹码变化 %+d", (int)c->delta);
    pa_footer(scene, "确定进入下一局");
}

const pa_game_t pa_game_cards = {
    .id = 31,
    .name = "二十一点", .genre = "牌桌",
    .hint = "十二局定输赢",
    .rule = {"下注阶段上下改注额，确定发牌",
             "下键停牌，确定要牌，爆点就输",
             "庄家不到十七点必须继续要牌"},
    .keys = "下停牌　确定要牌",
    .ok_mode = PA_OK_PRESS,
    .star = {140, 240, 400},
    .reset = reset, .tick = tick, .key = key, .draw = draw,
};
