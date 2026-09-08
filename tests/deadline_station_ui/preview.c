/* Render the production app with real LVGL and simulated board peripherals. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "apps/deadline_station/deadline_station.c"
static int64_t fake_us;
static int soc = 87, backlight;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }

int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t b) { assert(!callback_active); backlight = b; }
static ds_progress_t saved_progress = {.seed={2026,9,5,8,0}};
static unsigned storage_status;
ds_progress_t ds_storage_init(void) { return saved_progress; }
void ds_storage_save(ds_progress_t p) { assert(!callback_active); saved_progress=p; }
unsigned ds_storage_status(void) { return storage_status; }
static void advance(unsigned ms) { fake_us += (int64_t)ms * 1000; lv_tick_inc(ms); tick(s_timer); }
static void key_event(bsp_btn_t b, bsp_btn_ev_t e)
{
    callback_active = true; deadline_station_key(b, e); callback_active = false; advance(20);
}
static void key(bsp_btn_t b) { advance(250); key_event(b, BSP_BTN_CLICK); }
static void check_labels(lv_obj_t *o)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *text = lv_label_get_text(o);
        lv_area_t a; lv_obj_get_coords(o, &a);
        if (!(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320)) fprintf(stderr,"Out of screen: %s [%d,%d,%d,%d]\n",text,a.x1,a.y1,a.x2,a.y2);
        assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
        lv_point_t size;
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o)) fprintf(stderr, "Overflow: %s width=%d allowed=%d\n", text, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        for (uint32_t i = 0; text[i];) {
            uint32_t cp = lv_text_encoded_next(text, &i);
            if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) {
                bool conference_name = false;
                for (unsigned c = 0; c < ds_catalog_count; c++)
                    if (!strcmp(text, ds_catalog[c].name)) conference_name = true;
                assert(conference_name);
            }
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
            if (!(x.x2 < y.x1 || y.x2 < x.x1 || x.y2 < y.y1 || y.y2 < x.y1)) fprintf(stderr,"Overlap: %s [%d,%d,%d,%d] / %s [%d,%d,%d,%d]\n",lv_label_get_text(a),x.x1,x.y1,x.x2,x.y2,lv_label_get_text(b),y.x1,y.y1,y.x2,y.y2);
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
    lv_init(); assert(lv_display_create(240,320));deadline_station_prepare();
    deadline_station_enter(true);assert(s_page==CLOCK && !s_clock_valid);snap("clock");
    for(unsigned i=0;i<6;i++)key(BSP_BTN_OK);
    assert(s_clock_valid && s_page==HOME);snap("home");
    unsigned first=s_order[0];key(BSP_BTN_OK);assert(s_page==DETAIL);snap("detail");
    key(BSP_BTN_OK);assert(s_progress.flags[first]&1);
    advance(250);key_event(BSP_BTN_OK,BSP_BTN_LONG);assert(s_page==HOME);
    key_event(BSP_BTN_OK,BSP_BTN_PRESS);assert(s_page==HOME);
    advance(250);key_event(BSP_BTN_OK,BSP_BTN_LONG);assert(s_page==MENU);snap("menu");
    for(unsigned year=2027;year<=2029;year++)for(unsigned field=0;field<6;field++) {
        s_year=year;s_field=field;refresh_list();s_page=HOME;
        for(unsigned i=0;i<(s_count?s_count:1);i++) {
            s_cursor=i;render();check();
            const ds_entry_t *e=entry();
            if(!e)continue;
            if(e->count) {
                for(unsigned j=0;j<e->count;j++) {
                    s_page=DETAIL;s_node=j;render();check();
                    s_anchor_utc=e->nodes[j].utc-1;s_anchor_ms=now_ms();s_page=HOME;render();check();
                    advance(1000);check();
                }
            } else {snap("pending");s_page=DETAIL;render();check();}
            s_page=CHECKLIST;
            for(unsigned j=0;j<4;j++){s_check=j;render();check();key(BSP_BTN_OK);check();}
            snap("checklist");s_page=HOME;
        }
    }
    s_anchor_utc=ds_checked_utc;s_anchor_ms=now_ms();s_year=2027;s_field=1;refresh_list();s_page=HOME;render();snap("vision");
    s_cursor=0;s_page=DETAIL;render();snap("vision-detail");
    for(unsigned i=0;i<6;i++){s_page=MENU;s_menu=i;render();check();}
    for(unsigned i=0;i<6;i++){s_page=FILTER;s_choice=i;render();check();}
    for(unsigned i=0;i<3;i++){s_page=YEAR;s_choice=i;render();check();}
    s_choice=2;s_page=YEAR;render();snap("year");
    s_page=ABOUT;render();snap("about");
    begin_clock();
    for(unsigned i=0;i<6;i++){s_edit_field=i;render();check();}
    s_edit=(ds_date_t){2099,12,31,23,59};render();check();
    s_page=FOCUS;s_focus_left=900;render();snap("focus");key(BSP_BTN_OK);
    assert(s_focus_running);advance(10000);key(BSP_BTN_OK);assert(!s_focus_running);
    int64_t left=s_focus_left;advance(5000);assert(s_focus_left==left);
    key(BSP_BTN_OK);advance((unsigned)s_focus_left*1000);assert(s_focus_done && s_progress.sessions>0);snap("harvest");
    s_page=HOME;render();advance(121000);assert(backlight==25);
    unsigned before=s_cursor;key(BSP_BTN_DOWN);assert(backlight==100 && s_cursor==before);
    soc=-1;battery(NULL);check();soc=101;battery(NULL);check();
    storage_status=2;s_page=CHECKLIST;render();advance(100);snap("storage-failure");
    for(unsigned i=0;i<30;i++){deadline_station_exit();deadline_station_key(BSP_BTN_OK,BSP_BTN_CLICK);deadline_station_enter(true);check();}
    deadline_station_exit();s_clock_valid=false;deadline_station_enter(false);key(BSP_BTN_OK);assert(s_page==CLOCK && !s_clock_valid);snap("no-buttons");
    deadline_station_exit();assert(!s_screen && !s_timer);
    puts("Deadline Station UI: every record/page, glyphs, bounds, key queue, pause, wake, storage failure, 30 exits PASS");
}
