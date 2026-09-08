#include "pa_hub.h"
#include <stdio.h>
#include <string.h>

enum { ROW_H = 38, ROW_GAP = 2, ROW_TOP = 2 };

/* 摆放顺序照顾第一次上手:先放最好懂的,要动脑子的排后面。 */
/* 摆放顺序照顾第一次上手:开头几个必须一眼就会、也来不及输。打地鼠和三键节拍
   先把"三个键对三样东西"教会,然后是单键的,再是要转向和要动脑的。
   这只是显示顺序 —— 纪录按 pa_game_t.id 存,所以以后再调顺序也不会错位。 */
static const pa_game_t *const REGISTRY[PA_GAME_COUNT] = {
    &pa_game_mole, &pa_game_tiles, &pa_game_bird, &pa_game_jump,
    &pa_game_react, &pa_game_dino, &pa_game_snake, &pa_game_frog,
    &pa_game_brick, &pa_game_pong, &pa_game_invader, &pa_game_bowl,
    &pa_game_sling, &pa_game_simon, &pa_game_pairs, &pa_game_mathq,
    &pa_game_tetris, &pa_game_merge, &pa_game_slide, &pa_game_hanoi,
    &pa_game_lights, &pa_game_peg, &pa_game_sokoban, &pa_game_mines,
    &pa_game_nono, &pa_game_guess, &pa_game_poem, &pa_game_quiz,
    &pa_game_four, &pa_game_gomoku, &pa_game_reversi, &pa_game_cards,
};

/* 顺序必须和 bsp_button.h 里的 BSP_BTN_UP / DOWN / OK 一致。 */
static const pa_key_t ROW_TO_KEY[PA_KEY_ROWS] = {PA_KEY_UP, PA_KEY_DOWN, PA_KEY_OK};
const char *const PA_KEY_LABEL[PA_KEY_ROWS] = {"上", "下", "定"};

pa_key_t pa_key_of_row(unsigned row) { return ROW_TO_KEY[row % PA_KEY_ROWS]; }

unsigned pa_row_of_key(pa_key_t key)
{
    if (key == PA_KEY_UP) return 0;
    if (key == PA_KEY_DOWN) return 1;
    return 2;                      /* 确定和它的双击都算最后一行 */
}

const pa_game_t *pa_game_at(unsigned index)
{
    return REGISTRY[index % PA_GAME_COUNT];
}

static const pa_game_t *game_at(unsigned index) { return pa_game_at(index); }

/* 纪录格子按 id 找,不按大厅位置。 */
static uint16_t *record_of(pa_hub_t *hub, unsigned index)
{
    return &hub->record.best[game_at(index)->id];
}

static uint16_t best_of(const pa_hub_t *hub, unsigned index)
{
    return hub->record.best[game_at(index)->id];
}

unsigned pa_game_stars(unsigned game, long score)
{
    const pa_game_t *entry = game_at(game);
    unsigned stars = 0;
    for (unsigned i = 0; i < 3; i++)
        if (score >= (long)entry->star[i]) stars++;
    return stars;
}

unsigned pa_hub_stars(const pa_hub_t *hub)
{
    unsigned total = 0;
    for (unsigned i = 0; i < PA_GAME_COUNT; i++)
        total += pa_game_stars(i, best_of(hub, i));
    return total;
}

void pa_record_encode(const pa_record_t *record, uint8_t *out)
{
    out[0] = 'A';
    out[1] = 2;
    for (unsigned i = 0; i < PA_GAME_COUNT; i++) {
        out[2 + i * 2] = (uint8_t)(record->best[i] & 0xFFU);
        out[3 + i * 2] = (uint8_t)(record->best[i] >> 8);
    }
    out[2 + PA_GAME_COUNT * 2] = (uint8_t)(record->plays & 0xFFU);
    out[3 + PA_GAME_COUNT * 2] = (uint8_t)(record->plays >> 8);
}

