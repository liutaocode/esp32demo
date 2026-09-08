#pragma once
#include "pa_scene.h"
#include <stdbool.h>
#include <stdint.h>

/* 一个小游戏 = 四个纯函数 + 一份状态。全部与 ESP-IDF/LVGL 无关。
   时间统一用毫秒推进,位置需要小数时用"毫像素"定点:
   速度写成"像素/秒"时,速度乘毫秒正好等于毫像素位移。 */

typedef enum { PA_KEY_UP = 0, PA_KEY_DOWN, PA_KEY_OK, PA_KEY_OK2 } pa_key_t;

/* 机器上三颗键从头到尾的排列就是:上、下、确定。凡是把三行、三轨、三个洞
   直接对应三个键的游戏,屏幕上从上到下的第 n 行必须是手上的第 n 颗键 ——
   照着"确定在中间"摆的话,第二行对应的是第三颗键,手会一直按错。
   所有这类游戏都走下面这两个函数,不要各自写各自的映射。 */
enum { PA_KEY_ROWS = 3 };
extern const char *const PA_KEY_LABEL[PA_KEY_ROWS];   /* 屏幕上的键位小标签 */
pa_key_t pa_key_of_row(unsigned row);
unsigned pa_row_of_key(pa_key_t key);

/* PA_OK_PRESS:确定键取按下沿,延迟最低,动作游戏用。
   PA_OK_CLICK:确定键取单击/双击,回合制游戏才用得起这 200 毫秒。 */
typedef enum { PA_OK_PRESS = 0, PA_OK_CLICK } pa_ok_mode_t;

/* ---- 1 蛇行小道 ---- */
enum { PA_SNAKE_COLS = 18, PA_SNAKE_ROWS = 14, PA_SNAKE_MAX = 96 };
typedef struct {
    uint8_t x[PA_SNAKE_MAX], y[PA_SNAKE_MAX];
    uint16_t length;
    uint8_t dir, turned;         /* 0 右 1 下 2 左 3 上;turned 保证一拍只转一次 */
    uint8_t food_x, food_y, food_gold, eaten;
    uint16_t step_ms, gold_ms;
    uint32_t acc_ms;
} pa_snake_t;

/* ---- 2 方块塔 ---- */
enum { PA_TET_COLS = 8, PA_TET_ROWS = 14 };
typedef struct {
    uint8_t cell[PA_TET_ROWS][PA_TET_COLS];   /* 0 空,1..7 方块颜色 */
    uint8_t piece, next, rot;
    int8_t px, py;
    uint16_t lines, level, fall_ms, flash_ms;
    uint8_t flash_rows;          /* 刚消掉几行,用于底栏提示 */
    uint32_t acc_ms;
} pa_tetris_t;

/* ---- 3 数字合并 ---- */
typedef struct {
    uint8_t cell[4][4];          /* 指数:0 空,1 表示 2,2 表示 4 …… */
    uint16_t moves;
    uint8_t top;                 /* 最大指数 */
    bool reached;                /* 是否已经合出 2048 */
} pa_merge_t;

/* ---- 4 飞天小鸟 ---- */
enum { PA_BIRD_PIPES = 3, PA_BIRD_GAP = 62 };
typedef struct {
    int32_t y_m;                 /* 毫像素 */
    int16_t vy;                  /* 像素每秒 */
    int32_t pipe_x_m[PA_BIRD_PIPES];
    int16_t gap_y[PA_BIRD_PIPES];
    uint8_t passed[PA_BIRD_PIPES];
    uint16_t speed, flap_ms, bob_ms;
    bool flying;                 /* 第一次按键之前既不落也不撞 */
} pa_bird_t;

/* ---- 5 恐龙快跑 ---- */
enum { PA_DINO_OBS = 4, PA_DINO_GROUND = 128 };
typedef struct {
    int32_t y_m;                 /* 相对地面的高度,向上为负 */
    int16_t vy;
    int32_t ox_m[PA_DINO_OBS];
    uint8_t kind[PA_DINO_OBS];   /* 0 空 1 小柱 2 大柱 3 飞鸟 */
    uint16_t speed, duck_ms, step_ms;
    int32_t dist_m;
} pa_dino_t;

