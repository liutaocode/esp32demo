/* Host regression for the Jelly Squeeze state machine: integer only, no ESP-IDF. */
#include "jelly_squeeze_state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t magnitude(int32_t v) { return v < 0 ? -v : v; }

static js_state_t fresh(uint8_t mode, uint16_t challenge)
{
    js_state_t s;
    memset(&s, 0, sizeof(s));
    js_home(&s);
    s.mode = mode;
    js_start(&s, challenge);
    return s;
}

/* 把形变推到某个档位并等它停稳，模拟玩家提前动手的正常打法。 */
static void settle_to(js_state_t *s, int steps_target, unsigned settle_ms)
{
    while (s->target / JS_ANCHOR < steps_target && js_press(s, 1)) { }
    while (s->target / JS_ANCHOR > steps_target && js_press(s, -1)) { }
    for (unsigned i = 0; i < settle_ms / JS_STEP_MS && s->page == JS_PLAY; i++) js_tick(s, JS_STEP_MS);
}

static void test_shape_table(void)
{
    for (unsigned i = 0; i < JS_SHAPES; i++) {
        js_size_t a = js_shape_of(i);
        assert(a.w > 0 && a.h > 0);
        /* 体积守恒：宽 × 高 与 JS_AREA 的偏差只来自整数取整。 */
        assert(magnitude(a.w * a.h - JS_AREA) <= a.w);
        if (i) assert(a.w < js_shape_of(i - 1).w && a.h > js_shape_of(i - 1).h);
    }
    /* 五种体型两两对称：最扁与最高互为宽高。 */
    assert(js_shape_of(0).w == js_shape_of(4).h && js_shape_of(0).h == js_shape_of(4).w);
    assert(js_shape_of(1).w == js_shape_of(3).h && js_shape_of(1).h == js_shape_of(3).w);
    assert(js_shape_of(2).w == js_shape_of(2).h);

    /* 形变曲线连续、单调，档位落在锚点时正好是标准体型。 */
    js_state_t s = fresh(0, 1);
    int previous = 0;
    for (int32_t x = -JS_SHAPE_LIMIT; x <= JS_SHAPE_LIMIT; x++) {
        s.shape = x;
        js_size_t body = js_body(&s);
        assert(body.w >= 30 && body.w <= 110 && body.h >= 30 && body.h <= 110);
        if (x > -JS_SHAPE_LIMIT) assert(body.w <= previous && previous - body.w <= 1);
        previous = body.w;
        for (unsigned i = 0; i < JS_SHAPES; i++)
            if (x == ((int32_t)i - 2) * JS_ANCHOR) assert(body.w == js_shape_of(i).w);
    }
    /* 两端各留一档：压过头或拉过头都会比任何洞口更宽或更高。 */
    s.shape = -JS_SHAPE_LIMIT;
    assert(js_body(&s).w > js_shape_of(0).w);
    s.shape = JS_SHAPE_LIMIT;
    assert(js_body(&s).h > js_shape_of(JS_SHAPES - 1).h);
}

static void test_spring(void)
{
    for (int steps = -2; steps <= 2; steps++) {
        js_state_t s = fresh(0, 7);
        s.gate_span = 1000000;      /* 冻住闸门，只观察形变 */
        int32_t want = steps * JS_ANCHOR;
        while (s.target != want) assert(js_press(&s, s.target < want ? 1 : -1));
        int32_t peak = 0;
        for (unsigned i = 0; i < 60; i++) {
            js_tick(&s, JS_STEP_MS);
            assert(magnitude(s.shape) <= JS_SHAPE_LIMIT);
            int32_t over = magnitude(s.shape - want);
            if (over > peak) peak = over;
        }
        /* 一定会超调（看得见的晃动），也一定会在一秒内停稳。 */
        if (steps) assert(peak > 100);
        assert(magnitude(s.shape - want) <= JS_PERFECT / 2);
        assert(magnitude(s.vel) < 20);
    }
    /* 档位到顶后再按没有效果。 */
    js_state_t s = fresh(0, 9);
    for (int i = 0; i < 2; i++) assert(js_press(&s, 1));
    assert(!js_press(&s, 1) && s.target == 2 * JS_ANCHOR);
    for (int i = 0; i < 4; i++) assert(js_press(&s, -1));
    assert(!js_press(&s, -1) && s.target == -2 * JS_ANCHOR);
}