bool pa_record_decode(pa_record_t *record, const uint8_t *in)
{
    if (in[0] != 'A' || in[1] != 2) return false;
    for (unsigned i = 0; i < PA_GAME_COUNT; i++)
        record->best[i] = (uint16_t)(in[2 + i * 2] | ((uint16_t)in[3 + i * 2] << 8));
    record->plays = (uint16_t)(in[2 + PA_GAME_COUNT * 2] |
                               ((uint16_t)in[3 + PA_GAME_COUNT * 2] << 8));
    return true;
}

static void go(pa_hub_t *hub, pa_page_t page)
{
    hub->page = page;
    hub->guard_ms = PA_GUARD_MS;
    hub->click_pending = false;
    hub->click_ms = 0;
}

void pa_hub_init(pa_hub_t *hub, const pa_record_t *record, uint32_t seed)
{
    memset(hub, 0, sizeof(*hub));
    if (record) hub->record = *record;
    hub->rng = seed ? seed : 0x1234ABCDU;
    go(hub, PA_PAGE_MENU);
}

bool pa_hub_wants_click(const pa_hub_t *hub)
{
    return hub->page == PA_PAGE_PLAY && game_at(hub->game)->ok_mode == PA_OK_CLICK;
}

static void start_run(pa_hub_t *hub)
{
    memset(&hub->run, 0, sizeof(hub->run));
    hub->run.rng = pa_rand(&hub->rng);
    hub->new_best = false;
    game_at(hub->game)->reset(&hub->run);
    go(hub, PA_PAGE_PLAY);
}

/* 一局结束(自然结束或中途退出)都在这里落账。 */
static void settle(pa_hub_t *hub)
{
    long score = hub->run.score;
    if (score < 0) score = 0;
    if (score > 65535L) score = 65535L;
    hub->earned = (uint8_t)pa_game_stars(hub->game, score);
    uint16_t *best = record_of(hub, hub->game);
    if ((uint16_t)score > *best) {
        *best = (uint16_t)score;
        hub->new_best = true;
    }
    if (hub->record.plays < 65535U) hub->record.plays++;
    hub->record_dirty = true;
}

static void menu_move(pa_hub_t *hub, int delta)
{
    /* 首尾相接:从第一个往上走会落到最后一个,翻页也一样。 */
    int cursor = ((int)hub->cursor + delta) % PA_GAME_COUNT;
    if (cursor < 0) cursor += PA_GAME_COUNT;
    hub->cursor = (uint8_t)cursor;
    if (hub->cursor < hub->top) hub->top = hub->cursor;
    if (hub->cursor >= hub->top + PA_MENU_ROWS)
        hub->top = (uint8_t)(hub->cursor - PA_MENU_ROWS + 1);
}

static void dispatch(pa_hub_t *hub, pa_key_t key)
{
    switch (hub->page) {
    case PA_PAGE_MENU:
        if (key == PA_KEY_UP) menu_move(hub, -1);
        else if (key == PA_KEY_DOWN) menu_move(hub, 1);
        else { hub->game = hub->cursor; go(hub, PA_PAGE_BRIEF); }
        return;
    case PA_PAGE_BRIEF:
        if (key == PA_KEY_OK || key == PA_KEY_OK2) start_run(hub);
        else go(hub, PA_PAGE_MENU);
        return;
    case PA_PAGE_PLAY:
        game_at(hub->game)->key(&hub->run, key);
        if (hub->run.over) { settle(hub); go(hub, PA_PAGE_OVER); }
        return;
    default:
        if (key == PA_KEY_OK || key == PA_KEY_OK2) start_run(hub);
        else go(hub, PA_PAGE_MENU);
        return;
    }
}

