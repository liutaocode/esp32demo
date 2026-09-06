/* 俄罗斯方块状态机回归测试:形状表、七连袋、重力、锁定宽限、消行动画、
   长按撤销、计分、冲刺收尾与结束判定。 */
#include "clean_sweep_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void run(cs_state_t *s, unsigned ms)
{
    for (unsigned i = 0; i < ms; i += 20) cs_tick(s, 20);
}

static unsigned shape_cells(uint8_t piece, uint8_t rot)
{
    unsigned n = 0;
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            if (cs_shape_cell(piece, rot, row, col)) n++;
    return n;
}

static void test_shapes(void)
{
    for (uint8_t piece = 0; piece < CS_PIECES; piece++)
        for (uint8_t rot = 0; rot < CS_ROTATIONS; rot++) {
            assert(shape_cells(piece, rot) == 4);
            /* 每个朝向都必须能在出生列放下,否则开局就死。 */
            cs_state_t probe;
            memset(&probe, 0, sizeof(probe));
            assert(cs_fits(&probe, piece, rot, CS_SPAWN_X, 1));
        }
    /* 越界访问返回空格,不读到表外。 */
    assert(!cs_shape_cell(CS_PIECES, 0, 0, 0));
    assert(!cs_shape_cell(0, CS_ROTATIONS, 0, 0));
    assert(!cs_shape_cell(0, 0, -1, 0));
    assert(!cs_shape_cell(0, 0, 0, 4));
}

/* 每七个方块里七种各一次。 */
static void test_bag(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 7);
    unsigned seen[CS_PIECES] = {0};
    seen[s.piece]++;
    seen[s.next]++;
    for (unsigned i = 0; i < 5; i++) {
        cs_drop(&s);
        while (s.page == CS_CLEARING) cs_tick(&s, 20);
        assert(s.page == CS_PLAY);
        seen[s.next]++;
    }
    for (unsigned i = 0; i < CS_PIECES; i++) assert(seen[i] == 1);
}

/* 同一个题号必须给出同一串方块。 */
static void test_same_challenge(void)
{
    cs_state_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    cs_start(&a, 4242);
    cs_start(&b, 4242);
    for (unsigned i = 0; i < 30; i++) {
        assert(a.piece == b.piece && a.next == b.next);
        cs_drop(&a);
        cs_drop(&b);
        while (a.page == CS_CLEARING) cs_tick(&a, 20);
        while (b.page == CS_CLEARING) cs_tick(&b, 20);
        if (a.page != CS_PLAY) break;
    }
    assert(a.page == b.page && a.score == b.score);
    cs_state_t c;
    memset(&c, 0, sizeof(c));
    cs_start(&c, 4243);
    assert(c.challenge == 4243 && c.rng != a.rng);
    cs_start(&c, CS_CHALLENGE_MAX + 5);
    assert(c.challenge == 5);
}

/* 重力按等级间隔下落,触底后还有锁定宽限,移动会重置宽限但次数有上限。 */
static void test_gravity_and_lock(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 1);
    assert(cs_fall_interval(&s) == CS_FALL_MS[0]);
    int8_t start_y = s.py;
    run(&s, CS_FALL_MS[0]);
    assert(s.py == start_y + 1);

    cs_drop(&s);
    while (s.page == CS_CLEARING) cs_tick(&s, 20);
    uint32_t serial = s.piece_serial;
    while (cs_fits(&s, s.piece, s.rot, s.px, s.py + 1)) s.py++;
    run(&s, CS_LOCK_MS - 100);
    assert(s.piece_serial == serial);
    for (unsigned i = 0; i < CS_LOCK_RESET_MAX + 4; i++) {
        cs_move(&s, -1);
        cs_move(&s, 1);
        cs_tick(&s, 100);
    }
    assert(s.piece_serial > serial); /* 宽限重置次数用完后照样锁定 */
}

