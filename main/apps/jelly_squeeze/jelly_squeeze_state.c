#include "jelly_squeeze_state.h"

#include <string.h>

/* 形变曲线：七个锚点覆盖 -3000..3000 档，中间五个才是闸门会要求的标准体型。
 * 两端各留一档，压过头或拉过头都会撞上闸门，五种洞口因此都是双侧判定。
 * 宽度取自表格，高度由 JS_AREA / 宽度 得到，果冻体积保持恒定。 */
static const int16_t CURVE_W[JS_CURVE] = {104, 86, 72, 60, 50, 42, 35};
/* 解锁第 n 只果冻所需的累计过关数。 */
static const uint16_t UNLOCK_AT[JS_FLAVORS] = {0, 12, 30, 55, 90, 140, 210, 300};

/* 弹簧参数按 20 毫秒步长选定：周期约 340 毫秒，超调约四分之一，半秒内稳定。
 * 玩家因此必须提前一点动手，等果冻停稳，而不是贴着闸门临时改形。 */
enum { SPRING_K = 137, SPRING_C = 240 };

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    return x ^ (x >> 16);
}

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static int32_t magnitude(int32_t v) { return v < 0 ? -v : v; }

js_size_t js_shape_of(unsigned index)
{
    if (index >= JS_SHAPES) index = JS_SHAPES - 1;
    int16_t w = CURVE_W[index + 1];
    return (js_size_t){w, (int16_t)((JS_AREA + w / 2) / w)};
}

/* 形变档位到宽度：相邻锚点之间线性插值。 */
static int width_at(int32_t x)
{
    x = clamp32(x, -JS_SHAPE_LIMIT, JS_SHAPE_LIMIT);
    int32_t t = x + JS_SHAPE_LIMIT;
    int seg = (int)(t / JS_ANCHOR);
    if (seg > JS_CURVE - 2) seg = JS_CURVE - 2;
    int32_t a = CURVE_W[seg], b = CURVE_W[seg + 1];
    return (int)(a + (b - a) * (t - (int32_t)seg * JS_ANCHOR) / JS_ANCHOR);
}

js_size_t js_body(const js_state_t *s)
{
    int32_t x = (s->page == JS_HOME || s->page == JS_RESULT) ? 0 : s->shape;
    int16_t w = (int16_t)width_at(x);
    return (js_size_t){w, (int16_t)((JS_AREA + w / 2) / w)};
}

js_size_t js_gate(const js_state_t *s)
{
    js_size_t want = js_shape_of(s->shape_index);
    int16_t margin = s->mode ? 4 : 8;
    return (js_size_t){(int16_t)(want.w + margin), (int16_t)(want.h + margin)};
}

int js_gate_bottom(const js_state_t *s)
{
    /* 闸门一出现就露出 JS_GATE_PEEK 像素，门洞宽度从第一帧起就能读出来。 */
    if (!s->gate_span) return JS_GATE_PEEK;
    uint32_t p = s->gate_ms >= s->gate_span ? 1000u : s->gate_ms * 1000u / s->gate_span;
    return JS_GATE_PEEK + (JS_GROUND - JS_GATE_PEEK) * (int)p / 1000;
}

int js_wobble(const js_state_t *s)
{
    int32_t drift = magnitude(s->shape - s->target) + magnitude(s->vel) * 3;
    return (int)(drift > 1000 ? 100 : drift / 10);
}

uint8_t js_unlocked_count(uint32_t cleared)
{
    uint8_t n = 1;
    for (uint8_t i = 1; i < JS_FLAVORS; i++) if (cleared >= UNLOCK_AT[i]) n = (uint8_t)(i + 1);
    return n;
}

unsigned js_unlock_need(const js_state_t *s)
{
    /* 由累计过关数直接推算，即使 unlocked 落后一步也不会算出负数。 */
    uint8_t open = js_unlocked_count(s->total_cleared);
    if (open >= JS_FLAVORS) return 0;
    return UNLOCK_AT[open] - (unsigned)s->total_cleared;
}

void js_load(js_state_t *s, js_progress_t saved)
{
    s->total_cleared = saved.total_cleared;
    s->best[0] = saved.best_relaxed;
    s->best[1] = saved.best_dash;
    s->unlocked = js_unlocked_count(s->total_cleared);
    if (s->flavor >= s->unlocked) s->flavor = (uint8_t)(s->unlocked - 1);
}