static void test_gate_travel(void)
{
    js_state_t s = fresh(0, 3);
    int previous = js_gate_bottom(&s);
    assert(previous == JS_GATE_PEEK);
    while (s.page == JS_PLAY) {
        js_tick(&s, JS_STEP_MS);
        int bottom = js_gate_bottom(&s);
        assert(bottom >= previous && bottom <= JS_GROUND);
        previous = bottom;
    }
    assert(previous == JS_GROUND);
    /* 反馈期间闸门贴住地面不动。 */
    assert(js_gate_bottom(&s) == JS_GROUND);
    /* 门洞永远比赛道窄，闸门也永远画得下最高的门洞。 */
    for (unsigned mode = 0; mode < 2; mode++)
        for (unsigned i = 0; i < JS_SHAPES; i++) {
            js_state_t g = fresh((uint8_t)mode, 5);
            g.shape_index = (uint8_t)i;
            js_size_t hole = js_gate(&g), want = js_shape_of(i);
            assert(hole.w > want.w && hole.h > want.h);
            assert(hole.w < JS_FIELD_W - 20 && hole.h < JS_GATE_H - 10);
            /* 一口气的门洞一定比悠着点紧。 */
            js_state_t easy = g; easy.mode = 0;
            if (mode) assert(hole.w < js_gate(&easy).w && hole.h < js_gate(&easy).h);
        }
}

static void test_judging(void)
{
    /* 停在正确体型上必定过门且算刚刚好；停在相邻体型上一定不算刚刚好。 */
    for (unsigned mode = 0; mode < 2; mode++)
        for (unsigned i = 0; i < JS_SHAPES; i++) {
            js_state_t s = fresh((uint8_t)mode, 21);
            s.shape_index = (uint8_t)i;
            settle_to(&s, (int)i - 2, 900);
            assert(s.page == JS_PLAY);
            while (s.page == JS_PLAY) js_tick(&s, JS_STEP_MS);
            assert(s.page == JS_PASS && s.passed && s.perfect);
            assert(s.cleared == 1 && s.perfects == 1 && s.combo == 1 && s.score == 25);
        }
    /* 一口气模式下差一档就过不去。 */
    for (unsigned i = 0; i < JS_SHAPES; i++)
        for (int delta = -1; delta <= 1; delta += 2) {
            int wanted = (int)i - 2 + delta;
            if (wanted < -2 || wanted > 2) continue;
            js_state_t s = fresh(1, 33);
            s.shape_index = (uint8_t)i;
            settle_to(&s, wanted, 900);
            while (s.page == JS_PLAY) js_tick(&s, JS_STEP_MS);
            assert(s.page == JS_FAIL && !s.passed && s.lives == 0);
        }
    /* 还在晃的时候撞门：可能过，但绝不算刚刚好。 */
    js_state_t s = fresh(0, 44);
    s.shape_index = 2;
    s.shape = 0; s.target = 0; s.vel = 0;
    s.gate_ms = s.gate_span - 60;
    assert(js_press(&s, 1));
    while (s.page == JS_PLAY) js_tick(&s, JS_STEP_MS);
    assert(!s.perfect);
}

static void test_steady(void)
{
    js_state_t s = fresh(0, 8);
    assert(s.saves == 3);
    assert(js_press(&s, 1));
    js_tick(&s, 40);
    assert(s.vel != 0 && s.shape != s.target);
    assert(js_steady(&s));
    assert(s.vel == 0 && s.shape == s.target && s.saves == 2);
    assert(js_steady(&s) && js_steady(&s));
    assert(s.saves == 0 && !js_steady(&s));
    /* 一口气只有两次。 */
    js_state_t d = fresh(1, 8);
    assert(d.saves == 2 && d.lives == 1);
    /* 非游戏页不消耗次数。 */
    js_state_t h;
    memset(&h, 0, sizeof(h));
    js_home(&h);
    assert(!js_steady(&h));
}

static void test_pause_and_pages(void)
{
    js_state_t s = fresh(0, 12);
    js_tick(&s, 200);
    js_pause(&s);
    assert(s.page == JS_PAUSED);
    js_state_t frozen = s;
    js_tick(&s, 5000);
    assert(memcmp(&frozen, &s, sizeof(frozen)) == 0);
    assert(!js_press(&s, 1) && !js_steady(&s));
    js_pause(&s);
    assert(s.page == JS_PLAY);
    /* 首页与结算页上的暂停没有意义。 */
    js_home(&s);
    js_pause(&s);
    assert(s.page == JS_HOME);
    /* 反馈页也能暂停并原样恢复。 */
    js_start(&s, 12);
    while (s.page == JS_PLAY) js_tick(&s, JS_STEP_MS);
    js_page_t feedback = s.page;
    js_pause(&s);
    assert(s.page == JS_PAUSED && s.resume == feedback);
    js_pause(&s);
    assert(s.page == feedback);
}