/* 旋转带踢墙:贴边和刚出生的长条都能转起来。 */
static void test_rotate(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 11);
    s.piece = 0;              /* 长条出生时顶在第 0 行,只有向下让一行才立得起来 */
    s.rot = 0;
    s.px = CS_SPAWN_X;
    s.py = -1;
    assert(cs_rotate(&s));
    assert(s.rot == 1 && s.py >= 0);

    /* 贴着左墙也能转,踢墙会把方块推回场内。 */
    memset(&s, 0, sizeof(s));
    cs_start(&s, 12);
    s.piece = 0;
    s.rot = 1;
    s.px = -2;
    s.py = 2;
    assert(cs_fits(&s, s.piece, s.rot, s.px, s.py));
    assert(cs_rotate(&s));
    assert(cs_fits(&s, s.piece, s.rot, s.px, s.py));

    /* 只剩一列宽的竖井:竖着的长条转不动,踢墙也救不了,方块保持原样。 */
    memset(&s, 0, sizeof(s));
    cs_start(&s, 13);
    for (int row = 0; row < CS_ROWS; row++)
        for (int col = 0; col < CS_COLS; col++)
            if (col != 4) s.cell[row][col] = 1;
    s.piece = 0;
    s.rot = 1;
    s.px = 2;                 /* 竖长条落在第 4 列 */
    s.py = 2;
    assert(cs_fits(&s, s.piece, s.rot, s.px, s.py));
    uint8_t rot = s.rot;
    int8_t px = s.px, py = s.py;
    assert(!cs_rotate(&s));
    assert(s.rot == rot && s.px == px && s.py == py);
}

/* 落底把方块贴到最下面,落点与虚影一致,并按行数加分。 */
static void test_drop(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 5);
    int ghost = cs_ghost_y(&s);
    uint32_t rows = (uint32_t)(ghost - s.py);
    assert(rows > 0);
    cs_drop(&s);
    assert(s.score == rows * 2);
    assert(s.pieces == 1);
    /* 棋盘最底下一定有格子。 */
    unsigned filled = 0;
    for (int col = 0; col < CS_COLS; col++)
        if (s.cell[CS_ROWS - 1][col]) filled++;
    assert(filled > 0);
}

/* 铺满一整行:动画依次走三个阶段,结束后整行消失、上方落下。 */
static void test_clear_line(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 3);
    for (int col = 0; col < CS_COLS; col++) s.cell[CS_ROWS - 1][col] = 1;
    s.cell[CS_ROWS - 1][4] = 0;
    s.cell[CS_ROWS - 2][0] = 2; /* 上方留一格,验证塌落 */
    s.piece = 0;                /* 长条竖着塞进空洞 */
    s.rot = 1;
    s.px = 2;
    s.py = 0;
    cs_drop(&s);
    assert(s.page == CS_CLEARING && s.clear_count == 1);
    assert(cs_row_clearing(&s, CS_ROWS - 1));
    assert(s.gain == 100);
    assert(s.lines == 1 && s.combo == 1);
    assert(s.fx == CS_FX_FLASH && cs_fx_progress(&s) == 0);
    run(&s, CS_FLASH_MS);
    assert(s.fx == CS_FX_BURST);
    run(&s, CS_BURST_MS);
    assert(s.fx == CS_FX_COLLAPSE);
    assert(cs_row_shift(&s, CS_ROWS - 2) == 1);
    assert(cs_row_shift(&s, CS_ROWS - 1) == 0);
    run(&s, CS_COLLAPSE_MS);
    assert(s.page == CS_PLAY && s.clear_count == 0);
    assert(s.cell[CS_ROWS - 1][0] == 2); /* 上面那一格落到了底 */
    assert(s.cell[CS_ROWS - 1][4] == 1);  /* 长条剩下的三格也跟着落下 */
    for (int col = 1; col < CS_COLS; col++)
        if (col != 4) assert(!s.cell[CS_ROWS - 1][col]);
}

/* 四行同时消:分数按等级倍率,全清另有奖励,连击累加。 */
static void test_scoring(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 9);
    for (int row = CS_ROWS - 4; row < CS_ROWS; row++)
        for (int col = 0; col < CS_COLS; col++)
            s.cell[row][col] = col == 9 ? 0 : 3;
    s.piece = 0;
    s.rot = 1;
    s.px = 7; /* 竖长条落在最右列 */
    s.py = 0;
    s.score = 0;
    cs_drop(&s);
    assert(s.clear_count == 4 && s.perfect);
    /* 800 分基础 + 1000 全清,等级 0 倍率 1。 */
    assert(s.gain == 1800);
    assert(s.lines == 4 && s.combo == 1 && s.best_combo == 1);
    while (s.page == CS_CLEARING) cs_tick(&s, 20);
    assert(cs_top_row(&s) == CS_ROWS);

    /* 连续两次消行,第二次带连击加分。 */
    memset(&s, 0, sizeof(s));
    cs_start(&s, 9);
    s.level = 1;
    for (int col = 0; col < CS_COLS - 1; col++) s.cell[CS_ROWS - 1][col] = 3;
    s.combo = 1;
    s.piece = 0;
    s.rot = 1;
    s.px = 7;
    s.py = 0;
    cs_drop(&s);
    /* 单行 100 + 连击 50,等级 1 倍率 2,棋盘未清空所以没有全清奖励。 */
    assert(s.combo == 2 && !s.perfect && s.gain == (100 + 50) * 2);
}