js_progress_t js_save(const js_state_t *s)
{
    return (js_progress_t){s->total_cleared,
        (uint16_t)(s->best[0] > 65535u ? 65535u : s->best[0]),
        (uint16_t)(s->best[1] > 65535u ? 65535u : s->best[1])};
}

void js_home(js_state_t *s)
{
    s->page = JS_HOME;
    s->shape = s->target = s->vel = 0;
    s->carry_ms = s->phase_ms = 0;
    if (!s->unlocked) s->unlocked = 1;
}

/* 闸门从出现到贴地的时间：关数越高越快，到达下限后不再压缩。 */
static uint32_t span_for(const js_state_t *s)
{
    uint32_t base = s->mode ? 1800u : 2400u;
    uint32_t floor_ms = s->mode ? 520u : 640u;
    uint32_t drop = s->level * 90u;
    return drop + floor_ms >= base ? floor_ms : base - drop;
}

/* 下一道洞口：由题号与关数决定，且一定与上一道不同，逼玩家每关都动手。 */
static void next_gate(js_state_t *s)
{
    uint32_t r = mix(s->seed + s->level * 7919u);
    s->shape_index = (uint8_t)((s->shape_index + 1u + r % (JS_SHAPES - 1)) % JS_SHAPES);
    s->gate_span = span_for(s);
    s->gate_ms = 0;
    s->phase_ms = 0;
    s->page = JS_PLAY;
}

void js_start(js_state_t *s, uint16_t challenge)
{
    uint32_t best0 = s->best[0], best1 = s->best[1], total = s->total_cleared;
    uint8_t mode = s->mode ? 1 : 0, flavor = s->flavor, unlocked = s->unlocked;
    memset(s, 0, sizeof(*s));
    s->best[0] = best0; s->best[1] = best1; s->total_cleared = total;
    s->mode = mode; s->flavor = flavor; s->unlocked = unlocked ? unlocked : 1;
    s->challenge = (uint16_t)(challenge % 10000u);
    s->seed = mix(s->challenge + 1u);
    s->lives = mode ? 1 : 3;
    s->saves = mode ? 2 : 3;
    s->shape_index = 2;
    next_gate(s);
}

bool js_press(js_state_t *s, int direction)
{
    if (s->page != JS_PLAY && s->page != JS_PASS) return false;
    int32_t next = s->target + (direction > 0 ? JS_ANCHOR : -JS_ANCHOR);
    if (next > 2 * JS_ANCHOR || next < -2 * JS_ANCHOR) return false;
    s->target = next;
    return true;
}

bool js_steady(js_state_t *s)
{
    if (s->page != JS_PLAY || !s->saves) return false;
    s->saves--;
    s->shape = s->target;
    s->vel = 0;
    return true;
}

void js_pause(js_state_t *s)
{
    if (s->page == JS_PAUSED) { s->page = s->resume; return; }
    if (s->page == JS_PLAY || s->page == JS_PASS || s->page == JS_FAIL) {
        s->resume = s->page;
        s->page = JS_PAUSED;
    }
}

bool js_pick_flavor(js_state_t *s, int direction)
{
    if (s->page != JS_HOME || s->unlocked <= 1) return false;
    int next = (int)s->flavor + (direction > 0 ? 1 : s->unlocked - 1);
    s->flavor = (uint8_t)(next % s->unlocked);
    return true;
}

static void finish(js_state_t *s)
{
    s->page = JS_RESULT;
    s->new_best = s->score > s->best[s->mode];
    if (s->new_best) s->best[s->mode] = s->score;
}

static void resolve(js_state_t *s)
{
    js_size_t body = js_body(s), gate = js_gate(s);
    int32_t anchor = ((int32_t)s->shape_index - 2) * JS_ANCHOR;
    s->passed = body.w <= gate.w && body.h <= gate.h;
    s->perfect = s->passed && magnitude(s->shape - anchor) <= JS_PERFECT;
    s->phase_ms = 0;
    if (s->passed) {
        s->cleared++;
        s->total_cleared++;
        if (s->perfect) {
            s->perfects++;
            if (s->combo < 255) s->combo++;
            if (s->combo > s->best_combo) s->best_combo = s->combo;
            s->score += 20u + (s->combo > 6 ? 6u : s->combo) * 5u;
        } else {
            s->combo = 0;
            s->score += 10u;
        }
        /* 穿过去以后抖一下，看得见也提示形状还没停稳。 */
        s->vel += s->perfect ? 260 : 150;
        s->page = JS_PASS;
        uint8_t open = js_unlocked_count(s->total_cleared);
        if (open > s->unlocked) { s->unlocked = open; s->new_unlock = true; }
    } else {
        s->combo = 0;
        if (s->lives) s->lives--;
        s->page = JS_FAIL;
    }
}

