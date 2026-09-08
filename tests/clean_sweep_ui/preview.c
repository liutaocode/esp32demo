/* Execute the production LVGL page with simulated board I/O. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps/clean_sweep/clean_sweep.c"
#include "src/misc/lv_text_private.h"

static int64_t fake_us;
static int backlight, soc = 76;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t value) { assert(!callback_active); backlight = value; }

static void bounds(lv_obj_t *o)
{
    lv_area_t a; lv_obj_get_coords(o, &a);
    assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *str = lv_label_get_text(o);
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_point_t size;
        lv_text_get_size(&size, str, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o))
            fprintf(stderr, "Text overflow: %s (%d > %d)\n", str, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        uint32_t i = 0;
        while (str[i]) {
            uint32_t cp = lv_text_encoded_next(str, &i);
            if (cp == '\n') continue;
            lv_font_glyph_dsc_t d;
            assert(lv_font_get_glyph_dsc(font, &d, cp, 0) && !d.is_placeholder);
        }
        assert(lv_obj_get_scroll_bottom(o) <= 0);
        /* 面板里的文字不能被裁掉。 */
        lv_obj_t *parent = lv_obj_get_parent(o);
        if (parent && lv_obj_get_parent(parent)) {
            lv_area_t box; lv_obj_get_content_coords(parent, &box);
            if (a.y2 > box.y2)
                fprintf(stderr, "Panel clips %s at %d > %d\n", str, (int)a.y2, (int)box.y2);
            assert(a.x1 >= box.x1 && a.x2 <= box.x2 && a.y1 >= box.y1 && a.y2 <= box.y2);
        }
        /* Product UI must not leak English words. */
        for (const char *c = str; *c; c++) assert(!(*c >= 'A' && *c <= 'Z') && !(*c >= 'a' && *c <= 'z'));
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) bounds(lv_obj_get_child(o, i));
}

static void check(void) { lv_obj_update_layout(s_screen); bounds(s_screen); }

