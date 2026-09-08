#include "tally_ui.h"
#include "../../main/tally/tally_ui.c"
#include "src/misc/lv_text_private.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tc_state s;
static unsigned checks;
static void inspect(lv_obj_t *o)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    lv_area_t geometry; lv_obj_get_coords(o,&geometry);
    assert(geometry.x1>=0 && geometry.y1>=0 && geometry.x2<240 && geometry.y2<320);
    if (lv_obj_check_type(o,&lv_label_class)) {
        const char *text=lv_label_get_text(o);
        if (text[0]) {
            lv_area_t a; lv_obj_get_coords(o,&a);
            const lv_font_t *font=lv_obj_get_style_text_font(o,0);
            lv_point_t size; lv_text_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            if (size.x>lv_obj_get_width(o) || a.x1<0 || a.y1<0 || a.x2>=240 || a.y2>=320)
                fprintf(stderr,"Text bounds: %s, %d required / %d available, [%d,%d,%d,%d]\n",text,(int)size.x,(int)lv_obj_get_width(o),a.x1,a.y1,a.x2,a.y2);
            assert(size.x<=lv_obj_get_width(o));
            assert(a.x1>=0 && a.y1>=0 && a.x2<240 && a.y2<320);
            lv_obj_t *parent=lv_obj_get_parent(o);
            if (parent) {
                lv_area_t pa; lv_obj_get_content_coords(parent,&pa);
                assert(a.x1>=pa.x1 && a.x2<=pa.x2 && a.y1>=pa.y1 && a.y2<=pa.y2);
            }
            for (uint32_t i=0;text[i];) {
                uint32_t cp=lv_text_encoded_next(text,&i);
                assert(!(cp>='A' && cp<='Z') && !(cp>='a' && cp<='z'));
                lv_font_glyph_dsc_t d;
                assert(lv_font_get_glyph_dsc(font,&d,cp,0) && !d.is_placeholder);
            }
        }
    }
    for (unsigned i=0;i<lv_obj_get_child_count(o);i++) {
        lv_obj_t *a=lv_obj_get_child(o,i);
        if (lv_obj_check_type(a,&lv_label_class) && lv_label_get_text(a)[0]) {
            lv_area_t x; lv_obj_get_coords(a,&x);
            for (unsigned j=i+1;j<lv_obj_get_child_count(o);j++) {
                lv_obj_t *b=lv_obj_get_child(o,j);
                if (!lv_obj_check_type(b,&lv_label_class) || !lv_label_get_text(b)[0]) continue;
                lv_area_t y; lv_obj_get_coords(b,&y);
                if (!(x.x2<y.x1 || y.x2<x.x1 || x.y2<y.y1 || y.y2<x.y1))
                    fprintf(stderr,"Overlap: %s / %s\n",lv_label_get_text(a),lv_label_get_text(b));
                assert(x.x2<y.x1 || y.x2<x.x1 || x.y2<y.y1 || y.y2<x.y1);
            }
        }
        inspect(a);
    }
}
static void render(tc_notice n,int battery)
{
    tc_ui_render(&s,n,battery,1000); lv_obj_update_layout(tc_ui_screen());
    inspect(tc_ui_screen()); ++checks;
}
static void snap_current(const char *name)
{
    lv_draw_buf_t *b=lv_snapshot_take(tc_ui_screen(),LV_COLOR_FORMAT_RGB888); assert(b);
    char path[120]; snprintf(path,sizeof(path),"%s.ppm",name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for (unsigned y=0;y<b->header.h;y++) for (unsigned x=0;x<b->header.w;x++) {
        uint8_t *p=b->data+y*b->header.stride+x*3;
        uint8_t rgb[]={p[2],p[1],p[0]}; fwrite(rgb,1,3,f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
static void snap(const char *name) { render(TC_STORED,87); snap_current(name); }
int main(void)
{
    lv_init(); assert(lv_display_create(240,320));
    tc_init(&s,NULL); tc_ui_create(); snap("boot");
    s.page=TC_COUNT; s.data.count=128; snap("count");
    s.page=TC_PAUSE; snap("pause");
    s.page=TC_HISTORY_PAGE; snap("history-empty");
    for (int i=0;i<15;i++) {
        s.data.count=(unsigned)(45+i*11); tc_data d; tc_candidate(&s,&d); s.data=d;
    }
    s.page=TC_HISTORY_PAGE; snap("history");
    for (unsigned i=0;i<10;i++) { s.selected=i; render(TC_STORED,-1); }
    for (int page=0;page<=TC_HISTORY_PAGE;page++) {
        s.page=(tc_page)page;
        for (int n=0;n<=TC_ARCHIVED;n++) render((tc_notice)n,0);
    }
    s.page=TC_COUNT;
    unsigned values[]={0,1,9,10,99,100,888,999,1000,8888,9999};
    for (unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) { s.data.count=values[i]; render(TC_PENDING,101); }
    snap("maximum");
    for (unsigned i=0;i<500;i++) { s.data.count=i; render(TC_STORED,87); }
    for(int e=TC_FX_ADD;e<=TC_FX_ERROR;e++) {
        s.page=e==TC_FX_PAUSE ? TC_PAUSE:TC_COUNT;
        s.data.count=e==TC_FX_ARCHIVE ? 0:e==TC_FX_ADD ? 129:128;
        render(TC_STORED,87); tc_ui_feedback((tc_feedback)e);
        for(unsigned frame=0;frame<17;frame++) {
            lv_tick_inc(40); animate(NULL); lv_obj_update_layout(tc_ui_screen()); inspect(tc_ui_screen()); ++checks;
            if(e<=TC_FX_ARCHIVE) {
                char name[64]; snprintf(name,sizeof(name),"effect-%d-%02u",e,frame); snap_current(name);
            }
        }
        assert(fx==TC_FX_NONE);
    }
    tc_ui_feedback(TC_FX_ARCHIVE); tc_ui_destroy(); assert(!fx_timer);
    for (unsigned i=0;i<20;i++) { tc_ui_create(); render(TC_STORED,-1); tc_ui_destroy(); }
    printf("Tally UI: %u renders, all pages/notices, 0..9999, Chinese glyphs, overlap and clipping checks PASS\n",checks);
}