static void advance(js_state_t *s)
{
    bool failed = s->page == JS_FAIL;
    s->phase_ms = 0;
    if (failed) { s->shape = s->target = 0; s->vel = 0; }
    if (!s->lives || s->level + 1 >= JS_MAX_LEVEL) { finish(s); return; }
    s->level++;
    next_gate(s);
}

static void spring(js_state_t *s)
{
    s->vel += (s->target - s->shape) * SPRING_K / 1000;
    s->vel -= s->vel * SPRING_C / 1000;
    s->shape += s->vel;
    if (s->shape > JS_SHAPE_LIMIT) { s->shape = JS_SHAPE_LIMIT; if (s->vel > 0) s->vel = 0; }
    if (s->shape < -JS_SHAPE_LIMIT) { s->shape = -JS_SHAPE_LIMIT; if (s->vel < 0) s->vel = 0; }
}

static void step(js_state_t *s)
{
    spring(s);
    if (s->page == JS_PLAY) {
        s->gate_ms += JS_STEP_MS;
        if (s->gate_ms >= s->gate_span) { s->gate_ms = s->gate_span; resolve(s); }
        return;
    }
    s->phase_ms += JS_STEP_MS;
    if (s->phase_ms >= (s->page == JS_PASS ? (uint32_t)JS_PASS_MS : (uint32_t)JS_FAIL_MS)) advance(s);
}

void js_tick(js_state_t *s, uint32_t elapsed_ms)
{
    if (s->page != JS_PLAY && s->page != JS_PASS && s->page != JS_FAIL) return;
    s->carry_ms += elapsed_ms;
    /* 调度卡顿后不补跑大量步：宁可慢一点，也不要闸门瞬移到脸上。 */
    if (s->carry_ms > 400u) s->carry_ms = 400u;
    while (s->carry_ms >= (uint32_t)JS_STEP_MS &&
           (s->page == JS_PLAY || s->page == JS_PASS || s->page == JS_FAIL)) {
        s->carry_ms -= JS_STEP_MS;
        step(s);
    }
}

/* NVS 里的进度块：魔数、版本、三个计数和 Fletcher-16 校验。
 * 读到损坏内容就当作全新存档，绝不清空这块分区里别人的数据。 */
static uint16_t fletcher(const uint8_t *bytes, unsigned length)
{
    uint16_t low = 0, high = 0;
    for (unsigned i = 0; i < length; i++) {
        low = (uint16_t)((low + bytes[i]) % 255u);
        high = (uint16_t)((high + low) % 255u);
    }
    return (uint16_t)((high << 8) | low);
}

void js_encode(js_progress_t progress, uint8_t out[JS_SAVE_SIZE])
{
    out[0] = 0x4A; out[1] = 1;
    for (unsigned i = 0; i < 4; i++) out[2 + i] = (uint8_t)((progress.total_cleared >> (i * 8)) & 0xFF);
    out[6] = (uint8_t)(progress.best_relaxed & 0xFF);
    out[7] = (uint8_t)(progress.best_relaxed >> 8);
    out[8] = (uint8_t)(progress.best_dash & 0xFF);
    out[9] = (uint8_t)(progress.best_dash >> 8);
    uint16_t sum = fletcher(out, 10);
    out[10] = (uint8_t)(sum & 0xFF);
    out[11] = (uint8_t)(sum >> 8);
}

bool js_decode(js_progress_t *progress, const uint8_t in[JS_SAVE_SIZE])
{
    if (in[0] != 0x4A || in[1] != 1) return false;
    uint16_t sum = fletcher(in, 10);
    if (in[10] != (uint8_t)(sum & 0xFF) || in[11] != (uint8_t)(sum >> 8)) return false;
    uint32_t cleared = 0;
    for (unsigned i = 0; i < 4; i++) cleared |= (uint32_t)in[2 + i] << (i * 8);
    progress->total_cleared = cleared;
    progress->best_relaxed = (uint16_t)(in[6] | ((uint16_t)in[7] << 8));
    progress->best_dash = (uint16_t)(in[8] | ((uint16_t)in[9] << 8));
    return true;
}