/* ---- 6 三键节拍 ---- */
enum { PA_TILE_MAX = 10, PA_TILE_ROWS = 3, PA_TILE_HIT_X = 24, PA_TILE_HIT_W = 34 };
typedef struct {
    int32_t x[PA_TILE_MAX];      /* 毫像素 */
    uint8_t row[PA_TILE_MAX], kind[PA_TILE_MAX], live[PA_TILE_MAX];
    uint16_t speed, combo, best_combo, gap_ms, lives;
    uint32_t spawn_ms;
    int8_t flash_row;
    uint16_t flash_ms;
    bool flash_good;
} pa_tiles_t;

/* ---- 7 打地鼠 ---- */
typedef struct {
    uint8_t kind[3];             /* 0 空 1 普通 2 金 3 炸弹 */
    uint16_t live_ms[3], show_ms[3];
    uint32_t left_ms, spawn_ms;
    uint16_t gap_ms, combo, best_combo, hits, misses;
    int8_t flash_hole;
    int16_t flash_delta;
    uint16_t flash_ms;
} pa_mole_t;

/* ---- 8 太空守卫 ---- */
enum { PA_INV_COLS = 5, PA_INV_ROWS = 3, PA_INV_BOMBS = 3 };
typedef struct {
    uint8_t alive[PA_INV_ROWS][PA_INV_COLS];
    int16_t fleet_x, fleet_y;
    int8_t fleet_dir;
    uint8_t ship_col, lives, wave;
    int32_t shot_y_m;
    int8_t shot_col;             /* -1 表示没有子弹在飞 */
    int32_t bomb_y_m[PA_INV_BOMBS];
    int8_t bomb_col[PA_INV_BOMBS];
    uint16_t bomb_gap_ms, hit_ms;
    uint32_t march_ms, drop_ms, bomb_ms;
} pa_invader_t;

/* ---- 9 跳一跳 ---- */
enum { PA_JUMP_HOME_X = 40 };
typedef struct {
    uint8_t phase;               /* 0 蓄力 1 飞行 2 落点结算 */
    uint16_t power;              /* 0..100,来回摆动的力度条 */
    int8_t power_dir;
    uint8_t here_w, next_w;
    int16_t next_x;              /* 下一块平台左边缘 */
    int16_t land_x;              /* 本次落点 */
    uint16_t fly_ms, fly_total, hold_ms, combo;
    bool perfect, missed;
} pa_jump_t;

/* ---- 10 推箱子 ---- */
enum { PA_SOK_W = 9, PA_SOK_H = 8, PA_SOK_UNDO = 40, PA_SOK_LEVELS = 200 };
enum { PA_SOK_WALL = 1, PA_SOK_GOAL = 2, PA_SOK_BOX = 4 };
typedef struct {
    uint8_t cell[PA_SOK_H][PA_SOK_W];
    uint8_t px, py, dir;         /* 0 右 1 下 2 左 3 上 */
    uint8_t level, cleared;
    uint16_t moves, total_moves;
    uint8_t undo[PA_SOK_UNDO], undos;   /* 低 2 位方向,bit2 表示这步推了箱子 */
    uint16_t clear_ms;
} pa_sokoban_t;

/* ---- 11 二十一点 ---- */
enum { PA_CARD_HAND = 8, PA_CARD_HANDS = 12 };
typedef struct {
    uint8_t player[PA_CARD_HAND], dealer[PA_CARD_HAND], pn, dn;
    uint16_t chips, bet;
    uint8_t hand, phase;         /* 0 下注 1 要牌 2 结算 */
    int16_t delta;
    uint8_t result;              /* 0 平 1 赢 2 输 3 黑杰克 */
    bool hidden;                 /* 庄家暗牌是否还盖着 */
} pa_cards_t;

