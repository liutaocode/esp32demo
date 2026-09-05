#include "pvz_art.h"
#include "pvz_catalog.h"
#include "ui_pixel.h"
static lv_obj_t *shape(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius) {
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0); lv_obj_set_style_bg_opa(o,255,0);
    lv_obj_set_style_radius(o,radius,0); lv_obj_set_style_border_width(o,2,0);
    lv_obj_set_style_border_color(o,lv_color_hex(0x304C38),0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}
static void eyes(lv_obj_t *p,int x,int y) {
    shape(p,x,y,6,9,0xFFFCE7,2); shape(p,x+15,y,6,9,0xFFFCE7,2);
    shape(p,x+2,y+3,3,5,UI_INK,0); shape(p,x+17,y+3,3,5,UI_INK,0);
}
lv_obj_t *pvz_art(lv_obj_t *p,unsigned id,int x,int y) {
    if(id>=PVZ_COUNT) return NULL;
    lv_obj_t *a=lv_obj_create(p); lv_obj_remove_style_all(a);
    lv_obj_set_pos(a,x,y); lv_obj_set_size(a,84,88); lv_obj_remove_flag(a,LV_OBJ_FLAG_SCROLLABLE);
    uint32_t c=pvz_catalog[id].color;
    shape(a,7,76,69,8,0xACBE93,4);
    if(id>=PVZ_PLANTS) {
        shape(a,24,65,11,15,0x695444,0); shape(a,48,65,11,15,0x695444,0);
        shape(a,22,41,40,29,id==21?0xCB4E40:0x765C74,3);
        shape(a,36,44,9,19,0xE5A363,0);
        shape(a,23,15,40,35,0xA2B494,8); eyes(a,29,25);
        shape(a,36,39,18,5,0xFFF2D5,0);
        if(id==17) {
            shape(a,25,4,36,16,0xEB883C,0); shape(a,33,0,20,13,0xEB883C,0);
            shape(a,27,10,32,5,0xFFF1D0,0);
        } else if(id==18) { shape(a,21,4,45,20,0xB6C6CA,2); shape(a,30,9,21,4,0xECF4EE,0); }
        else if(id==19) shape(a,72,6,5,75,0xDCA663,0);
        else if(id==20) {
            shape(a,12,45,58,23,0xF3ECDD,0);
            for(int i=0;i<3;i++) shape(a,18,50+i*5,42-i*7,2,0x667872,0);
        } else if(id==21) {shape(a,18,5,49,20,0xDE5B45,5);shape(a,17,22,52,5,0xD5D8CC,0);}
        else if(id==22) {shape(a,65,20,2,40,UI_INK,0);shape(a,48,0,34,32,0xD992C2,15);}
        else if(id==23) {shape(a,4,24,9,56,0xA37E50,1);shape(a,0,22,23,13,0x806348,0);shape(a,60,39,18,23,0xA2B494,4);}
        return a;
    }
    if(id==9) {shape(a,10,47,65,28,c,13);shape(a,39,47,30,5,0xC3DD86,0);eyes(a,26,53);return a;}
    shape(a,37,46,9,31,0x559349,0);
    shape(a,15,62,26,13,0x71AF48,7); shape(a,43,62,25,13,0x71AF48,7);
    if(id==0 || id==5 || id==7) {
        shape(a,18,15,43,37,c,17);shape(a,51,26,25,20,c,7);
        shape(a,66,30,8,12,0x28563B,4);
        lv_obj_t *shine = shape(a,25,19,13,5,0xB1D782,3);
        lv_obj_set_style_border_width(shine,0,0);shape(a,31,22,6,10,UI_INK,2);
        if(id==7) shape(a,20,9,25,10,0x27643E,1);
        shape(a,76,38,7,7,id==5?0xE0FCFF:0xC7EB71,3);
    } else if(id==1) {
        for(int i=0;i<3;i++) for(int j=0;j<3;j++)
            shape(a,14+i*18,8+j*18,20,20,c,7);
        shape(a,23,18,39,39,0x9C623A,17);eyes(a,31,27);
        shape(a,35,44,15,4,0xF5D291,2);
    } else if(id==2) {
        shape(a,31,3,6,29,0x5C914B,0); shape(a,37,9,19,5,0x5C914B,0);
        shape(a,6,27,37,38,c,16);shape(a,41,20,37,39,c,16);
        eyes(a,15,36);eyes(a,49,30);
    } else if(id==3 || id==13 || id==4 || id==10) {
        int top=id==13?2:20;
        shape(a,18,top,49,75-top,c,14);eyes(a,30,top+15);
        shape(a,36,top+34,16,4,0x724E38,1);
        if(id==4) {shape(a,5,67,74,10,0x947348,1);shape(a,39,7,6,17,0xCF514A,1);}
        if(id==10) {shape(a,25,30,11,3,UI_INK,0);shape(a,48,30,11,3,UI_INK,0);}
    } else if(id==6) {
        shape(a,10,10,65,48,c,18);shape(a,22,34,50,19,0x593257,5);
        for(int i=0;i<4;i++) shape(a,28+i*10,35,6,9,0xFFF7DF,0);
        shape(a,27,21,6,8,UI_INK,1);
    } else if(id==8 || id==15) {
        shape(a,28,36,30,32,0xEAD8AC,8);eyes(a,32,45);
        shape(a,10,16,66,31,c,13);
        if(id==8) {shape(a,20,23,12,10,0xEFE6DA,5);shape(a,49,20,12,10,0xEFE6DA,5);}
        else {shape(a,28,1,9,26,0xCE514E,0);shape(a,49,1,9,26,0x6898BD,0);shape(a,30,21,27,8,0xD0D4D6,0);}
    } else if(id==11) {
        shape(a,29,16,27,49,c,12);shape(a,44,51,23,13,c,4);
        shape(a,39,5,7,18,0x428543,2);eyes(a,31,26);
    } else if(id==12) {
        shape(a,20,33,48,42,0xAC7642,7);eyes(a,31,46);
        shape(a,21,12,45,24,0xF69A36,7);shape(a,34,0,19,33,0xFFCF51,5);
    } else if(id==14) {
        shape(a,30,9,27,66,c,11);shape(a,9,37,24,12,c,4);shape(a,10,22,11,26,c,4);
        shape(a,55,46,22,12,c,4);shape(a,65,28,11,29,c,4);eyes(a,32,23);
        for(int i=0;i<3;i++) shape(a,35,48+i*8,4,3,0xE8E6BA,0);
    }
    return a;
}
