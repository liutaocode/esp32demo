#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/listening/listening_ui.c"
static unsigned checks;
static void bounds(lv_obj_t *o){
 lv_area_t a;lv_obj_get_coords(o,&a);assert(a.x1>=0&&a.y1>=0&&a.x2<240&&a.y2<320);
 if(lv_obj_check_type(o,&lv_label_class)){
  const char *text=lv_label_get_text(o),*p=text;const lv_font_t *font=lv_obj_get_style_text_font(o,0);
  while(*p){uint32_t cp=(unsigned char)*p++;if(cp>=0xE0){cp=((cp&15)<<12)|(((unsigned char)p[0]&63)<<6)|((unsigned char)p[1]&63);p+=2;}else if(cp>=0xC0){cp=((cp&31)<<6)|((unsigned char)*p++&63);}
   lv_font_glyph_dsc_t d;assert(lv_font_get_glyph_dsc(font,&d,cp,0)&&!d.is_placeholder);
   if((cp>='A'&&cp<='Z')||(cp>='a'&&cp<='z'))assert(o==english_label);
  }
  if(lv_obj_get_height(o)>2*font->line_height){fprintf(stderr,"More than two lines: %s\n",text);assert(0);}
 }
 for(unsigned i=0;i<lv_obj_get_child_count(o);i++)bounds(lv_obj_get_child(o,i));
}
static void overlaps(lv_obj_t *parent){
 for(unsigned i=0;i<lv_obj_get_child_count(parent);i++){
  lv_obj_t *a=lv_obj_get_child(parent,i);overlaps(a);if(!lv_obj_check_type(a,&lv_label_class)||!*lv_label_get_text(a))continue;
  lv_area_t aa;lv_obj_get_coords(a,&aa);
  for(unsigned j=i+1;j<lv_obj_get_child_count(parent);j++){
   lv_obj_t *b=lv_obj_get_child(parent,j);if(!lv_obj_check_type(b,&lv_label_class)||!*lv_label_get_text(b))continue;
   lv_area_t bb;lv_obj_get_coords(b,&bb);
   if(!(aa.x2<bb.x1||bb.x2<aa.x1||aa.y2<bb.y1||bb.y2<aa.y1)){fprintf(stderr,"Overlap: %s / %s\n",lv_label_get_text(a),lv_label_get_text(b));assert(0);}
  }
 }
}
static void check(li_state_t *s){li_ui_render(s,87,true,1);assert((english_label!=NULL)==(s->page==LI_LISTEN||s->page==LI_FEEDBACK));if(english_label)assert(!strcmp(lv_label_get_text(english_label),li_english[li_current(s)]));lv_obj_update_layout(screen);bounds(screen);overlaps(screen);checks++;}
static void snap(const char *name){
 lv_draw_buf_t *b=lv_snapshot_take(screen,LV_COLOR_FORMAT_RGB888);assert(b);char path[120];snprintf(path,sizeof(path),"%s.ppm",name);FILE *f=fopen(path,"wb");assert(f);
 fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
 for(unsigned y=0;y<b->header.h;y++)for(unsigned x=0;x<b->header.w;x++){uint8_t *p=b->data+y*b->header.stride+x*3;uint8_t rgb[]={p[2],p[1],p[0]};fwrite(rgb,1,3,f);}fclose(f);lv_draw_buf_destroy(b);
}
int main(void){
 lv_init();assert(lv_display_create(240,320));li_state_t s;li_init(&s,NULL,3);s.voice_enabled=true;li_ui_create(&s);check(&s);snap("home");
 s.page=LI_TOPICS_PAGE;check(&s);snap("island");
 s.page=LI_LISTEN;s.count=LI_PER_TOPIC;s.pos=0;s.order[0]=0;check(&s);snap("listen");
 for(unsigned i=0;i<LI_COUNT;i++){
  s.order[0]=i;s.page=LI_LISTEN;check(&s);
  s.page=LI_QUIZ;s.options[0]=i;s.options[1]=(i+1)%LI_COUNT;check(&s);
  s.page=LI_FEEDBACK;s.right=i%2;check(&s);
 }
 s.page=LI_QUIZ;s.options[0]=24;s.options[1]=25;check(&s);snap("quiz");
 s.page=LI_FEEDBACK;s.order[0]=24;s.right=false;check(&s);snap("retry");
 s.page=LI_ALBUM;for(unsigned i=0;i<8;i++){s.topic=i;check(&s);}snap("album");
 s.page=LI_SETTINGS;check(&s);snap("settings");
 s.page=LI_FINISH;check(&s);snap("finish");
 s.timed_out=true;check(&s);snap("rest");
 s.page=LI_HOME;for(unsigned i=0;i<LI_COUNT;i++){s.progress.heard[i]=1;s.progress.mistakes[i]=1;}check(&s);
 li_ui_render(&s,-1,false,0);lv_obj_update_layout(screen);bounds(screen);overlaps(screen);snap("unavailable");
 printf("Production LVGL UI: PASS (%u states; all 384 translations, glyphs and label overlap checked)\n",checks);
}