/* 堆到顶:新方块放不下就结束,并记录本次开机最高分。 */
static void test_top_out(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 21);
    unsigned guard = 0;
    while (s.page == CS_PLAY && guard++ < 400) {
        cs_drop(&s);
        while (s.page == CS_CLEARING) cs_tick(&s, 20);
    }
    assert(s.page == CS_RESULT && !s.won);
    assert(s.best_score == s.score && s.best_lines == s.lines);
    uint32_t first = s.score;
    cs_home(&s);
    assert(s.page == CS_HOME);
    cs_start(&s, 21);
    assert(s.best_score == first && s.score == 0);
}

/* 冲刺模式清满四十行即胜利,用时更短才刷新纪录。 */
static void test_sprint(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    s.mode = CS_SPRINT;
    cs_start(&s, 33);
    s.lines = CS_SPRINT_LINES - 1;
    s.elapsed_ms = 60000;
    for (int col = 0; col < CS_COLS - 1; col++) s.cell[CS_ROWS - 1][col] = 3;
    s.piece = 0;
    s.rot = 1;
    s.px = 7;
    s.py = 0;
    cs_drop(&s);
    while (s.page == CS_CLEARING) cs_tick(&s, 20);
    assert(s.page == CS_RESULT && s.won && s.new_best);
    assert(s.best_sprint_ms == 60000);

    uint32_t best = s.best_sprint_ms;
    cs_start(&s, 33);
    assert(s.best_sprint_ms == best && s.elapsed_ms == 0);
}

/* 暂停期间时间不走,棋盘不动。 */
static void test_pause(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 8);
    run(&s, 200);
    assert(cs_pause(&s));
    assert(!cs_pause(&s));
    cs_state_t frozen = s;
    run(&s, 5000);
    assert(memcmp(&frozen, &s, sizeof(frozen)) == 0);
    assert(!cs_move(&s, -1) && !cs_rotate(&s) && !cs_drop(&s));
    assert(cs_resume(&s));
    assert(!cs_resume(&s));
    assert(s.page == CS_PLAY);
}

/* 卡顿时一帧最多推进 CS_TICK_MAX_MS,不会一次掉穿棋盘。 */
static void test_tick_clamp(void)
{
    cs_state_t s;
    memset(&s, 0, sizeof(s));
    cs_start(&s, 6);
    int8_t start = s.py;
    cs_tick(&s, 100000);
    assert(s.elapsed_ms == CS_TICK_MAX_MS);
    assert(s.py <= start + 1);
    cs_tick(&s, 0);
    assert(s.elapsed_ms == CS_TICK_MAX_MS);
}

/* 长时间自动对局:棋盘不出现悬空行错位,消行后没有半截行。 */
static void test_long_game(void)
{
    for (uint32_t challenge = 0; challenge < 12; challenge++) {
        cs_state_t s;
        memset(&s, 0, sizeof(s));
        cs_start(&s, challenge * 137);
        unsigned guard = 0;
        while (s.page != CS_RESULT && guard++ < 5000) {
            if (s.page == CS_PLAY) {
                for (unsigned i = 0; i < challenge % 5; i++) cs_move(&s, -1);
                if (challenge % 3 == 0) cs_rotate(&s);
                cs_drop(&s);
            } else {
                cs_tick(&s, 20);
            }
            for (int row = 0; row < CS_ROWS; row++) {
                unsigned filled = 0;
                for (int col = 0; col < CS_COLS; col++)
                    if (s.cell[row][col]) filled++;
                assert(filled <= CS_COLS);
                if (s.page == CS_PLAY) assert(filled < CS_COLS);
            }
        }
        assert(s.page == CS_RESULT);
    }
}

int main(void)
{
    test_shapes();
    test_bag();
    test_same_challenge();
    test_gravity_and_lock();
    test_rotate();
    test_drop();
    test_clear_line();
    test_scoring();
    test_top_out();
    test_sprint();
    test_pause();
    test_tick_clamp();
    test_long_game();
    printf("Clean Sweep state: PASS\n");
    return 0;
}
