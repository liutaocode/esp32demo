#pragma once
#include <stdbool.h>
#include <stdint.h>

/* 俄罗斯方块 —— 与 ESP-IDF/LVGL 无关的纯状态机,可在主机上回归测试。
   棋盘按行列存放颜色号;时间全部用毫秒,渲染层每帧把经过的毫秒交给 cs_tick()。
   三键映射由渲染层负责:上下键按下即左右移动,确定键单击旋转、双击落底、长按暂停。
   按键组件对同一次按下只会给出单击、双击、长按三者之一,所以这三个动作互不重叠,
   状态机不需要为"按下先转了一下"做任何补偿。 */

enum {
    CS_COLS = 10,
    CS_ROWS = 20,
    CS_PIECES = 7,          /* 七种四格方块 */
    CS_ROTATIONS = 4,
    CS_MAX_CLEAR = 4,       /* 一次最多消四行 */
    CS_SPAWN_X = 3,         /* 出生时 4x4 方块框的左列 */
    CS_LOCK_MS = 500,       /* 触底后的锁定宽限,按键慢也来得及贴边 */
    CS_LOCK_RESET_MAX = 10, /* 宽限最多被移动/旋转重置的次数 */
    CS_FLASH_MS = 320,      /* 消除动画:闪烁 */
    CS_BURST_MS = 240,      /* 消除动画:炸开 */
    CS_COLLAPSE_MS = 200,   /* 消除动画:上方整体落下 */
    CS_SPRINT_LINES = 20,
    CS_MAX_LEVEL = 14,
    CS_TICK_MAX_MS = 80,    /* 卡顿时一帧最多推进的游戏时间 */
    CS_CHALLENGE_MAX = 10000,
};

typedef enum { CS_HOME, CS_PLAY, CS_CLEARING, CS_PAUSED, CS_RESULT } cs_page_t;
typedef enum { CS_FX_FLASH, CS_FX_BURST, CS_FX_COLLAPSE } cs_fx_t;
typedef enum { CS_CLASSIC, CS_SPRINT, CS_MODE_COUNT } cs_mode_t;

/* 4x4 位图,最高位是左上角:bit = 0x8000 >> (行 * 4 + 列)。 */
extern const uint16_t CS_SHAPE[CS_PIECES][CS_ROTATIONS];
extern const uint16_t CS_FALL_MS[CS_MAX_LEVEL + 1];

typedef struct {
    cs_page_t page;
    uint8_t mode;                       /* cs_mode_t */
    uint8_t cell[CS_ROWS][CS_COLS];     /* 0 空,1..7 颜色号 */
    uint8_t piece, rot, next;
    int8_t px, py;                      /* 当前方块 4x4 框的左上角 */
    uint8_t bag[CS_PIECES], bag_pos;    /* 七连袋随机,保证不会长时间缺同一种 */
    uint32_t rng, challenge;
    uint32_t drop_ms, lock_ms, elapsed_ms;
    uint32_t lock_resets;
    uint32_t score, lines, level, combo, best_combo, pieces;
    uint32_t piece_serial;              /* 每出生一个方块加一,用来识别过期的长按 */

    cs_fx_t fx;                         /* 消除动画阶段 */
    uint32_t fx_ms;
    uint8_t clear_rows[CS_MAX_CLEAR], clear_count;
    uint32_t gain;                      /* 本次消行得分,动画期间显示 */
    bool perfect;                       /* 本次消行后棋盘全空 */

    bool won, new_best;
    uint32_t best_score, best_lines, best_sprint_ms;
} cs_state_t;

void cs_home(cs_state_t *s);
void cs_start(cs_state_t *s, uint32_t challenge);
void cs_tick(cs_state_t *s, uint32_t ms);

bool cs_move(cs_state_t *s, int dir);   /* dir: -1 左, +1 右 */
bool cs_rotate(cs_state_t *s);
bool cs_drop(cs_state_t *s);            /* 落底并立刻锁定 */
bool cs_pause(cs_state_t *s);
bool cs_resume(cs_state_t *s);

bool cs_shape_cell(uint8_t piece, uint8_t rot, int row, int col);
bool cs_fits(const cs_state_t *s, uint8_t piece, uint8_t rot, int x, int y);
int cs_ghost_y(const cs_state_t *s);            /* 当前方块的落点行 */
uint32_t cs_fall_interval(const cs_state_t *s); /* 当前等级的下落间隔(毫秒) */
uint32_t cs_fx_total(const cs_state_t *s);      /* 当前动画阶段的总时长 */
unsigned cs_fx_progress(const cs_state_t *s);   /* 当前阶段进度,0..1000 */
bool cs_row_clearing(const cs_state_t *s, int row);
unsigned cs_row_shift(const cs_state_t *s, int row); /* 该行动画结束后要下移几行 */
unsigned cs_top_row(const cs_state_t *s);       /* 最高的非空行,空盘返回 CS_ROWS */
