#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 一挤就过：果冻体积守恒，按键把它拉高或压扁，闸门落下时形状要能穿过洞口。
 * 这里只有整数运算，不依赖 ESP-IDF 与 LVGL，可在主机上完整回归。 */
enum {
    JS_SHAPES      = 5,     /* 超扁 / 扁 / 圆 / 高 / 超高 */
    JS_CURVE       = 7,     /* 形变曲线锚点数：五种体型两端各多留一档 */
    JS_FLAVORS     = 8,
    JS_FIELD_W     = 220,   /* 面板内可用宽度 */
    JS_ARENA_Y     = 24,    /* 赛道在面板内的顶边 */
    JS_ARENA_H     = 178,
    JS_GROUND      = 166,   /* 赛道内的地面线 */
    JS_GATE_H      = 118,   /* 闸门整块高度 */
    JS_GATE_PEEK   = 12,    /* 闸门起始露出的高度 */
    JS_AREA        = 3600,  /* 果冻面积：宽 × 高 恒定 */
    JS_SHAPE_LIMIT = 3000,  /* 形变行程上限（千分档） */
    JS_ANCHOR      = 1000,  /* 相邻体型之间的档距 */
    JS_PERFECT     = 150,   /* 判定“刚刚好”的档位误差 */
    JS_STEP_MS     = 20,    /* 弹性模拟固定步长 */
    JS_PASS_MS     = 420,
    JS_FAIL_MS     = 760,
    JS_MAX_LEVEL   = 99,
    JS_SAVE_SIZE   = 12,    /* 写进 NVS 的进度块长度 */
};

typedef enum { JS_HOME, JS_PLAY, JS_PASS, JS_FAIL, JS_PAUSED, JS_RESULT } js_page_t;

typedef struct { int16_t w, h; } js_size_t;

/* 跨局保留的进度：累计过关数决定解锁到第几只果冻。 */
typedef struct { uint32_t total_cleared; uint16_t best_relaxed, best_dash; } js_progress_t;

typedef struct {
    js_page_t page, resume;
    uint8_t mode;          /* 0 悠着点：三条命、洞口宽；1 一口气：一条命、洞口紧 */
    uint8_t flavor;        /* 当前果冻编号 */
    uint8_t unlocked;      /* 已解锁果冻数量，至少 1 */
    uint8_t lives, saves;
    uint8_t shape_index;   /* 本关洞口对应的体型 0..4 */
    uint8_t combo, best_combo;
    uint16_t level;        /* 本局已经遇到的闸门数 */
    uint16_t cleared, perfects;
    uint16_t challenge;
    uint32_t seed;
    uint32_t score;
    uint32_t best[2];
    uint32_t total_cleared;
    int32_t target, shape, vel;   /* 形变档位与速度，单位为千分档 */
    uint32_t gate_ms, gate_span, phase_ms, carry_ms;
    bool passed, perfect, new_best, new_unlock;
} js_state_t;

void js_home(js_state_t *s);
void js_start(js_state_t *s, uint16_t challenge);
void js_tick(js_state_t *s, uint32_t elapsed_ms);
bool js_press(js_state_t *s, int direction);   /* +1 拉高，-1 压扁 */
bool js_steady(js_state_t *s);                 /* 稳住：立刻止住晃动 */
void js_pause(js_state_t *s);
bool js_pick_flavor(js_state_t *s, int direction);

void js_load(js_state_t *s, js_progress_t saved);
js_progress_t js_save(const js_state_t *s);

js_size_t js_shape_of(unsigned index);          /* 标准体型的宽高 */
js_size_t js_body(const js_state_t *s);         /* 果冻当前宽高 */
js_size_t js_gate(const js_state_t *s);         /* 洞口宽高 */
int js_gate_bottom(const js_state_t *s);        /* 闸门底边在赛道内的 y */
int js_wobble(const js_state_t *s);             /* 晃动强度 0..100，用于表情 */
uint8_t js_unlocked_count(uint32_t cleared);
void js_encode(js_progress_t progress, uint8_t out[JS_SAVE_SIZE]);
bool js_decode(js_progress_t *progress, const uint8_t in[JS_SAVE_SIZE]);
unsigned js_unlock_need(const js_state_t *s);   /* 距离下一只还差几关，全开为 0 */