static void test_progress_and_unlocks(void)
{
    assert(js_unlocked_count(0) == 1);
    uint8_t previous = 1;
    for (uint32_t cleared = 0; cleared <= 400; cleared++) {
        uint8_t open = js_unlocked_count(cleared);
        assert(open >= previous && open >= 1 && open <= JS_FLAVORS);
        previous = open;
    }
    assert(js_unlocked_count(400) == JS_FLAVORS);

    js_state_t s;
    memset(&s, 0, sizeof(s));
    js_home(&s);
    for (uint32_t cleared = 0; cleared <= 400; cleared++) {
        js_progress_t p = {cleared, 0, 0};
        js_load(&s, p);
        unsigned need = js_unlock_need(&s);
        assert(s.flavor < s.unlocked);
        if (s.unlocked == JS_FLAVORS) assert(need == 0);
        else assert(need > 0 && need <= 90);
    }

    /* 换口味只在已解锁范围内循环。 */
    js_progress_t three = {30, 0, 0};
    js_load(&s, three);
    assert(s.unlocked == 3);
    for (unsigned i = 0; i < 9; i++) {
        assert(js_pick_flavor(&s, 1));
        assert(s.flavor < 3);
    }
    assert(s.flavor == 0);
    assert(js_pick_flavor(&s, -1) && s.flavor == 2);
    js_progress_t one = {0, 0, 0};
    js_load(&s, one);
    assert(!js_pick_flavor(&s, 1) && s.flavor == 0);

    /* 存档编解码：往返一致，任何一位被改坏都会被拒绝。 */
    js_progress_t cases[] = {{0, 0, 0}, {1, 2, 3}, {70000, 65535, 40000}, {0xFFFFFFFFu, 65535, 65535}};
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint8_t blob[JS_SAVE_SIZE];
        js_encode(cases[i], blob);
        js_progress_t back;
        memset(&back, 0, sizeof(back));
        assert(js_decode(&back, blob));
        assert(back.total_cleared == cases[i].total_cleared);
        assert(back.best_relaxed == cases[i].best_relaxed && back.best_dash == cases[i].best_dash);
        for (unsigned bit = 0; bit < JS_SAVE_SIZE * 8; bit++) {
            uint8_t broken[JS_SAVE_SIZE];
            memcpy(broken, blob, sizeof(broken));
            broken[bit / 8] ^= (uint8_t)(1u << (bit % 8));
            js_progress_t ignored;
            assert(!js_decode(&ignored, broken));
        }
    }
}

static void test_run_shape(void)
{
    /* 同一题号重来必须逐关一致；换题号则不同。 */
    uint8_t first[40], again[40], other[40];
    for (unsigned pass = 0; pass < 3; pass++) {
        js_state_t s = fresh(0, pass == 2 ? 4321 : 1234);
        uint8_t *out = pass == 0 ? first : (pass == 1 ? again : other);
        memset(out, 0xFF, 40);
        for (unsigned i = 0; i < 40; i++) {
            if (s.page == JS_RESULT) break;
            out[i] = s.shape_index;
            settle_to(&s, (int)s.shape_index - 2, 0);
            unsigned guard = 0;
            while (s.page == JS_PLAY && guard++ < 20000) js_tick(&s, JS_STEP_MS);
            guard = 0;
            while (s.page != JS_PLAY && s.page != JS_RESULT && guard++ < 20000) js_tick(&s, JS_STEP_MS);
        }
    }
    assert(memcmp(first, again, sizeof(first)) == 0);
    assert(memcmp(first, other, sizeof(first)) != 0);
    /* 连续两关的洞口一定不同，每关都得动手。 */
    for (unsigned i = 1; i < 40 && first[i] != 0xFF; i++) assert(first[i] != first[i - 1]);
    assert(first[3] != 0xFF);

    /* 一直不动手：三条命耗光后进结算，得分为零。 */
    js_state_t idle = fresh(0, 77);
    unsigned guard = 0;
    while (idle.page != JS_RESULT && guard++ < 100000) js_tick(&idle, JS_STEP_MS);
    assert(idle.page == JS_RESULT && idle.lives == 0 && idle.score == 0 && idle.cleared == 0);
    assert(!idle.new_best && idle.best[0] == 0);

    /* 会读洞口的玩家能一路刷分，纪录会更新。 */
    js_state_t good = fresh(0, 77);
    guard = 0;
    while (good.page != JS_RESULT && guard++ < 100000) {
        if (good.page == JS_PLAY) settle_to(&good, (int)good.shape_index - 2, 700);
        js_tick(&good, JS_STEP_MS);
    }
    assert(good.page == JS_RESULT && good.cleared > 10 && good.score > 200);
    assert(good.new_best && good.best[0] == good.score && good.best_combo >= 3);
    /* 开新一局保留纪录与累计进度，清空本局数据。 */
    uint32_t best = good.best[0], total = good.total_cleared;
    js_start(&good, 78);
    assert(good.best[0] == best && good.total_cleared == total);
    assert(good.score == 0 && good.cleared == 0 && good.level == 0 && good.combo == 0);
}

