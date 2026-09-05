/* Render the production app with real LVGL and simulated board peripherals. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "apps/fruit_merge/fruit_merge.c"
static int64_t fake_us;
static int soc = 87, backlight;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }
uint32_t esp_random(void) { return 1234; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t b) { assert(!callback_active); backlight = b; }
static void advance(unsigned ms) { fake_us += (int64_t)ms * 1000; lv_tick_inc(ms); tick(s_timer); }
static void key_event(bsp_btn_t b, bsp_btn_ev_t e)
{
    callback_active = true; fruit_merge_key(b, e); callback_active = false; advance(20);
}
static void key(bsp_btn_t b) { advance(300); key_event(b, b == BSP_BTN_OK ? BSP_BTN_CLICK : BSP_BTN_PRESS); }
static void check_labels(lv_obj_t *o)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *text = lv_label_get_text(o);
        lv_area_t a; lv_obj_get_coords(o, &a);
        assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
        if (text[0] && lv_obj_get_parent(o)) {
            lv_area_t parent; lv_obj_get_content_coords(lv_obj_get_parent(o), &parent);
            if (!(a.x1 >= parent.x1 && a.y1 >= parent.y1 && a.x2 <= parent.x2 && a.y2 <= parent.y2))
                fprintf(stderr, "Label clipped by parent: %s\n", text);
            assert(a.x1 >= parent.x1 && a.y1 >= parent.y1 && a.x2 <= parent.x2 && a.y2 <= parent.y2);
        }
        lv_point_t size;
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o)) fprintf(stderr, "Overflow: %s width=%d allowed=%d\n", text, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        for (uint32_t i = 0; text[i];) {
            uint32_t cp = lv_text_encoded_next(text, &i);
            assert(!(cp >= 'A' && cp <= 'Z') && !(cp >= 'a' && cp <= 'z'));
            lv_font_glyph_dsc_t dsc;
            assert(lv_font_get_glyph_dsc(font, &dsc, cp, 0) && !dsc.is_placeholder);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *a = lv_obj_get_child(o, i);
        if (!lv_obj_check_type(a, &lv_label_class) || !lv_label_get_text(a)[0]) continue;
        lv_area_t x; lv_obj_get_coords(a, &x);
        for (unsigned j = i + 1; j < lv_obj_get_child_count(o); j++) {
            lv_obj_t *b = lv_obj_get_child(o, j);
            if (!lv_obj_check_type(b, &lv_label_class) || !lv_label_get_text(b)[0]) continue;
            lv_area_t y; lv_obj_get_coords(b, &y);
            assert(x.x2 < y.x1 || y.x2 < x.x1 || x.y2 < y.y1 || y.y2 < x.y1);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) check_labels(lv_obj_get_child(o, i));
}
static void check(void)
{
    lv_obj_update_layout(s_screen); check_labels(s_screen);
    /* All app panel objects must remain inside its actual content rectangle. */
    lv_area_t a; lv_obj_get_content_coords(s_content, &a);
    for (unsigned i = 0; i < lv_obj_get_child_count(s_content); i++) {
        lv_area_t b; lv_obj_get_coords(lv_obj_get_child(s_content, i), &b);
        if (!(b.x1 >= a.x1 && b.y1 >= a.y1 && b.x2 <= a.x2 && b.y2 <= a.y2)) fprintf(stderr, "Child bounds: %d,%d-%d,%d content %d,%d-%d,%d text=%s\n", b.x1,b.y1,b.x2,b.y2,a.x1,a.y1,a.x2,a.y2,lv_obj_check_type(lv_obj_get_child(s_content,i),&lv_label_class)?lv_label_get_text(lv_obj_get_child(s_content,i)):"box");
        assert(b.x1 >= a.x1 && b.y1 >= a.y1 && b.x2 <= a.x2 && b.y2 <= a.y2);
    }
}
static void snap(const char *name)
{
    check();
    lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[120]; snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%u %u\n255\n", b->header.w, b->header.h);
    for (unsigned y = 0; y < b->header.h; y++) for (unsigned x = 0; x < b->header.w; x++) {
        uint8_t *p = b->data + y * b->header.stride + x * 3;
        uint8_t rgb[] = {p[2], p[1], p[0]}; fwrite(rgb, 1, 3, f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}

static void resolve(void) {
    unsigned n=0;
    while(s_state.busy) { advance(240); assert(++n<22); check(); }
    advance(300);
}
int main(void)
{
    lv_init(); assert(lv_display_create(240,320));
    fruit_merge_enter(true); snap("home");
    key(BSP_BTN_OK); assert(s_state.page==FM_RULES); snap("rules");
    key(BSP_BTN_OK); assert(s_state.page==FM_PLAY); snap("first-drop");
    key(BSP_BTN_OK); assert(s_state.board.turns==1); resolve();
    key(BSP_BTN_OK); assert(s_state.busy); advance(240); snap("first-merge"); resolve();
    assert(s_state.board.score==4 && s_state.board.cells[0][0]==2);
    /* Long hold must pause without first placing a fruit. */
    unsigned turns=s_state.board.turns;
    key_event(BSP_BTN_OK,BSP_BTN_PRESS); advance(900); key_event(BSP_BTN_OK,BSP_BTN_LONG);
    assert(s_state.page==FM_PAUSE && s_state.board.turns==turns); snap("pause");
    key(BSP_BTN_UP); assert(s_state.page==FM_PLAY && s_state.undo_used && s_state.board.turns==1); snap("undo");
    resolve(); advance(300); key_event(BSP_BTN_OK,BSP_BTN_DOUBLE); resolve();
    assert(s_state.board.turns==2);
    key_event(BSP_BTN_OK,BSP_BTN_LONG); advance(300); key(BSP_BTN_DOWN); assert(s_state.page==FM_HOME);
    key(BSP_BTN_DOWN); assert(s_state.page==FM_BOOK); snap("fruit-book");
    key(BSP_BTN_DOWN); snap("fruit-book-large"); key(BSP_BTN_OK); key(BSP_BTN_OK);
    assert(s_state.page==FM_PLAY);
    /* Every rank, both inactive and highlighted, including a densely packed board. */
    for(unsigned rank=1;rank<=8;rank++) {
        s_state.board.cells[0][0]=rank; s_state.board.peak=rank; s_state.busy=true;
        s_state.focus_row=0; s_state.focus_col=0; render(); check();
    }
    fm_start(&s_state,5);
    for(unsigned r=0;r<FM_ROWS;r++) for(unsigned c=0;c<FM_COLS;c++)
        s_state.board.cells[r][c]=(r+c)%8+1;
    s_state.board.cells[4][2]=0; s_state.board.selected=2; s_state.board.score=999999;
    s_state.board.current=3; s_state.board.next=2; s_state.board.peak=8;
    s_state.chain=19; render(); snap("busy-board");
    s_state.board.selected=0; render(); snap("full-column");
    s_state.has_previous=true; s_state.previous=s_state.board; s_state.previous.cells[4][0]=0;
    s_state.page=FM_RESULT; s_state.max_chain=19; render(); snap("result");
    advance(300); key_event(BSP_BTN_OK,BSP_BTN_LONG); assert(s_state.page==FM_PLAY && s_state.undo_used); check();
    s_state.page=FM_RESULT; render(); snap("result-used-undo");
    key(BSP_BTN_OK); assert(s_state.page==FM_PLAY && s_state.seed==5);
    key_event(BSP_BTN_OK,BSP_BTN_PRESS); advance(1200); key_event(BSP_BTN_OK,BSP_BTN_LONG);
    assert(s_state.page==FM_PAUSE && !s_state.board.turns);
    key(BSP_BTN_OK); advance(60000); assert(s_dimmed && backlight==20 && s_state.page==FM_PAUSE);
    key(BSP_BTN_OK); assert(!s_dimmed && backlight==100 && s_state.page==FM_PAUSE);
    advance(900); key(BSP_BTN_OK); assert(s_state.page==FM_PLAY);
    advance(180000); assert(s_off && backlight==0);
    key(BSP_BTN_OK); assert(backlight==100 && !s_off && s_state.page==FM_PAUSE);
    soc=-1; battery(NULL); check(); soc=101; battery(NULL); check();
    advance(1000); key(BSP_BTN_OK); assert(s_state.page==FM_PLAY);
    fruit_merge_key(BSP_BTN_OK,BSP_BTN_CLICK); advance(400); assert(!s_state.board.turns);
    for(unsigned i=0;i<8;i++) fruit_merge_key(BSP_BTN_OK,BSP_BTN_CLICK);
    advance(20); assert(s_state.board.turns==1); resolve();
    fm_start(&s_state, 20260905);
    unsigned chooser=17;
    while(s_state.page==FM_PLAY && s_state.board.turns<500) {
        chooser=chooser*1664525u+1013904223u;
        unsigned col=(chooser>>16)%FM_COLS;
        while(fm_landing(&s_state,col)<0) col=(col+1)%FM_COLS;
        s_state.board.selected=col; s_activity=now_ms();
        assert(fm_drop(&s_state)); s_next_step=now_ms()+220; render(); resolve();
        if(s_state.board.turns==20) snap("playing");
        if(s_state.chain>=3 && s_state.page==FM_PLAY) snap("chain");
    }
    assert(s_state.page==FM_RESULT); snap("round-result");
    advance(180000); assert(backlight==0);
    fruit_merge_exit(); fruit_merge_enter(true); assert(backlight==100 && !s_state.has_game);
    for(unsigned i=0;i<50;i++) {
        fruit_merge_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        fruit_merge_key(BSP_BTN_OK,BSP_BTN_CLICK); fruit_merge_enter(true); check();
    }
    fruit_merge_exit(); fruit_merge_enter(false); key(BSP_BTN_OK);
    assert(s_state.page==FM_HOME); snap("no-buttons");
    puts("Fruit Merge UI: all pages, Chinese glyphs, bounds, overlap, inputs, wake and lifecycle PASS");
}
