/* Render the production app with real LVGL and simulated board peripherals. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "apps/focus_post/focus_post.c"
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
    callback_active = true; focus_post_key(b, e); callback_active = false; advance(20);
}
static void key(bsp_btn_t b) { key_event(b, BSP_BTN_PRESS); }
static void check_labels(lv_obj_t *o)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *text = lv_label_get_text(o);
        lv_area_t a; lv_obj_get_coords(o, &a);
        assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
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
int main(void)
{
    lv_init(); assert(lv_display_create(240,320));
    focus_post_enter(true); snap("home");
    key_event(BSP_BTN_OK,BSP_BTN_CLICK); key_event(BSP_BTN_OK,BSP_BTN_DOUBLE);
    assert(s_state.page==FP_HOME);
    for(unsigned pace=0;pace<3;pace++) {
        s_state.pace=pace; render(); check();
        advance(300); key(BSP_BTN_OK); assert(s_state.page==FP_RULE); snap("rule");
        advance(300); key(BSP_BTN_OK); assert(s_state.page==FP_READY); snap("ready");
        advance(900); assert(s_state.page==FP_PRACTICE); snap("practice-send");
        advance(5000); assert(s_state.page==FP_PRACTICE);
        key(BSP_BTN_OK); assert(s_state.page==FP_FEEDBACK && s_state.correct); snap("delivered");
        key(BSP_BTN_OK); assert(s_state.page==FP_FEEDBACK);
        advance(300); key(BSP_BTN_OK); advance(900); snap("practice-wait");
        key(BSP_BTN_OK); assert(!s_state.correct); snap("try-again");
        advance(300); key(BSP_BTN_OK); advance(900); advance(fp_duration(&s_state));
        assert(s_state.correct); snap("waited");
        advance(300); key(BSP_BTN_OK); assert(s_state.page==FP_RULE && !s_state.tutorial);
        advance(300); key(BSP_BTN_OK); advance(900);
        for(unsigned r=0;r<12;r++) {
            assert(s_state.page==FP_VISITOR); snap("visitor");
            advance(300); key(BSP_BTN_DOWN); assert(s_state.page==FP_PAUSE); snap("pause");
            int64_t left=s_state.remaining;
            advance(500); key(BSP_BTN_OK); assert(s_state.page==FP_READY);
            advance(900); assert(s_state.page==FP_VISITOR);
            assert(s_state.deadline-now_ms()==left);
            advance(300);
            bool match=fp_current(&s_state)==s_state.target;
            if (r==3 ? !match : match) key(BSP_BTN_OK);
            else advance(fp_duration(&s_state));
            assert(s_state.page==FP_FEEDBACK); check();
            advance(300); key(BSP_BTN_OK); advance(900); check();
        }
        assert(s_state.page==FP_RESULT && s_state.delivered+s_state.waited==11); snap("result");
        uint8_t old[12]; memcpy(old,s_state.cards,12);
        advance(300); key(BSP_BTN_UP); assert(!memcmp(old,s_state.cards,12));
        advance(300); key_event(BSP_BTN_OK,BSP_BTN_LONG); assert(s_state.page==FP_HOME);
    }
    for(unsigned a=0;a<3;a++) {
        fp_start(&s_state,a,false); s_state.target=a; render(); check();
        fp_ok(&s_state,now_ms()); fp_tick(&s_state,now_ms()+850);
        s_state.cards[0]=a; render(); check();
        s_state.delivered=s_state.waited=6; s_state.page=FP_RESULT; render(); snap("perfect");
    }
    fp_home(&s_state); render(); s_state.opened=0;
    advance(300); key(BSP_BTN_OK); advance(30000); assert(s_state.page==FP_PAUSE);
    advance(60000); assert(backlight==20); key(BSP_BTN_OK); assert(backlight==100 && s_state.page==FP_PAUSE);
    advance(180000); assert(backlight==0); key(BSP_BTN_OK); assert(backlight==100 && s_state.page==FP_PAUSE);
    advance(1000); key_event(BSP_BTN_OK,BSP_BTN_LONG); assert(s_state.page==FP_HOME);
    soc=-1; battery(NULL); check(); soc=101; battery(NULL); check();
    advance(300); focus_post_key(BSP_BTN_OK,BSP_BTN_PRESS); advance(251); assert(s_state.page==FP_HOME);
    for(int i=0;i<8;i++) focus_post_key(BSP_BTN_OK,BSP_BTN_PRESS);
    advance(20); assert(s_state.page==FP_RULE);
    /* Last 50 ms survive pause, the release cue, and the UI input guard. */
    fp_start(&s_state,42,false); s_state.cards[0]=s_state.target;
    fp_ok(&s_state,now_ms()); advance(900); render();
    advance(fp_duration(&s_state)-50); key(BSP_BTN_DOWN); assert(s_state.page==FP_PAUSE);
    advance(300); key(BSP_BTN_OK); assert(s_state.page==FP_READY);
    advance(900); key(BSP_BTN_OK); assert(s_state.delivered==1);
    advance(300);
    /* A queued physical press before expiry still counts when processed late. */
    fp_start(&s_state,42,false); s_state.cards[0]=s_state.target;
    fp_ok(&s_state,now_ms()); advance(900); render();
    advance(fp_duration(&s_state)-10);
    focus_post_key(BSP_BTN_OK,BSP_BTN_PRESS); advance(20);
    assert(s_state.page==FP_FEEDBACK && s_state.delivered==1);
    advance(300); fp_start(&s_state,42,false); s_state.cards[0]=s_state.target;
    fp_ok(&s_state,now_ms()); advance(900); render();
    fake_us+=(int64_t)(fp_duration(&s_state)+10)*1000;
    focus_post_key(BSP_BTN_OK,BSP_BTN_PRESS); advance(20);
    assert(s_state.page==FP_FEEDBACK && s_state.misses==1);
    for(int i=0;i<50;i++) {
        focus_post_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        focus_post_key(BSP_BTN_OK,BSP_BTN_PRESS); focus_post_enter(true); check();
    }
    focus_post_exit(); focus_post_enter(false); key(BSP_BTN_OK); assert(s_state.page==FP_HOME); snap("no-buttons");
    puts("Focus Post UI: all pages, Chinese glyphs, bounds, input, wake and 50 exits PASS");
}