/* ---- 12 弹球对战 ---- */
enum { PA_PONG_PAD = 44, PA_PONG_STEP = 15 };
typedef struct {
    int32_t bx_m, by_m;
    int16_t vx, vy;              /* 像素每秒 */
    int16_t px, ax;              /* 玩家 / 电脑挡板左边缘 */
    uint8_t you, cpu, round;
    uint16_t rally, best_rally, serve_ms;
    int8_t serve_dir;
} pa_pong_t;

/* ---- 13 四子棋 ---- */
enum { PA_FOUR_W = 7, PA_FOUR_H = 6 };
typedef struct {
    uint8_t cell[PA_FOUR_H][PA_FOUR_W];   /* 0 空 1 你 2 电脑 */
    uint8_t col, round, depth, turn;      /* turn:0 轮到你,1 轮到电脑 */
    uint8_t result;                       /* 0 进行中 1 你赢 2 电脑赢 3 平局 */
    int8_t last_col;
    uint16_t think_ms;
} pa_four_t;

/* ---- 14 黑白棋 ---- */
typedef struct {
    uint8_t cell[8][8];                   /* 0 空 1 你(黑) 2 电脑(白) */
    uint8_t legal[64], count, pick;       /* 合法落子点,存 行*8+列 */
    uint8_t turn, passes, round;
    int8_t last;
    uint16_t think_ms;
} pa_reversi_t;

/* ---- 15 扫雷 ---- */
enum { PA_MINE_W = 6, PA_MINE_H = 6, PA_MINE_BOMB = 1, PA_MINE_OPEN = 2, PA_MINE_FLAG = 4 };
typedef struct {
    uint8_t cell[PA_MINE_H][PA_MINE_W];
    uint8_t cx, cy, mines, board, opened;
    bool seeded, boom;
    uint16_t clear_ms;
} pa_mines_t;

/* ---- 16 数字华容道 ---- */
typedef struct {
    uint8_t tile[16];                     /* 0 表示空格 */
    uint8_t option[4], count, pick;       /* 当前可以推动的方块下标 */
    uint16_t moves, board, clear_ms;
} pa_slide_t;

/* ---- 17 汉诺塔 ---- */
enum { PA_HANOI_MAX = 7 };
typedef struct {
    uint8_t peg[3][PA_HANOI_MAX], height[3];
    uint8_t cursor, held, disks, cleared;
    uint16_t moves, clear_ms;
} pa_hanoi_t;

/* ---- 18 青蛙过河 ---- */
enum { PA_FROG_LANES = 7 };
typedef struct {
    int32_t offset_m[PA_FROG_LANES];
    int16_t speed[PA_FROG_LANES];
    uint8_t pattern[PA_FROG_LANES];
    int16_t fx;
    uint8_t row, lives, round;
    uint16_t hit_ms, safe_ms;
} pa_frog_t;

/* ---- 19 保龄球 ---- */
typedef struct {
    uint16_t pins;                        /* 十个瓶的位掩码 */
    uint8_t frame, ball, rolls[21], rolled, phase;
    int16_t aim, aim_dir, power, power_dir;
    uint16_t roll_ms, hold_ms;
    int16_t landed;
} pa_bowl_t;

/* ---- 20 弹弓打靶 ---- */
typedef struct {
    uint8_t phase, shot, hits;
    int16_t angle, angle_dir, power, power_dir;
    int32_t x_m, y_m;
    int16_t vx, vy;
    int16_t target_x, target_y, target_w;
    int8_t wind;
    uint16_t hold_ms;
    bool hit;
} pa_sling_t;

/* ---- 21 反应力 ---- */
enum { PA_REACT_ROUNDS = 5 };
typedef struct {
    uint8_t phase, round;
    uint16_t wait_ms, spent_ms, hold_ms;
    uint16_t result[PA_REACT_ROUNDS];
    bool jumped;
} pa_react_t;

/* ---- 22 记忆翻牌 ---- */
typedef struct {
    uint8_t face[16], state[16];          /* state:0 盖着 1 翻开 2 已配对 */
    uint8_t cx, cy, board, matched;
    int8_t first, second;
    uint16_t moves, flip_ms, clear_ms;
} pa_pairs_t;

