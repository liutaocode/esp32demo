#include "cat_ui.h"
#include "ui_pixel.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(cat_zh_12);
LV_FONT_DECLARE(cat_zh_14);
LV_FONT_DECLARE(cat_zh_16);
LV_FONT_DECLARE(cat_zh_20);
#define INK 0x49392F
#define CREAM 0xFFF7E6
#define PEACH 0xE99D89

typedef struct { lv_layer_t *layer; int x,y; } brush_t;
static void box(brush_t b,int x,int y,int w,int h,uint32_t fill,int radius,int border) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(fill); d.radius=radius; d.border_width=border;
    d.border_color=lv_color_hex(INK);
    lv_area_t a={b.x+x,b.y+y,b.x+x+w-1,b.y+y+h-1}; lv_draw_rect(b.layer,&d,&a);
}
static void line(brush_t b,int x,int y,int x2,int y2,uint32_t color,int width) {
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){b.x+x,b.y+y}; d.p2=(lv_point_precise_t){b.x+x2,b.y+y2};
    d.color=lv_color_hex(color); d.width=width; d.round_start=1; d.round_end=1; lv_draw_line(b.layer,&d);
}
static void tri(brush_t b,int x,int y,int x2,int y2,int x3,int y3,uint32_t color) {
    lv_draw_triangle_dsc_t d; lv_draw_triangle_dsc_init(&d); d.color=lv_color_hex(color);
    d.p[0]=(lv_point_precise_t){b.x+x,b.y+y}; d.p[1]=(lv_point_precise_t){b.x+x2,b.y+y2};
    d.p[2]=(lv_point_precise_t){b.x+x3,b.y+y3}; lv_draw_triangle(b.layer,&d);
}
static void heart(brush_t b,int x,int y,int k) {
    box(b,x,y,7*k,7*k,PEACH,LV_RADIUS_CIRCLE,0); box(b,x+5*k,y,7*k,7*k,PEACH,LV_RADIUS_CIRCLE,0);
    tri(b,x,y+4*k,x+12*k,y+4*k,x+6*k,y+12*k,PEACH);
}
static void leaf(brush_t b,int x,int y) {
    box(b,x,y,18,11,0x9DAF6B,7,1); line(b,x+3,y+8,x+17,y+1,0x536B47,1);
}
static void toy(brush_t b,unsigned kind,int x,int y) {
    if(kind==0) {
        box(b,x,y,17,15,0xFFFCEF,5,2); line(b,x+3,y+4,x+9,y+8,0xB8B1A2,1);
        line(b,x+9,y+8,x+6,y+12,0xB8B1A2,1);
    } else if(kind==1) {
        box(b,x,y,20,20,0xC1B6D6,LV_RADIUS_CIRCLE,2);
        line(b,x+4,y+5,x+14,y+14,0x8C7FA1,1); line(b,x+4,y+12,x+14,y+5,0x8C7FA1,1);
        line(b,x+17,y+15,x+28,y+18,0x8C7FA1,2);
    } else {
        line(b,x,y+20,x+16,y,0x8E795F,2); box(b,x+5,y,14,12,0xB2CEB6,7,1);
        line(b,x+9,y+9,x+16,y+2,0x5E8165,1);
    }
}
static void draw_scene(lv_event_t *e) {
    cat_ui_t *u=lv_event_get_user_data(e); const cat_state_t *c=u->cat;
    lv_area_t area; lv_obj_get_coords(u->scene,&area);
    brush_t b={lv_event_get_layer(e),area.x1,area.y1};
    uint32_t coat[]={0xE8B77E,0xEAE5DB,0xB1B6C2}; uint32_t fur=coat[c->profile.coat%3];
    uint32_t patch=c->profile.coat==2 ? 0x858D9F : 0xC9915A;
    float t=u->motion/1000.0f;
    int breathe=(int)roundf(sinf(t*2.0f)); int wag=(int)roundf(sinf(t*2.7f)*6);
    box(b,0,0,208,164,0xF7EACD,8,0);
    for(int x=12;x<208;x+=29) line(b,x,0,x,115,0xEEDDBB,1);
    box(b,132,8,58,65,0xCFB18B,8,2); box(b,137,13,48,52,0xBFE3E0,5,0);
    box(b,160,18,14,14,0xFFF3C0,LV_RADIUS_CIRCLE,0);
    box(b,140,49,42,16,0xA9C1A0,7,0);
    line(b,160,14,160,65,0xCFB18B,3); line(b,137,40,184,40,0xCFB18B,3);
    box(b,128,70,67,5,0xB49875,2,0);
    /* Quiet view out of the window; no clock or sensor claim. */
    if(c->pose==CAT_LOOK) {
        int bx=145+(u->motion/110)%27; line(b,bx,32,bx+4,35,0x537C79,2); line(b,bx+4,35,bx+8,32,0x537C79,2);
    }
    box(b,0,118,208,46,0xD8B58B,0,0); line(b,0,118,207,118,0xBD9972,2);
    line(b,0,145,207,145,0xCDA77D,1);
    box(b,28,125,153,31,0xF0C8AD,LV_RADIUS_CIRCLE,0);
    box(b,38,130,130,19,0xE5B9A0,LV_RADIUS_CIRCLE,0);
    toy(b,c->profile.last_toy,178,139);
    bool dancing=c->pose==CAT_DANCING;
    float dance_t=c->pose_ms/1000.0f;
    int beat=(int)(c->pose_ms/500);
    if(dancing) {
        box(b,0,0,208,164,0xD9CEDF,8,0);
        static const uint32_t lights[]={0xE9B6C9,0xB4DCCF,0xF1D59D,0xBDD5EB};
        tri(b,35,0,0,115,105,115,lights[(beat/4)%4]);
        tri(b,170,0,105,115,207,115,lights[(beat/4+2)%4]);
        for(int y=0;y<3;y++) for(int x=0;x<7;x++)
            box(b,x*30,119+y*15,29,14,lights[(x+y+beat/4)%4],2,0);
        line(b,104,0,104,10,INK,2); box(b,90,8,28,22,0xFFF5DD,11,2);
        line(b,96,13,112,13,0xAD9CAA,1); line(b,93,19,115,19,0xAD9CAA,1);
        line(b,101,10,101,27,0xAD9CAA,1); line(b,108,10,108,27,0xAD9CAA,1);
        for(int side=0;side<2;side++) {
            int nx=side ? 183 : 16, ny=38+(int)(sinf(dance_t*3.0f+side)*8);
            box(b,nx-5,ny+12,10,7,INK,4,0); line(b,nx+3,ny+15,nx+3,ny,INK,2);
            line(b,nx+3,ny,nx+10,ny+4,INK,2);
        }
    }
    int cx=98,hy=64,by=91;
    bool close=c->pose==CAT_SNUGGLE || c->pose==CAT_PAT || c->pose==CAT_ROLL;
    if(c->pose==CAT_APPROACH) { int p=(int)(c->pose_ms<1800 ? c->pose_ms : 1800); cx=78+24*p/1800; hy=62+(int)(sinf(t*15)*2); }
    if(close) { cx=104; hy=70; by=99; }
    if(c->pose==CAT_PLAY) cx=89+(int)(sinf(t*3.2f)*15);
    if(c->pose==CAT_STRETCH) { hy=82; by=105; }
    if(c->pose==CAT_SLEEP) { hy=108; by=105; cx=111; }
    if(dancing) {
        cx=100+(int)(sinf(dance_t*6.283185f)*19);
        hy=62-(int)(fabsf(sinf(dance_t*12.56637f))*7);
        by=94-(int)(fabsf(sinf(dance_t*12.56637f))*7);
        wag=(int)(sinf(dance_t*12.56637f)*9);
    }
    hy+=breathe;
    if(c->pose==CAT_ROLL) {
        box(b,64,86,100,61,fur,29,2); box(b,91,94,47,43,CREAM,22,0);
        box(b,88,78,21,25,fur,10,2); box(b,126,79,21,24,fur,10,2);
        box(b,88,127,21,24,fur,10,2); box(b,132,126,21,24,fur,10,2);
        for(int i=0;i<2;i++) box(b,94+i*39,133,8,9,PEACH,4,0);
        cx=68; hy=92;
    } else {
        /* Tail consists of overlapping outlined capsules, moving at its tip. */
        box(b,cx+24,by+18,39,22,fur,12,2);
        box(b,cx+48,by+6+wag,17,28,fur,9,2);
        box(b,cx-35,by,73,c->pose==CAT_SLEEP ? 42 : 54,fur,26,2);
        box(b,cx-21,by+16,43,32,CREAM,20,0);
        if(c->pose!=CAT_SLEEP) {
            int kick=dancing ? (int)(fabsf(sinf(dance_t*6.283185f))*8) : 0;
            box(b,cx-34,by+38-(beat%2==0 ? kick : 0),32,15,fur,8,2);
            box(b,cx+4,by+38-(beat%2 ? kick : 0),32,15,fur,8,2);
            line(b,cx-23,by+43,cx-23,by+48,patch,1); line(b,cx+23,by+43,cx+23,by+48,patch,1);
        }
    }
    /* Chunky ears, oversized cheeks and tiny face stay readable on this panel. */
    tri(b,cx-44,hy-8,cx-40,hy-39,cx-12,hy-17,INK);
    tri(b,cx+44,hy-8,cx+40,hy-39,cx+12,hy-17,INK);
    tri(b,cx-41,hy-10,cx-38,hy-34,cx-16,hy-16,fur);
    tri(b,cx+41,hy-10,cx+38,hy-34,cx+16,hy-16,fur);
    tri(b,cx-35,hy-11,cx-35,hy-26,cx-21,hy-16,PEACH);
    tri(b,cx+35,hy-11,cx+35,hy-26,cx+21,hy-16,PEACH);
    box(b,cx-47,hy-19,95,66,fur,28,2);
    box(b,cx-29,hy+13,58,31,CREAM,15,0);
    line(b,cx-10,hy-15,cx-7,hy-5,patch,4); line(b,cx+1,hy-16,cx+1,hy-5,patch,4); line(b,cx+12,hy-14,cx+9,hy-5,patch,4);
    bool closed=c->pose==CAT_SLEEP || c->pose==CAT_PAT || c->pose==CAT_ROLL || c->pose==CAT_SNUGGLE || u->motion%5100>4920;
    int eyes=hy+12;
    for(int side=-1;side<=1;side+=2) {
        int ex=cx+side*23;
        if(closed) { line(b,ex-6,eyes+2,ex,eyes+5,INK,3); line(b,ex,eyes+5,ex+6,eyes+2,INK,3); }
        else { box(b,ex-4,eyes-5,9,13,INK,5,0); box(b,ex-2,eyes-3,3,4,0xFFFFFF,2,0); }
        box(b,cx+side*32-6,hy+22,12,6,PEACH,4,0);
    }
    if(dancing) {
        box(b,cx-37,hy+2,32,18,INK,6,0); box(b,cx+5,hy+2,32,18,INK,6,0);
        line(b,cx-5,hy+7,cx+5,hy+7,INK,3);
        line(b,cx-30,hy+6,cx-22,hy+6,0xB8D8D2,2);
        line(b,cx+12,hy+6,cx+20,hy+6,0xB8D8D2,2);
        for(int side=-1;side<=1;side+=2) {
            bool raised=((beat/2)%2==0)==(side<0);
            if((beat/8)%4==3) raised=true;
            int px=cx+side*(raised ? 49 : 45), py=raised ? hy-14 : by+14;
            line(b,cx+side*30,by+12,px,py,INK,18);
            line(b,cx+side*30,by+12,px,py,fur,14);
            box(b,px-10,py-10,21,22,fur,10,2); box(b,px-4,py-1,8,9,PEACH,4,0);
        }
    }
    tri(b,cx-4,hy+23,cx+4,hy+23,cx,hy+27,0xAD776B);
    line(b,cx,hy+27,cx-4,hy+31,INK,2); line(b,cx,hy+27,cx+4,hy+31,INK,2);
    line(b,cx-44,hy+20,cx-33,hy+22,INK,1); line(b,cx+33,hy+22,cx+44,hy+20,INK,1);
    if(c->pose==CAT_GROOM) box(b,cx-34,hy+14+(int)(sinf(t*8)*5),19,23,fur,10,2);
    if(c->pose==CAT_PAT || c->pose==CAT_ROLL) { heart(b,27,12+(int)(sinf(t*3)*3),1); heart(b,96,8,1); }
    if(c->pose==CAT_PLAY) {
        int tx=60+(int)((sinf(t*3.2f)+1)*40);
        toy(b,c->profile.toy,tx,140-(int)(fabsf(sinf(t*4))*12));
        box(b,cx+22,116+(int)(sinf(t*5)*5),31,16,fur,9,2);
    }
    if(c->pose==CAT_GIFT) leaf(b,cx-9,hy+32);
    if(c->pose==CAT_SLEEP) {
        box(b,43,57,14,9,0xFFF8E8,5,0); box(b,48,48,10,7,0xFFF8E8,4,0);
    }
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,int h,const char *s,const lv_font_t *font,uint32_t color) {
    lv_obj_t *o=ui_pixel_label(p,s,font,color); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP); return o;
}
static void hidden(lv_obj_t *o,bool h) { if(h) lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN); }
void cat_ui_create(cat_ui_t *u,const cat_state_t *c) {
    memset(u,0,sizeof *u); u->cat=c; u->previous_screen=lv_screen_active(); u->screen=ui_pixel_screen_create("");
    lv_obj_set_style_bg_color(u->screen,lv_color_hex(0xB8D8D2),0);
    u->title=label(u->screen,15,13,137,25,"小猫在呢",&cat_zh_20,0xFFF8E7);
    u->subtitle=label(u->screen,12,47,151,19,"",&cat_zh_14,INK);
    u->battery=label(u->screen,169,46,65,20,"",&cat_zh_14,INK);
    lv_obj_set_style_text_align(u->battery,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_t *room=ui_pixel_panel_create(u->screen,9,70,218,172,0xFFF7E6);
    lv_obj_set_style_pad_all(room,0,0); lv_obj_set_style_border_width(room,3,0); lv_obj_set_style_radius(room,10,0);
    u->scene=lv_obj_create(room); lv_obj_remove_style_all(u->scene); lv_obj_set_pos(u->scene,2,1); lv_obj_set_size(u->scene,208,164);
    lv_obj_remove_flag(u->scene,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_event_cb(u->scene,draw_scene,LV_EVENT_DRAW_MAIN,u);
    u->message_panel=ui_pixel_panel_create(u->screen,9,250,218,29,CREAM);
    lv_obj_set_style_border_width(u->message_panel,2,0); lv_obj_set_style_pad_all(u->message_panel,0,0);
    u->message=label(u->screen,15,255,206,20,"",&cat_zh_16,INK);
    lv_obj_set_style_text_align(u->message,LV_TEXT_ALIGN_CENTER,0);
    unsigned shadow_index=lv_obj_get_child_count(u->screen);
    u->menu=ui_pixel_panel_create(u->screen,9,70,218,172,CREAM);
    u->menu_shadow=lv_obj_get_child(u->screen,shadow_index);
    lv_obj_set_style_pad_all(u->menu,0,0); lv_obj_set_style_border_width(u->menu,3,0);
    for(unsigned i=0;i<6;i++) {
        u->rows[i]=label(u->menu,5,4+i*27,200,25,"",&cat_zh_16,INK);
        lv_obj_set_style_pad_left(u->rows[i],5,0); lv_obj_set_style_pad_top(u->rows[i],2,0);
    }
    for(unsigned i=0;i<3;i++) {
        u->keys[i]=label(u->screen,9+76*i,284,70,20,"",&cat_zh_14,INK);
        lv_obj_set_style_text_align(u->keys[i],LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_style_bg_color(u->keys[i],lv_color_hex(CREAM),0);
        lv_obj_set_style_bg_opa(u->keys[i],LV_OPA_COVER,0); lv_obj_set_style_radius(u->keys[i],4,0);
    }
    u->hint=label(u->screen,9,305,220,15,"",&cat_zh_12,INK);
    lv_obj_set_style_text_align(u->hint,LV_TEXT_ALIGN_CENTER,0);
    lv_screen_load(u->screen);
}
void cat_ui_render(cat_ui_t *u,uint32_t motion,unsigned page,unsigned selection,int battery,bool audio,bool storage,bool buttons) {
    const cat_state_t *c=u->cat; u->motion=motion; u->page=page; u->selection=selection;
    char text[96];
    snprintf(text,sizeof text,"%s的%s",cat_name(c),page ? "小窝" : c->pose==CAT_DANCING ? "舞台" : "小房间"); lv_label_set_text(u->subtitle,text);
    if(battery>=0 && battery<=100) snprintf(text,sizeof text,"电量%d%%",battery); else snprintf(text,sizeof text,"电量未知");
    lv_label_set_text(u->battery,text);
    hidden(u->menu,page==0); hidden(u->menu_shadow,page==0); hidden(lv_obj_get_parent(u->scene),page!=0);
    if(page==0) {
        lv_label_set_text(u->message,cat_message(c));
        lv_label_set_text(u->keys[0],"上 摸摸"); lv_label_set_text(u->keys[1],"确认 叫它"); lv_label_set_text(u->keys[2],"下 玩玩");
        lv_label_set_text(u->hint,!buttons ? "按键暂不可用，小猫陪你待着" : !storage ? "暂不能保存，长按确认进小窝" : "双击跳舞 · 长按进小窝");
        if(c->pose==CAT_DANCING) {
            lv_label_set_text(u->keys[0],"上 停舞"); lv_label_set_text(u->keys[1],"确认 停舞"); lv_label_set_text(u->keys[2],"下 停舞");
            lv_label_set_text(u->hint,"一曲三十二秒 · 按键停舞");
        }
    } else if(page==1) {
        static const char *coat[]={"橘奶猫","奶油猫","灰团子"};
        for(unsigned i=0;i<6;i++) {
            switch(i) {
            case 0: snprintf(text,sizeof text,"名字    %s",cat_name(c)); break;
            case 1: snprintf(text,sizeof text,"毛色    %s",coat[c->profile.coat]); break;
            case 2: snprintf(text,sizeof text,"玩具    %s",cat_toy_name(c->profile.toy)); break;
            case 3: snprintf(text,sizeof text,"声音    %s",!audio ? "暂不可用" : c->profile.sound ? "轻轻响" : "安安静静"); break;
            case 4: snprintf(text,sizeof text,"陪伴    %s",c->profile.pocket ? "随身" : "桌面"); break;
            default: snprintf(text,sizeof text,"看看我们的小回忆"); break;
            }
            lv_label_set_text(u->rows[i],text);
            lv_obj_set_style_bg_color(u->rows[i],lv_color_hex(0xE8D2A7),0);
            lv_obj_set_style_bg_opa(u->rows[i],i==selection ? LV_OPA_COVER : LV_OPA_TRANSP,0);
        }
        static const char *tips[]={"选个顺口的小名","哪一只最像你的猫","换个玩具，回去陪它玩","只在互动时轻轻回应","随身闲置三分钟熄屏","那些一起待着的时刻"};
        lv_label_set_text(u->message,tips[selection%6]);
        lv_label_set_text(u->keys[0],"上 上一项"); lv_label_set_text(u->keys[1],selection==5 ? "确认 看看" : "确认 换换"); lv_label_set_text(u->keys[2],"下 下一项");
        lv_label_set_text(u->hint,"长按确认回到小猫身边");
    } else {
        for(unsigned i=0;i<6;i++) {
            lv_label_set_text(u->rows[i],c->profile.memories&(1u<<i) ? cat_memory(i) : "还会有新的小回忆");
            lv_obj_set_style_bg_opa(u->rows[i],LV_OPA_TRANSP,0);
            lv_obj_set_style_text_color(u->rows[i],lv_color_hex(c->profile.memories&(1u<<i) ? INK : 0x968674),0);
        }
        lv_label_set_text(u->message,"慢慢来，不用赶着长大");
        lv_label_set_text(u->keys[0],"上 返回"); lv_label_set_text(u->keys[1],"确认 返回"); lv_label_set_text(u->keys[2],"下 返回");
        lv_label_set_text(u->hint,"长按确认回到小猫身边");
    }
    if(page==1) for(unsigned i=0;i<6;i++) lv_obj_set_style_text_color(u->rows[i],lv_color_hex(INK),0);
    if(page==0) lv_obj_invalidate(u->scene);
}
void cat_ui_destroy(cat_ui_t *u) {
    if(!u->screen) return;
    lv_screen_load(u->previous_screen); lv_obj_delete(u->screen); u->screen=NULL;
}
