#include "pvz_view.h"
#include "pvz_art.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
static lv_obj_t *screen;
static unsigned clips;
static const char *output_dir;
static void check(lv_obj_t *o) {
    if(lv_obj_check_type(o,&lv_label_class)) {
        const unsigned char *p=(const unsigned char *)lv_label_get_text(o);
        const lv_font_t *font=lv_obj_get_style_text_font(o,0);
        while(*p) {
            unsigned cp=*p++;
            if(cp>=0xC0) {
                unsigned n=cp<0xE0?1:(cp<0xF0?2:3);
                cp&=(1U<<(6-n))-1;
                for(unsigned i=0;i<n;i++) cp=(cp<<6)|(*p++ & 63);
            }
            if(cp=='\n') continue;
            lv_font_glyph_dsc_t glyph;
            if(!lv_font_get_glyph_dsc(font,&glyph,cp,0) || glyph.is_placeholder) {
                fprintf(stderr,"Missing glyph U+%04X in %s\n",cp,lv_label_get_text(o));clips++;
            }
        }
        lv_area_t a;lv_obj_get_coords(o,&a);
        lv_area_t parent;
        lv_obj_get_content_coords(lv_obj_get_parent(o),&parent);
        if(a.x1<parent.x1 || a.x2>parent.x2 || a.y1<parent.y1 || a.y2>parent.y2) {
            fprintf(stderr,"PARENT CLIP: %s (%d,%d,%d,%d) parent (%d,%d,%d,%d)\n",
                lv_label_get_text(o),a.x1,a.y1,a.x2,a.y2,parent.x1,parent.y1,parent.x2,parent.y2);clips++;
        }
        if(a.x1<0 || a.x2>=240 || a.y1<0 || a.y2>=320) {
            fprintf(stderr,"CLIP: %s (%d,%d,%d,%d)\n",lv_label_get_text(o),a.x1,a.y1,a.x2,a.y2);clips++;
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++) check(lv_obj_get_child(o,i));
}
static void snap(const char *name) {
    lv_obj_update_layout(screen);check(screen);
    lv_draw_buf_t *b=lv_snapshot_take(screen,LV_COLOR_FORMAT_RGB888);assert(b);
    char path[256];snprintf(path,sizeof(path),"%s/%s.ppm",output_dir,name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++)for(unsigned x=0;x<b->header.w;x++) {
        uint8_t *p=b->data+y*b->header.stride+x*3;uint8_t rgb[3]={p[2],p[1],p[0]};fwrite(rgb,1,3,f);
    }
    fclose(f);lv_draw_buf_destroy(b);
}
int main(int argc, char **argv) {
    assert(argc==2);output_dir=argv[1];
    lv_init();assert(lv_display_create(240,320));
    pvz_state_t s;pvz_init(&s);screen=pvz_view_create();lv_screen_load(screen);
    pvz_view_battery(87);pvz_view_render(&s,true);snap("home");
    pvz_open(&s,0,2026);
    for(unsigned i=0;i<PVZ_COUNT;i++) {
        s.index=i;pvz_view_render(&s,true);pvz_view_status(i==0?1:0);
        char n[24];snprintf(n,sizeof(n),"card-%02u",i);snap(n);
    }
    pvz_open(&s,2,2026);pvz_view_render(&s,true);pvz_view_status(1);snap("quiz");
    pvz_confirm(&s);pvz_view_render(&s,true);snap("reveal");
    s.page=PVZ_RESULT;s.score=5;pvz_view_render(&s,true);snap("result");
    pvz_open(&s,1,0);pvz_view_render(&s,true);pvz_view_battery(-1);pvz_view_status(-1);snap("no-audio");
    pvz_home(&s);pvz_view_render(&s,false);snap("no-buttons");
    for(unsigned i=0;i<100;i++) {pvz_view_destroy();screen=pvz_view_create();lv_screen_load(screen);pvz_view_render(&s,true);}
    pvz_view_destroy();assert(clips==0);puts("24 cards, quiz, result, missing peripherals, 100 screen lifecycles PASS");
}