static void snap(const char *name)
{
    check();
    lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[100]; snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%u %u\n255\n", b->header.w, b->header.h);
    for (unsigned y = 0; y < b->header.h; y++) for (unsigned x = 0; x < b->header.w; x++) {
        uint8_t *p = b->data + y * b->header.stride + x * 3;
        uint8_t rgb[] = {p[2], p[1], p[0]}; fwrite(rgb, 1, 3, f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}

static void advance(unsigned ms)
{
    for (unsigned done = 0; done < ms; done += 20) {
        fake_us += 20 * 1000;
        lv_tick_inc(20);
        frame(s_timer);
    }
}

static void post(bsp_btn_t b, bsp_btn_ev_t ev)
{
    callback_active = true; clean_sweep_key(b, ev); callback_active = false;
}
/* 方向键给按下事件,确定键给单击事件,和按键组件真实的分工一致。 */
static void key(bsp_btn_t b)
{
    post(b, b == BSP_BTN_OK ? BSP_BTN_CLICK : BSP_BTN_PRESS);
    advance(20);
}

/* 双击确定:组件对这一次按下只发双击,不会先补一次单击。 */
static void drop_ok(void) { post(BSP_BTN_OK, BSP_BTN_DOUBLE); advance(20); }

/* ---------- 自动对局:用简单启发式把整局跑完,每帧都验证排版 ---------- */

static int column_height(const cs_state_t *s, int col)
{
    for (int row = 0; row < CS_ROWS; row++)
        if (s->cell[row][col]) return CS_ROWS - row;
    return 0;
}

static int count_holes(const cs_state_t *s)
{
    int holes = 0;
    for (int col = 0; col < CS_COLS; col++) {
        bool covered = false;
        for (int row = 0; row < CS_ROWS; row++) {
            if (s->cell[row][col]) covered = true;
            else if (covered) holes++;
        }
    }
    return holes;
}

static long evaluate(const cs_state_t *src, uint8_t rot, int x)
{
    cs_state_t sim = *src;
    sim.rot = rot;
    sim.px = (int8_t)x;
    bool placed = false;
    for (int dy = 0; dy <= 3 && !placed; dy++)
        if (cs_fits(&sim, sim.piece, rot, x, src->py + dy)) {
            sim.py = (int8_t)(src->py + dy);
            placed = true;
        }
    if (!placed) return LONG_MIN;
    uint32_t before = sim.lines;
    cs_drop(&sim);
    for (unsigned guard = 0; sim.page == CS_CLEARING && guard < 200; guard++) cs_tick(&sim, 20);
    if (sim.page != CS_PLAY) return LONG_MIN / 2;
    long score = (long)(sim.lines - before) * 760 - count_holes(&sim) * 420;
    long previous = -1;
    for (int col = 0; col < CS_COLS; col++) {
        long height = column_height(&sim, col);
        score -= height * 510;
        if (previous >= 0) score -= labs(height - previous) * 180;
        previous = height;
    }
    return score;
}

static uint8_t target_rot;
static int target_x;
static uint32_t target_serial;
static unsigned target_steps;

static void plan(void)
{
    long best = LONG_MIN;
    target_rot = s_state.rot;
    target_x = s_state.px;
    for (uint8_t rot = 0; rot < CS_ROTATIONS; rot++)
        for (int x = -2; x < CS_COLS; x++) {
            long value = evaluate(&s_state, rot, x);
            if (value <= best) continue;
            best = value;
            target_rot = rot;
            target_x = x;
        }
    target_serial = s_state.piece_serial;
    target_steps = 0;
}

/* 走一步:先转到目标朝向,再左右挪到目标列,最后按住确定落底。 */
static void play_step(void)
{
    if (s_state.page != CS_PLAY) { advance(20); return; }
    if (target_serial != s_state.piece_serial) plan();
    if (target_steps++ > 24) { drop_ok(); return; }
    if (s_state.rot != target_rot) key(BSP_BTN_OK);
    else if (s_state.px > target_x) key(BSP_BTN_UP);
    else if (s_state.px < target_x) key(BSP_BTN_DOWN);
    else drop_ok();
}

/* 摆好一局棋盘,直接触发指定的消除,用来稳定地截到动画的三个阶段。 */
static void stage_clear(uint32_t challenge, unsigned filled_rows, uint8_t piece, uint8_t rot,
                        int px, bool leave_crumb)
{
    cs_start(&s_state, challenge);
    for (unsigned i = 0; i < filled_rows; i++)
        for (int col = 0; col < CS_COLS - (piece == 3 ? 2 : 1); col++)
            s_state.cell[CS_ROWS - 1 - i][col] = (uint8_t)(2 + i % 5);
    if (leave_crumb) s_state.cell[CS_ROWS - 1 - filled_rows][0] = 4;
    s_state.piece = piece;
    s_state.rot = rot;
    s_state.px = (int8_t)px;
    s_state.py = 0;
    render();
    cs_drop(&s_state);
    advance(20);
}

int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    clean_sweep_prepare(); clean_sweep_enter(true);
    assert(s_state.page == CS_HOME);
    snap("home");
    key(BSP_BTN_DOWN); assert(s_state.mode == CS_SPRINT); snap("home-sprint");
    key(BSP_BTN_UP); assert(s_state.mode == CS_CLASSIC);

    key(BSP_BTN_OK); assert(s_state.page == CS_PLAY);
    /* 确定键的按下事件不参与操作:旋转只认单击,所以按住不会先转一下。 */
    uint8_t rot = s_state.rot;
    post(BSP_BTN_OK, BSP_BTN_PRESS); post(BSP_BTN_OK, BSP_BTN_PRESS); advance(20);
    assert(s_state.rot == rot);
    key(BSP_BTN_OK); assert(s_state.rot != rot);
    key(BSP_BTN_UP); key(BSP_BTN_DOWN);
    advance(200);
    snap("play");

    /* 长按确定暂停,方向键长按不再被当成操作。 */
    int8_t before_x = s_state.px;
    post(BSP_BTN_UP, BSP_BTN_LONG); advance(20);
    assert(s_state.page == CS_PLAY && s_state.px == before_x);
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20);
    assert(s_state.page == CS_PAUSED);
    snap("paused");
    cs_state_t frozen = s_state; advance(4000);
    assert(memcmp(&frozen, &s_state, sizeof(frozen)) == 0);
    key(BSP_BTN_UP); assert(s_state.page == CS_PLAY);

    /* 双击落底:转到目标朝向后双击,方块落在虚影的位置,不会多转一次。 */
    plan();
    while (s_state.rot != target_rot) key(BSP_BTN_OK);
    uint8_t dropped_rot = s_state.rot;
    uint8_t dropped_piece = s_state.piece;
    int8_t dropped_x = s_state.px;
    int ghost = cs_ghost_y(&s_state);
    uint32_t serial = s_state.piece_serial;
    drop_ok();
    assert(s_state.piece_serial == serial + 1);
    assert(dropped_rot == target_rot);
    /* 落点那一格确实写进了棋盘。 */
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            if (cs_shape_cell(dropped_piece, dropped_rot, row, col))
                assert(s_state.cell[ghost + row][dropped_x + col] ||
                       cs_row_clearing(&s_state, ghost + row));

    /* 一整局:每一帧都验证排版,顺带截下消除动画的三个阶段。 */
    cs_start(&s_state, 128); render();
    bool shot_flash = false, shot_burst = false, shot_collapse = false;
    unsigned guard = 0;
    while (s_state.page != CS_RESULT && guard++ < 900) {
        play_step();
        if (s_state.page == CS_CLEARING) {
            if (!shot_flash && s_state.fx == CS_FX_FLASH) { shot_flash = true; snap("clear-flash"); }
            if (!shot_burst && s_state.fx == CS_FX_BURST) { shot_burst = true; snap("clear-burst"); }
            if (!shot_collapse && s_state.fx == CS_FX_COLLAPSE) {
                shot_collapse = true; snap("clear-collapse");
            }
        }
        check();
        if (s_state.lines >= 12) break;
    }
    assert(shot_flash && shot_burst && shot_collapse);
    assert(s_state.lines >= 12 && s_state.level >= 1);

    /* 四行齐消和全清各自的横幅。 */
    stage_clear(77, 4, 0, 1, 7, true);
    assert(s_state.clear_count == 4 && !s_state.perfect);
    advance(60); snap("tetris");
    while (s_state.page == CS_CLEARING) { advance(20); check(); }

    /* 消除动画期间按住确定只是等下一块,不能因此退出这一局。 */
    stage_clear(79, 1, 0, 1, 7, true);
    assert(s_state.page == CS_CLEARING);
    key(BSP_BTN_OK); drop_ok();
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20);
    assert(s_state.page == CS_CLEARING || s_state.page == CS_PLAY);
    while (s_state.page == CS_CLEARING) { advance(20); check(); }
    assert(s_state.page == CS_PLAY);

    stage_clear(78, 2, 3, 0, 7, false);
    assert(s_state.clear_count == 2 && s_state.perfect);
    advance(60); snap("perfect");
    while (s_state.page == CS_CLEARING) { advance(20); check(); }
    assert(cs_top_row(&s_state) == CS_ROWS);

    /* 结束页:堆到顶。 */
    cs_start(&s_state, 5); render();
    guard = 0;
    while (s_state.page != CS_RESULT && guard++ < 600) {
        if (s_state.page == CS_PLAY) drop_ok(); else advance(20);
        check();
    }
    assert(s_state.page == CS_RESULT && !s_state.won && s_state.best_score == s_state.score);
    snap("result");

    uint32_t challenge = s_state.challenge;
    key(BSP_BTN_UP);
    assert(s_state.page == CS_PLAY && s_state.challenge == (challenge + 1) % CS_CHALLENGE_MAX);
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20);
    assert(s_state.page == CS_PAUSED); /* 游戏中长按确定是暂停 */
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20);
    assert(s_state.page == CS_HOME);   /* 暂停页长按确定回首页 */

    /* 冲刺模式:清满四十行的结果页。 */
    cs_home(&s_state); s_state.mode = CS_SPRINT; render();
    cs_start(&s_state, 21);
    s_state.lines = CS_SPRINT_LINES - 1;
    s_state.elapsed_ms = 96400;
    for (int col = 0; col < CS_COLS - 1; col++) s_state.cell[CS_ROWS - 1][col] = 6;
    s_state.piece = 0; s_state.rot = 1; s_state.px = 7; s_state.py = 0;
    render();
    advance(20); snap("sprint-play");
    cs_drop(&s_state);
    while (s_state.page == CS_CLEARING) { advance(20); check(); }
    assert(s_state.page == CS_RESULT && s_state.won);
    snap("sprint-result");
    key(BSP_BTN_DOWN); assert(s_state.page == CS_HOME);
    snap("home-sprint-record");

    /* 每一种方块、每个朝向、每一列都留在井里。 */
    s_state.mode = CS_CLASSIC;
    cs_start(&s_state, 3); render();
    for (uint8_t piece = 0; piece < CS_PIECES; piece++)
        for (uint8_t r = 0; r < CS_ROTATIONS; r++)
            for (int x = -2; x < CS_COLS; x++) {
                if (!cs_fits(&s_state, piece, r, x, 2)) continue;
                s_state.piece = piece; s_state.rot = r; s_state.px = (int8_t)x; s_state.py = 2;
                animate(); check();
            }

    /* 唤醒、降级显示、反复退出重进。 */
    clean_sweep_exit(); clean_sweep_enter(true);
    advance(20); assert(backlight == 100);
    s_last_activity -= CS_IDLE_DIM_MS + 1; advance(20); assert(backlight == 20);
    s_last_activity -= CS_IDLE_OFF_MS + 1; advance(20); assert(backlight == 0);
    key(BSP_BTN_OK); assert(backlight == 100 && s_state.page == CS_HOME);
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20); assert(s_state.page == CS_HOME);
    /* 游戏中闲置会先暂停再变暗,方块不会在没人看的时候堆到顶。 */
    key(BSP_BTN_OK); assert(s_state.page == CS_PLAY);
    s_last_activity -= CS_IDLE_DIM_MS + 1; advance(20);
    assert(s_state.page == CS_PAUSED && backlight == 20);
    key(BSP_BTN_UP); assert(backlight == 100);

    clean_sweep_exit(); clean_sweep_enter(false);
    check(); assert(s_state.page == CS_HOME);
    key(BSP_BTN_OK); assert(s_state.page == CS_HOME); /* 按键不可用时不接受输入 */
    snap("no-buttons");
    for (unsigned i = 0; i < 50; i++) { clean_sweep_exit(); clean_sweep_enter(true); }
    check();
    clean_sweep_exit();
    printf("Clean Sweep preview: PASS\n");
    return 0;
}
