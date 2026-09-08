#include "cat/cat_ui.h"
#include "cat/cat_control.h"
#include "src/misc/lv_text_private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static cat_control_t control;
static cat_ui_t ui;
static unsigned checked;
static bool visible(lv_obj_t *o) { for(;o;o=lv_obj_get_parent(o)) if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)) return false; return true; }
static void labels(lv_obj_t *o) {
    if(!visible(o)) return;
    if(lv_obj_check_type(o,&lv_label_class) && lv_label_get_text(o)[0]) {
        const char *s=lv_label_get_text(o); lv_area_t a; lv_obj_get_coords(o,&a);
        if(a.x1<0 || a.y1<0 || a.x2>=240 || a.y2>=320) fprintf(stderr,"Offscreen %s [%d,%d,%d,%d]\n",s,a.x1,a.y1,a.x2,a.y2);
        assert(a.x1>=0 && a.y1>=0 && a.x2<240 && a.y2<320);
        lv_point_t dim; const lv_font_t *font=lv_obj_get_style_text_font(o,0);
        lv_text_get_size(&dim,s,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        int width=lv_obj_get_width(o)-lv_obj_get_style_pad_left(o,0)-lv_obj_get_style_pad_right(o,0);
        int height=lv_obj_get_height(o)-lv_obj_get_style_pad_top(o,0)-lv_obj_get_style_pad_bottom(o,0);
        if(dim.x>width || dim.y>height) fprintf(stderr,"Clipped %s: text %dx%d, room %dx%d\n",s,dim.x,dim.y,width,height);
        assert(dim.x<=width && dim.y<=height);
        for(uint32_t i=0;s[i];) { uint32_t ch=lv_text_encoded_next(s,&i); lv_font_glyph_dsc_t d;
            assert(!(ch>='a' && ch<='z') && !(ch>='A' && ch<='Z'));
            assert(lv_font_get_glyph_dsc(font,&d,ch,0) && !d.is_placeholder);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++) {
        lv_obj_t *a=lv_obj_get_child(o,i);
        if(!visible(a) || !lv_obj_check_type(a,&lv_label_class) || !lv_label_get_text(a)[0]) continue;
        lv_area_t x; lv_obj_get_coords(a,&x);
        for(unsigned j=i+1;j<lv_obj_get_child_count(o);j++) {
            lv_obj_t *b=lv_obj_get_child(o,j);
            if(!visible(b) || !lv_obj_check_type(b,&lv_label_class) || !lv_label_get_text(b)[0]) continue;
            lv_area_t y; lv_obj_get_coords(b,&y);
            bool apart=x.x2<y.x1 || y.x2<x.x1 || x.y2<y.y1 || y.y2<x.y1;
            if(!apart) fprintf(stderr,"Overlap %s / %s\n",lv_label_get_text(a),lv_label_get_text(b));
            assert(apart);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++) labels(lv_obj_get_child(o,i));
}
static void check_cat_visible(void) {
    lv_draw_buf_t *b=lv_snapshot_take(ui.screen,LV_COLOR_FORMAT_RGB888); assert(b);
    const uint32_t colors[]={0xE8B77E,0xEAE5DB,0xB1B6C2}; uint32_t fur=colors[control.cat.profile.coat];
    unsigned pixels=0;
    for(unsigned y=80;y<238;y++) for(unsigned x=15;x<220;x++) {
        uint8_t *p=b->data+y*b->header.stride+x*3;
        pixels+=p[2]==(fur>>16) && p[1]==((fur>>8)&255) && p[0]==(fur&255);
    }
    if(pixels<1500) fprintf(stderr,"Cat hidden/occluded: %u visible fur pixels\n",pixels);
    assert(pixels>=1500); lv_draw_buf_destroy(b);
}
static void render(unsigned time,unsigned page,unsigned row,int battery,bool audio,bool storage,bool buttons) {
    cat_ui_render(&ui,time,page,row,battery,audio,storage,buttons); lv_obj_update_layout(ui.screen); labels(ui.screen); if(page==0) check_cat_visible(); checked++;
}
static void snapshot(const char *name) {
    lv_draw_buf_t *b=lv_snapshot_take(ui.screen,LV_COLOR_FORMAT_RGB888); assert(b);
    char path[100]; snprintf(path,sizeof path,"%s.ppm",name); FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++) for(unsigned x=0;x<b->header.w;x++) {
        uint8_t *p=b->data+y*b->header.stride+x*3; uint8_t rgb[]={p[2],p[1],p[0]}; fwrite(rgb,1,3,f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
int main(int argc,char **argv) {
    bool capture=argc>1 && !strcmp(argv[1],"--capture");
    lv_init(); assert(lv_display_create(240,320)); cat_control_init(&control,714,NULL); cat_ui_create(&ui,&control.cat);
    for(unsigned coat=0;coat<3;coat++) for(unsigned pose=0;pose<CAT_POSES;pose++) {
        control.cat.profile.coat=coat; control.cat.pose=(cat_pose_t)pose;
        for(unsigned variant=0;variant<3;variant++) for(unsigned frame=0;frame<5;frame++) {
            control.cat.variant=variant; control.cat.pose_ms=frame*420;
            render(frame*1200,0,0,100,true,true,true);
        }
        if(capture) { char name[50]; snprintf(name,sizeof name,"coat-%u-pose-%02u",coat,pose); snapshot(name); }
    }
    control.cat.pose=CAT_DANCING;
    for(unsigned f=0;f<400;f++) {
        control.cat.pose_ms=f*80; render(f*80,0,0,87,true,true,true);
        if(capture && f<100) { char name[50]; snprintf(name,sizeof name,"dance-%03u",f); snapshot(name); }
    }
    for(unsigned name=0;name<4;name++) for(unsigned toy=0;toy<3;toy++) for(unsigned row=0;row<6;row++) for(unsigned pocket=0;pocket<2;pocket++) {
        control.cat.profile.name=name; control.cat.profile.toy=toy; control.cat.profile.pocket=pocket;
        render(0,1,row,-1,false,false,false);
    }
    for(unsigned mask=0;mask<64;mask++) { control.cat.profile.memories=mask; render(0,2,0,0,true,true,true); }
    if(capture) {
        control.cat.profile.coat=0; control.cat.profile.name=0; control.cat.profile.toy=1; control.cat.profile.sound=1; control.cat.profile.pocket=0;
        render(0,1,0,87,true,true,true); snapshot("nest"); render(0,2,0,87,true,true,true); snapshot("memories");
        control.cat.pose=CAT_PAT; render(600,0,0,-1,false,false,false); snapshot("degraded");
        cat_control_init(&control,714,NULL);
        for(unsigned f=0;f<220;f++) {
            if(f==20 || f==100) { cat_control_key(&control,2,0); cat_control_key(&control,2,1); }
            if(f==44 || f==50 || f==56) cat_control_key(&control,0,0);
            if(f==145) cat_control_key(&control,1,0);
            cat_control_tick(&control,120); render(f*120,0,0,87,true,true,true);
            char name[50]; snprintf(name,sizeof name,"motion-%03u",f); snapshot(name);
        }
    }
    cat_ui_destroy(&ui);
    for(unsigned n=0;n<10;n++) { cat_ui_create(&ui,&control.cat); render(0,0,0,50,true,true,true); cat_ui_destroy(&ui); }
    printf("Cat UI: %u views checked; all text Chinese, supported, contained, separated; lifecycle PASS\n",checked);
    lv_deinit(); return 0;
}