void pa_hub_key(pa_hub_t *hub, pa_key_t key)
{
    if (hub->guard_ms) return;
    if (!pa_hub_wants_click(hub)) {
        dispatch(hub, key == PA_KEY_OK2 ? PA_KEY_OK : key);
        return;
    }
    /* 单击先压住,等一小会儿看有没有第二下,免得双击被当成两次单击。 */
    if (key == PA_KEY_OK) {
        hub->click_pending = true;
        hub->click_ms = PA_CLICK_WAIT_MS;
        return;
    }
    if (key == PA_KEY_OK2) {
        hub->click_pending = false;
        hub->click_ms = 0;
        dispatch(hub, PA_KEY_OK2);
        return;
    }
    dispatch(hub, key);
}

void pa_hub_long(pa_hub_t *hub, pa_key_t key)
{
    /* 二十四个游戏用单按滚太慢,大厅里长按上下直接翻一页。 */
    if (hub->page == PA_PAGE_MENU && key != PA_KEY_OK) {
        menu_move(hub, key == PA_KEY_UP ? -PA_MENU_ROWS : PA_MENU_ROWS);
        return;
    }
    if (hub->page == PA_PAGE_PLAY) settle(hub);
    go(hub, PA_PAGE_MENU);
}

void pa_hub_tick(pa_hub_t *hub, uint32_t ms)
{
    if (hub->guard_ms) hub->guard_ms = (uint16_t)(hub->guard_ms > ms ? hub->guard_ms - ms : 0);
    if (hub->click_pending) {
        hub->click_ms = (uint16_t)(hub->click_ms > ms ? hub->click_ms - ms : 0);
        if (!hub->click_ms) {
            hub->click_pending = false;
            dispatch(hub, PA_KEY_OK);
        }
    }
    if (hub->page != PA_PAGE_PLAY) return;
    game_at(hub->game)->tick(&hub->run, ms);
    hub->run.elapsed_ms += ms;
    if (hub->run.over) { settle(hub); go(hub, PA_PAGE_OVER); }
}

/* 三颗星:拿到的画实心,没拿到的画空心。 */
static void star_text(char out[12], unsigned stars)
{
    out[0] = '\0';
    for (unsigned i = 0; i < 3; i++) {
        const char *glyph = (i < stars) ? "★" : "☆";
        size_t used = strlen(out);
        snprintf(out + used, 12 - used, "%s", glyph);
    }
}

static void draw_menu(const pa_hub_t *hub, pa_scene_t *scene)
{
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "共 %u 星   第 %u 个", pa_hub_stars(hub), (unsigned)(hub->cursor + 1U));
    for (unsigned row = 0; row < PA_MENU_ROWS; row++) {
        unsigned index = hub->top + row;
        if (index >= PA_GAME_COUNT) break;
        const pa_game_t *game = game_at(index);
        bool picked = (index == hub->cursor);
        int y = ROW_TOP + (int)row * (ROW_H + ROW_GAP);
        pa_frect(scene, 0, y, PA_FIELD_W, ROW_H, picked ? PA_YELLOW : PA_PAPER, 4);
        if (picked) pa_frect(scene, 0, y, 4, ROW_H, PA_RED, 0);
        pa_text(scene, 6, PA_FIELD_Y + y + 2, 84, game->name, PA_INK, PA_FONT_ZH, PA_LEFT);
        pa_text(scene, 92, PA_FIELD_Y + y + 6, 34, game->genre, PA_SLATE, PA_FONT_ZH12, PA_LEFT);
        char stars[12];
        star_text(stars, pa_game_stars(index, best_of(hub, index)));
        pa_text(scene, 128, PA_FIELD_Y + y + 2, 62, stars, PA_ORANGE, PA_FONT_ZH, PA_RIGHT);
        pa_text(scene, 6, PA_FIELD_Y + y + 22, 118, game->hint, PA_SLATE, PA_FONT_ZH12, PA_LEFT);
        pa_textf(scene, 126, PA_FIELD_Y + y + 22, 64, PA_SLATE, PA_FONT_ZH12, PA_RIGHT,
                 "最好 %u", (unsigned)best_of(hub, index));
    }
    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "%u 个游戏   玩过 %u 局", PA_GAME_COUNT, (unsigned)hub->record.plays);
    pa_footer(scene, "上下选　长按翻页　确定进");
}