static void test_timing_robustness(void)
{
    /* 一次补上很久的时间也不能跳过反馈页，闸门不会瞬移到脸上。 */
    js_state_t s = fresh(0, 15);
    js_tick(&s, 100000);
    assert(s.page == JS_PLAY || s.page == JS_PASS || s.page == JS_FAIL);
    assert(js_gate_bottom(&s) <= JS_GROUND);

    /* 逐毫秒推进与整步推进得到同样的局面。 */
    js_state_t fine = fresh(0, 16), coarse = fresh(0, 16);
    for (unsigned i = 0; i < 400; i++) {
        for (unsigned k = 0; k < JS_STEP_MS; k++) js_tick(&fine, 1);
        js_tick(&coarse, JS_STEP_MS);
    }
    assert(memcmp(&fine, &coarse, sizeof(fine)) == 0);

    /* 关卡上限：即使一直过关也会收在结算页。 */
    js_state_t s2 = fresh(0, 17);
    unsigned guard = 0;
    while (s2.page != JS_RESULT && guard++ < 400000) {
        if (s2.page == JS_PLAY) { s2.shape = s2.target = ((int32_t)s2.shape_index - 2) * JS_ANCHOR; s2.vel = 0; }
        js_tick(&s2, JS_STEP_MS);
    }
    assert(s2.page == JS_RESULT && s2.level < JS_MAX_LEVEL);
}

static void test_fuzz(void)
{
    /* 随机乱按不能把状态弄到自相矛盾。 */
    uint32_t rng = 2463534242u;
    for (unsigned seed = 0; seed < 400; seed++) {
        js_state_t s = fresh((uint8_t)(seed & 1u), (uint16_t)(seed * 37u));
        for (unsigned turn = 0; turn < 3000; turn++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            switch (rng % 6u) {
            case 0: js_press(&s, 1); break;
            case 1: js_press(&s, -1); break;
            case 2: js_steady(&s); break;
            case 3: js_pause(&s); break;
            default: js_tick(&s, 1u + rng % 90u); break;
            }
            assert(s.page <= JS_RESULT);
            assert(magnitude(s.shape) <= JS_SHAPE_LIMIT);
            assert(s.target >= -2 * JS_ANCHOR && s.target <= 2 * JS_ANCHOR);
            assert(s.lives <= 3 && s.saves <= 3);
            assert(s.cleared <= s.level + 1u && s.perfects <= s.cleared);
            assert(s.combo <= s.best_combo);
            assert(s.level < JS_MAX_LEVEL);
            js_size_t body = js_body(&s);
            assert(body.w > 0 && body.h > 0 && body.w < JS_FIELD_W && body.h < JS_ARENA_H);
            int bottom = js_gate_bottom(&s);
            assert(bottom >= 0 && bottom <= JS_GROUND);
            if (s.page == JS_RESULT) break;
        }
    }
}

int main(void)
{
    test_shape_table();
    test_spring();
    test_gate_travel();
    test_judging();
    test_steady();
    test_pause_and_pages();
    test_progress_and_unlocks();
    test_run_shape();
    test_timing_robustness();
    test_fuzz();
    printf("Jelly Squeeze state: PASS\n");
    return 0;
}
