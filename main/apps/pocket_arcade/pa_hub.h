#pragma once
#include "pa_game.h"

/* 大厅状态机:选游戏、看玩法、玩、结算。和游戏本体一样是纯 C,
   所以整条路径都能在主机上回归。 */

enum {
    PA_MENU_ROWS = 4,             /* 一屏显示几个游戏 */
    PA_STAR_TOTAL = PA_GAME_COUNT * 3,
    PA_GUARD_MS = 300,            /* 换页后先不接按键,免得同一次按下被用两次 */
    PA_CLICK_WAIT_MS = 260,       /* 等一等看是不是双击 */
    PA_SAVE_SIZE = 2 + PA_GAME_COUNT * 2 + 2,
};

typedef enum { PA_PAGE_MENU = 0, PA_PAGE_BRIEF, PA_PAGE_PLAY, PA_PAGE_OVER } pa_page_t;

typedef struct {
    uint16_t best[PA_GAME_COUNT];
    uint16_t plays;
} pa_record_t;

typedef struct {
    pa_page_t page;
    uint8_t cursor, top, game;
    pa_run_t run;
    pa_record_t record;
    uint32_t rng;
    uint16_t guard_ms, click_ms;
    bool click_pending, record_dirty, new_best;
    uint8_t earned;               /* 上一局拿到的星 */
} pa_hub_t;

/* 大厅列表里的第 index 个游戏。 */
const pa_game_t *pa_game_at(unsigned index);

void pa_hub_init(pa_hub_t *hub, const pa_record_t *record, uint32_t seed);
void pa_hub_key(pa_hub_t *hub, pa_key_t key);
void pa_hub_long(pa_hub_t *hub, pa_key_t key);
void pa_hub_tick(pa_hub_t *hub, uint32_t ms);
void pa_hub_draw(const pa_hub_t *hub, pa_scene_t *scene);

/* 玩这一关时确定键该取单击还是按下沿。 */
bool pa_hub_wants_click(const pa_hub_t *hub);
unsigned pa_hub_stars(const pa_hub_t *hub);
unsigned pa_game_stars(unsigned game, long score);

void pa_record_encode(const pa_record_t *record, uint8_t *out);
bool pa_record_decode(pa_record_t *record, const uint8_t *in);