/* ---- 23 打砖块 ---- */
enum { PA_BRICK_W = 8, PA_BRICK_ROWS = 4 };
typedef struct {
    uint8_t brick[PA_BRICK_ROWS][PA_BRICK_W];
    int32_t bx_m, by_m;
    int16_t vx, vy, px;
    uint8_t lives, level;
    uint16_t serve_ms;
} pa_brick_t;

/* ---- 24 猜数字 ---- */
enum { PA_GUESS_TRIES = 8, PA_GUESS_LEN = 4 };
typedef struct {
    uint8_t secret[PA_GUESS_LEN], entry[PA_GUESS_LEN];
    uint8_t guess[PA_GUESS_TRIES][PA_GUESS_LEN];
    uint8_t bulls[PA_GUESS_TRIES], cows[PA_GUESS_TRIES];
    uint8_t digit, tries, round;
    bool solved;
    uint16_t clear_ms;
} pa_guess_t;

/* ---- 25 点灯 ---- */
typedef struct {
    uint8_t cell[5];                      /* 每行 5 位,1 表示亮着 */
    uint8_t cx, cy, level;
    uint16_t moves, budget, clear_ms;
} pa_lights_t;

/* ---- 26 记忆音阶 ---- */
enum { PA_SIMON_MAX = 40 };
typedef struct {
    uint8_t seq[PA_SIMON_MAX];
    uint8_t length, step, phase, round;   /* phase:0 展示 1 跟按 2 对了 3 错了 */
    uint16_t timer_ms;
    int8_t lit;
} pa_simon_t;

/* ---- 27 五子棋 ---- */
enum { PA_GOMOKU = 9 };
typedef struct {
    uint8_t cell[PA_GOMOKU][PA_GOMOKU];   /* 0 空 1 你 2 电脑 */
    uint8_t cx, cy, turn, result, round;
    int8_t last;
    uint16_t think_ms;
} pa_gomoku_t;

/* ---- 28 快速判断 ---- */
typedef struct {
    uint16_t left, right, shown;
    uint8_t op, correct;
    uint32_t time_ms;
    uint16_t combo, best_combo, hits, misses, flash_ms;
    int8_t flash;
} pa_mathq_t;

/* ---- 29 数织 ---- */
enum { PA_NONO = 7, PA_NONO_CLUES = 3, PA_NONO_BOARDS = 8 };
typedef struct {
    uint8_t target[PA_NONO], fill[PA_NONO], cross[PA_NONO];   /* 每行 7 位 */
    uint8_t row_clue[PA_NONO][PA_NONO_CLUES], col_clue[PA_NONO][PA_NONO_CLUES];
    uint8_t cx, cy, board, picture;
    uint16_t paints, budget, clear_ms;
} pa_nono_t;

/* ---- 30 孔明棋 ---- */
enum { PA_PEG_W = 7, PA_PEG_H = 7, PA_PEG_MOVES = 48 };
enum { PA_PEG_VOID = 0, PA_PEG_HOLE = 1, PA_PEG_STONE = 2 };
typedef struct {
    uint8_t cell[PA_PEG_H][PA_PEG_W];
    uint8_t from[PA_PEG_MOVES], dir[PA_PEG_MOVES], count, pick;
    uint8_t pegs;
    uint16_t jumps, clear_ms;
} pa_peg_t;

/* ---- 31 诗词接句 ---- */
enum { PA_POEM_OPTIONS = 4 };
typedef struct {
    uint16_t verse, option[PA_POEM_OPTIONS];
    uint8_t answer, pick, lives, phase;   /* phase:0 答题 1 亮答案 */
    uint16_t asked, right, combo, hold_ms;
    bool correct;
} pa_poem_t;

/* ---- 32 常识问答 ---- */
typedef struct {
    uint16_t question;
    uint8_t order[3];                     /* 三个选项摆在哪一行 */
    uint8_t pick, lives, phase;
    uint16_t asked, right, combo, hold_ms;
    bool correct;
} pa_quiz_t;