static void draw_brief(const pa_hub_t *hub, pa_scene_t *scene)
{
    const pa_game_t *game = game_at(hub->game);
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "%s   %s", game->name, game->genre);
    pa_frect(scene, 0, 4, PA_FIELD_W, 148, PA_PAPER, 4);
    char stars[12];
    star_text(stars, pa_game_stars(hub->game, best_of(hub, hub->game)));
    pa_textf(scene, 6, PA_FIELD_Y + 12, 186, PA_ORANGE, PA_FONT_ZH, PA_CENTER,
             "%s   最好 %u", stars, (unsigned)best_of(hub, hub->game));
    for (unsigned i = 0; i < 3; i++)
        pa_text(scene, 4, PA_FIELD_Y + 40 + (int)i * 20, 190, game->rule[i],
                PA_INK, PA_FONT_ZH12, PA_LEFT);
    pa_textf(scene, 4, PA_FIELD_Y + 104, 190, PA_GREEN, PA_FONT_ZH12, PA_LEFT,
             "星级 %u / %u / %u", (unsigned)game->star[0], (unsigned)game->star[1],
             (unsigned)game->star[2]);
    pa_text(scene, 4, PA_FIELD_Y + 124, 190, game->keys, PA_SLATE, PA_FONT_ZH12, PA_LEFT);
    pa_text(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, "长按确定随时回大厅",
            PA_INK, PA_FONT_ZH, PA_CENTER);
    pa_footer(scene, "确定开始　上下返回");
}

static void draw_over(const pa_hub_t *hub, pa_scene_t *scene)
{
    const pa_game_t *game = game_at(hub->game);
    pa_textf(scene, 0, PA_ROW_TOP, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "%s   本局结束", game->name);
    pa_frect(scene, 0, 4, PA_FIELD_W, 148, PA_PAPER, 4);
    pa_text(scene, 0, PA_FIELD_Y + 14, PA_VIEW_W,
            hub->run.note ? hub->run.note : "这一局结束了", PA_SLATE, PA_FONT_ZH, PA_CENTER);
    pa_num(scene, 0, PA_FIELD_Y + 44, PA_VIEW_W, hub->run.score, PA_INK,
           PA_FONT_NUM20, PA_CENTER);
    pa_text(scene, 0, PA_FIELD_Y + 72, PA_VIEW_W, "本局得分", PA_SLATE, PA_FONT_ZH, PA_CENTER);
    char stars[12];
    star_text(stars, hub->earned);
    pa_text(scene, 0, PA_FIELD_Y + 96, PA_VIEW_W, stars, PA_ORANGE, PA_FONT_ZH, PA_CENTER);
    if (hub->new_best)
        pa_text(scene, 0, PA_FIELD_Y + 122, PA_VIEW_W, "新纪录",
                PA_GREEN, PA_FONT_ZH, PA_CENTER);
    else
        pa_textf(scene, 0, PA_FIELD_Y + 122, PA_VIEW_W, PA_SLATE, PA_FONT_ZH, PA_CENTER,
                 "最好 %u", (unsigned)best_of(hub, hub->game));
    pa_textf(scene, 0, PA_ROW_BOTTOM, PA_VIEW_W, PA_INK, PA_FONT_ZH, PA_CENTER,
             "共 %u 星   玩过 %u 局", pa_hub_stars(hub), (unsigned)hub->record.plays);
    pa_footer(scene, "确定再来一局　上下回大厅");
}

void pa_hub_draw(const pa_hub_t *hub, pa_scene_t *scene)
{
    pa_scene_reset(scene);
    switch (hub->page) {
    case PA_PAGE_MENU:  draw_menu(hub, scene); break;
    case PA_PAGE_BRIEF: draw_brief(hub, scene); break;
    case PA_PAGE_OVER:  draw_over(hub, scene); break;
    default:            game_at(hub->game)->draw(&hub->run, scene); break;
    }
}
