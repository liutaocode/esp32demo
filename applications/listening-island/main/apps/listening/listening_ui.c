#include "listening_ui.h"
#include "listening_catalog.h"
#include "ui_pixel.h"
#include <stdio.h>
LV_FONT_DECLARE(listening_zh_16);
LV_FONT_DECLARE(listening_zh_22);
static lv_obj_t *screen,*body,*battery_label,*english_label;
static const uint32_t colors[8]={0xFFD07C,0xFFDCA8,0xAADBEF,0xA5DDB2,0xFFBB98,0xBDE6AE,0xCABAF0,0xBCCBFA};
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const char *text,bool big,uint32_t color) {
 lv_obj_t *o=ui_pixel_label(parent,text,big?&listening_zh_22:&listening_zh_16,color);
 lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);return o;
}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color) {
 lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
 lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_radius(o,0,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);return o;
}
static void panel(int y,int h,uint32_t color) {lv_obj_t *p=ui_pixel_panel_create(body,10,y,215,h,color);(void)p;}
static void row(int y,const char *text,bool selected) {
 box(body,15,y,205,32,selected?UI_YELLOW:UI_PAPER);
 if(selected)box(body,15,y,4,32,UI_INK);
 label(body,21,y+5,193,text,false,UI_INK);
}
static void island(unsigned topic) {
 /* Code-native pixel art, with no bitmap RAM allocation. */
 box(body,52,58,128,8,0xB6EEFD);box(body,42,69,146,5,0x8EDBEF);
 box(body,72,36,97,24,0x866148);box(body,62,30,117,16,0x55951D);box(body,72,25,97,9,0xA7D93E);
 box(body,135,0,6,33,0x68462D);box(body,117,0,40,16,colors[topic]);
 box(body,91,10,28,24,0xFFFFFF);box(body,87,5,36,9,colors[topic]);box(body,101,21,8,13,0x76502D);
}
void li_ui_create(li_state_t *s) {
 screen=ui_pixel_screen_create("");
 label(screen,13,13,137,"英语磨耳朵",true,0xFFFFFF);
 battery_label=label(screen,162,29,75,"电量 --",false,0xFFFFFF);
 body=box(screen,0,49,240,271,UI_SKY);
 /* Keep the shared grass and mascot identity visible on the home page. */
 li_ui_render(s,-1,true,1);lv_screen_load(screen);
}
void li_ui_render(li_state_t *s,int battery,bool audio,int saving) {
 char b[96];snprintf(b,sizeof(b),battery<0?"电量 --":"电量 %d",battery);lv_label_set_text(battery_label,b);
 lv_obj_clean(body);english_label=NULL;box(body,0,238,240,33,UI_GRASS);box(body,0,238,240,4,0xA7D93E);
 const char *footer="长按确定回首页";
 switch(s->page) {
 case LI_HOME: {
  static const char *const menus[]={"随身听","听力寻宝","错题回听","我的印章","声音与定时"};
  label(body,10,1,171,"每天听一点，慢慢懂",false,0xFFFFFF);
  ui_pixel_mascot_create(body,185,0);
  for(unsigned i=0;i<5;i++)row(55+(int)i*32,menus[i],s->menu==i);
  snprintf(b,sizeof(b),"已听 %u 句  待复习 %u",li_heard(s),li_mistakes(s));label(body,10,216,220,b,false,0xFFFFFF);
  footer="上下选玩法  确定进入";break;
 }
 case LI_TOPICS_PAGE:case LI_ALBUM:
  island(s->topic);panel(86,135,UI_PAPER);
  label(body,22,97,191,li_topics[s->topic],true,UI_INK);
  snprintf(b,sizeof(b),"第 %u 座岛  /  共 8 座",s->topic+1);label(body,20,132,195,b,false,0x45617A);
  if(s->page==LI_ALBUM)snprintf(b,sizeof(b),"印章 %u / 3",s->progress.stamps[s->topic]);
  else snprintf(b,sizeof(b),s->mode==0?"48 句短句  连听两遍":"每轮 8 题  不限答题时间");
  label(body,20,160,195,b,false,UI_INK);
  label(body,20,190,195,s->page==LI_ALBUM?"答对六题起获得印章":"上下换岛  确定出发",false,UI_INK);break;
 case LI_LISTEN:
  label(body,10,0,220,li_topics[li_current(s)/LI_PER_TOPIC],false,0xFFFFFF);
  panel(29,153,colors[li_current(s)/LI_PER_TOPIC]);
  snprintf(b,sizeof(b),"第 %u / %u 句",s->pos+1,s->count);label(body,20,41,195,b,false,UI_INK);
  english_label=label(body,20,68,195,li_english[li_current(s)],false,UI_INK);
  label(body,20,112,195,li_meanings[li_current(s)],false,0x36547A);
  label(body,20,156,195,!audio?"声音未就绪":s->paused?"已暂停":s->waiting?"留点空白，跟读一遍":"正在听英语",false,UI_INK);
  label(body,10,196,220,"上句 / 下句  确定暂停",false,0xFFFFFF);
  snprintf(b,sizeof(b),"%u 分钟自动休息",s->progress.minutes);label(body,10,219,220,b,false,0xFFFFFF);break;
 case LI_QUIZ:
  snprintf(b,sizeof(b),"听力寻宝  %u / 8",s->pos+1);label(body,10,0,220,b,false,0xFFFFFF);
  panel(27,59,colors[s->topic]);label(body,20,37,195,audio?"听一句，找中文":"声音未就绪",true,UI_INK);
  for(unsigned i=0;i<2;i++) {
   box(body,13,100+i*60,211,54,s->selection==i?UI_YELLOW:UI_PAPER);
   if(s->selection==i)box(body,13,100+i*60,4,54,UI_INK);
   label(body,23,106+i*60,191,li_meanings[s->options[i]],false,UI_INK);
  }
  label(body,10,217,220,"上下选答案  确定提交",false,0xFFFFFF);footer="长按上键再听一遍";break;
 case LI_FEEDBACK:
  label(body,10,1,220,s->right?"听懂了！":"再听一次就更熟",true,0xFFFFFF);
  panel(38,145,s->right?0xB6E9B5:0xFFD9AC);
  english_label=label(body,20,52,195,li_english[li_current(s)],false,UI_INK);
  label(body,20,98,195,li_meanings[li_current(s)],false,0x36547A);
  label(body,20,150,195,s->right?"跟着声音说一遍吧":"已经放进错题回听",false,UI_INK);
  label(body,10,205,220,"上下重听  确定下一句",false,0xFFFFFF);break;
 case LI_FINISH:
  island(s->topic);panel(86,137,UI_PAPER);
  label(body,20,100,195,s->timed_out?"到休息时间啦":s->count?"这一站完成啦":"还没有错题",true,UI_INK);
  if(s->mode==1)snprintf(b,sizeof(b),"本轮听懂 %u / 8 句",s->correct);
  else snprintf(b,sizeof(b),s->count?"闭上眼睛，休息一会儿":"去听力寻宝试试吧");
  label(body,20,143,195,b,false,UI_INK);
  label(body,20,187,195,"按确定回首页",false,UI_INK);break;
 case LI_SETTINGS:
  label(body,10,5,220,"调到舒服的声音",true,0xFFFFFF);
  snprintf(b,sizeof(b),"音量  %u / 5",s->progress.volume/20);row(61,b,s->setting==0);
  snprintf(b,sizeof(b),"定时休息  %u 分钟",s->progress.minutes);row(106,b,s->setting==1);
  label(body,20,158,200,"上下选择  确定调整",false,0xFFFFFF);
  label(body,20,192,200,"连听结束也会自动停下",false,0xFFFFFF);break;
 }
 if(!audio && s->page!=LI_LISTEN && s->page!=LI_QUIZ) footer="声音不可用，请重启";
 else if(saving==0 || saving==3)footer="记录未保存，请重启";
 label(body,5,247,230,footer,false,UI_INK);
}