/* ---- 一局的公共状态 ---- */
typedef struct {
    uint32_t rng;
    long score;
    uint32_t elapsed_ms;
    bool over;
    const char *note;            /* 结算页写一句为什么结束 */
    union {
        pa_snake_t snake;
        pa_tetris_t tetris;
        pa_merge_t merge;
        pa_bird_t bird;
        pa_dino_t dino;
        pa_tiles_t tiles;
        pa_mole_t mole;
        pa_invader_t invader;
        pa_jump_t jump;
        pa_sokoban_t sokoban;
        pa_cards_t cards;
        pa_pong_t pong;
        pa_four_t four;
        pa_reversi_t reversi;
        pa_mines_t mines;
        pa_slide_t slide;
        pa_hanoi_t hanoi;
        pa_frog_t frog;
        pa_bowl_t bowl;
        pa_sling_t sling;
        pa_react_t react;
        pa_pairs_t pairs;
        pa_brick_t brick;
        pa_guess_t guess;
        pa_lights_t lights;
        pa_simon_t simon;
        pa_gomoku_t gomoku;
        pa_mathq_t mathq;
        pa_nono_t nono;
        pa_peg_t peg;
        pa_poem_t poem;
        pa_quiz_t quiz;
    } u;
} pa_run_t;

typedef struct {
    /* 存档按 id 索引,不按大厅里的位置,所以调整摆放顺序不会打乱谁的纪录。
       id 一旦分配就不再改动;新游戏往后接。 */
    uint8_t id;
    const char *name;            /* 大厅里的名字 */
    const char *genre;           /* 两个字的分类 */
    const char *hint;            /* 一句话玩法 */
    const char *rule[3];         /* 玩法页的三行说明 */
    const char *keys;            /* 底栏按键提示 */
    uint8_t ok_mode;             /* pa_ok_mode_t */
    uint16_t star[3];            /* 一星 / 二星 / 三星的分数门槛 */
    void (*reset)(pa_run_t *run);
    void (*tick)(pa_run_t *run, uint32_t ms);
    void (*key)(pa_run_t *run, pa_key_t key);
    void (*draw)(const pa_run_t *run, pa_scene_t *scene);
} pa_game_t;

#define PA_GAME_COUNT 32

/* 推箱子的关卡表,主机测试里的求解器直接读它。 */
extern const char PA_SOK_LEVEL[PA_SOK_LEVELS][PA_SOK_H][PA_SOK_W + 1];

extern const pa_game_t pa_game_snake;
extern const pa_game_t pa_game_tetris;
extern const pa_game_t pa_game_merge;
extern const pa_game_t pa_game_bird;
extern const pa_game_t pa_game_dino;
extern const pa_game_t pa_game_tiles;
extern const pa_game_t pa_game_mole;
extern const pa_game_t pa_game_invader;
extern const pa_game_t pa_game_jump;
extern const pa_game_t pa_game_sokoban;
extern const pa_game_t pa_game_cards;
extern const pa_game_t pa_game_pong;
extern const pa_game_t pa_game_four;
extern const pa_game_t pa_game_reversi;
extern const pa_game_t pa_game_mines;
extern const pa_game_t pa_game_slide;
extern const pa_game_t pa_game_hanoi;
extern const pa_game_t pa_game_frog;
extern const pa_game_t pa_game_bowl;
extern const pa_game_t pa_game_sling;
extern const pa_game_t pa_game_react;
extern const pa_game_t pa_game_pairs;
extern const pa_game_t pa_game_brick;
extern const pa_game_t pa_game_guess;
extern const pa_game_t pa_game_lights;
extern const pa_game_t pa_game_simon;
extern const pa_game_t pa_game_gomoku;
extern const pa_game_t pa_game_mathq;
extern const pa_game_t pa_game_nono;
extern const pa_game_t pa_game_peg;
extern const pa_game_t pa_game_poem;
extern const pa_game_t pa_game_quiz;
