/* Actual production LVGL UI; exported frames are host previews, never capture receipts. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/rhythm_agent/rhythm_agent.c"
#include "src/misc/lv_text_private.h"
static int64_t fake_us;
static int soc=87,brightness;
static bool callback_active,available=true,busy;
static unsigned plays,stops,best;
static int note=-1;
int64_t esp_timer_get_time(void) {return fake_us;}
void bsp_display_backlight(uint8_t v) {assert(!callback_active);brightness=v;}
int ra_service_soc(void) {assert(!callback_active);return soc;}
unsigned ra_service_best(void) {return best;}
bool ra_service_saved(void) {return true;}
void ra_service_save(unsigned v) {assert(!callback_active);if(v>best)best=v;}
bool ra_audio_ready(void) {return available;}
bool ra_audio_busy(void) {return busy;}
int ra_audio_note_index(void) {return note;}
void ra_audio_stop(void) {assert(!callback_active);stops++;busy=false;}
void ra_audio_demo(const ra_pattern_t *p,unsigned v,bool spoken) {(void)spoken;assert(!callback_active && p->count<=8 && v<4);plays++;busy=true;}
void ra_audio_note(unsigned k,unsigned v) {assert(!callback_active && k<3 && v<4);plays++;}
void ra_audio_feedback(bool p,unsigned v,bool spoken) {(void)p;(void)spoken;assert(!callback_active && v<4);}
static void bounds(lv_obj_t *o) {
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)) return;
    if(lv_obj_check_type(o,&lv_label_class)) {
        const char *s=lv_label_get_text(o);if(!*s)return;
        lv_area_t a;lv_obj_get_coords(o,&a);const lv_font_t *f=lv_obj_get_style_text_font(o,0);
        lv_point_t size;lv_text_get_size(&size,s,f,0,2,lv_obj_get_width(o),LV_TEXT_FLAG_NONE);
        if(size.y>lv_obj_get_height(o)) fprintf(stderr,"Text too tall: %s need %d have %d\n",s,size.y,lv_obj_get_height(o));
        assert(size.y<=lv_obj_get_height(o));
        assert(a.x1>=0 && a.y1>=0 && a.x2<240 && a.y2<320);
        lv_obj_t *parent=lv_obj_get_parent(o);lv_area_t p;lv_obj_get_content_coords(parent,&p);
        if(a.x1<p.x1||a.x2>p.x2||a.y1<p.y1||a.y2>p.y2) fprintf(stderr,"Parent clip %s [%d,%d,%d,%d] in [%d,%d,%d,%d]\n",s,a.x1,a.y1,a.x2,a.y2,p.x1,p.y1,p.x2,p.y2);
        assert(a.x1>=p.x1 && a.x2<=p.x2 && a.y1>=p.y1 && a.y2<=p.y2);
        uint32_t i=0;while(s[i]) {uint32_t cp=lv_text_encoded_next(s,&i);if(cp=='\n')continue;lv_font_glyph_dsc_t d;assert(lv_font_get_glyph_dsc(f,&d,cp,0)&&!d.is_placeholder);assert(!(cp>='A'&&cp<='Z')&&!(cp>='a'&&cp<='z'));}
        /* Fixed rows must fit without hidden horizontal truncation. */
        const char *start=s;for(const char *c=s;;c++) if(*c=='\n'||!*c) {
            char row[200];size_t len=(size_t)(c-start);assert(len<sizeof(row));memcpy(row,start,len);row[len]=0;
            lv_text_get_size(&size,row,f,0,2,1000,LV_TEXT_FLAG_NONE);
            if(size.x>lv_obj_get_width(o))fprintf(stderr,"Row too wide %s: %d > %d\n",row,size.x,lv_obj_get_width(o));
            assert(size.x<=lv_obj_get_width(o));if(!*c)break;start=c+1;
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++)bounds(lv_obj_get_child(o,i));
}
static void check(void) {lv_obj_update_layout(screen);bounds(screen);}
static void snap(const char *name) {
    check();lv_draw_buf_t *b=lv_snapshot_take(screen,LV_COLOR_FORMAT_RGB888);assert(b);
    char path[100];snprintf(path,sizeof(path),"%s.ppm",name);FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++)for(unsigned x=0;x<b->header.w;x++) {uint8_t *p=b->data+y*b->header.stride+x*3;uint8_t rgb[]={p[2],p[1],p[0]};fwrite(rgb,1,3,f);}
    fclose(f);lv_draw_buf_destroy(b);
}
static void advance(unsigned ms) {fake_us+=(int64_t)ms*1000;lv_tick_inc(ms);if(timer)frame(timer);}
static void post(bsp_btn_t k,bsp_btn_ev_t e) {callback_active=true;rhythm_agent_key(k,e);callback_active=false;}
static void menu_key(bsp_btn_t k) {post(k,BSP_BTN_PRESS);advance(1);if(k==BSP_BTN_OK){post(k,BSP_BTN_CLICK);advance(1);}}
static void ready(void) {busy=false;note=-1;advance(10);if(game.phase!=RA_READY)fprintf(stderr,"ready fail: page=%d phase=%d visual=%d vol=%u time=%lld feedback=%lld ok=%d\n",page,game.phase,visual_demo,volume,(long long)now_ms(),(long long)feedback_started,game_ok_press);assert(game.phase==RA_READY);}
static void perform(bool wrong) {
    int64_t origin=now_ms()+1000;
    for(unsigned i=0;i<game.pattern.count;i++) {
        unsigned k=game.pattern.keys[i];if(wrong&&i==0)k=(k+1)%3;fake_us=(origin+game.pattern.at[i])*1000;
        post((bsp_btn_t)k,BSP_BTN_PRESS);advance(1);
        if(k==BSP_BTN_OK) {post(BSP_BTN_OK,BSP_BTN_CLICK);advance(1);}
    }
    assert(game.phase==RA_FEEDBACK);check();advance(500);
}
int main(void) {
    lv_init();assert(lv_display_create(240,320));rhythm_agent_prepare();rhythm_agent_enter(true);advance(1);snap("01-home");
    selected=2;render();menu_key(BSP_BTN_OK);assert(page==SETTINGS);snap("02-settings");
    selected=2;render();menu_key(BSP_BTN_OK);assert(page==HELP);snap("03-help-keys");menu_key(BSP_BTN_OK);snap("04-help-rhythm");menu_key(BSP_BTN_OK);
    page=HOME;selected=0;render();menu_key(BSP_BTN_OK);assert(page==GAME);note=1;advance(20);snap("05-listen");ready();snap("06-your-turn");
    perform(false);snap("07-success");menu_key(BSP_BTN_OK);ready();
    /* A long OK starts with a PRESS. It must roll back the whole attempt. */
    unsigned score=game.score,lives=game.lives;post(BSP_BTN_OK,BSP_BTN_PRESS);advance(20);post(BSP_BTN_OK,BSP_BTN_LONG);advance(800);
    assert(page==PAUSE && game.score==score && game.lives==lives);snap("08-paused");menu_key(BSP_BTN_OK);ready();
    perform(true);snap("09-retry");menu_key(BSP_BTN_OK);ready();
    for(unsigned door=game.door;door<RA_DOORS;door++) {perform(false);menu_key(BSP_BTN_OK);if(page==GAME)ready();}
    assert(page==RESULT&&game.completed);snap("10-result");uint32_t seed=game.seed;menu_key(BSP_BTN_OK);assert(game.seed==seed&&game.score==0);
    page=HOME;selected=1;render();menu_key(BSP_BTN_OK);ready();
    for(unsigned life=0;life<3;life++){perform(true);menu_key(BSP_BTN_OK);if(page==GAME)ready();}
    assert(page==RESULT&&!game.completed);snap("11-out-of-chances");
    /* Holding the final key must also undo a failed lock and its lost life. */
    ra_start(&game,12,RA_CHALLENGE);page=GAME;game.pattern.count=1;game.pattern.keys[0]=0;ra_ready(&game);
    game_ok_press=false;undo_valid=false;post(BSP_BTN_OK,BSP_BTN_PRESS);advance(10);assert(game.lives==2);
    post(BSP_BTN_OK,BSP_BTN_LONG);advance(800);assert(page==PAUSE&&game.lives==3&&game.score==0);
    /* Worst-case labels, all locks, all phases and marks. */
    for(unsigned mode=0;mode<2;mode++)for(unsigned door=0;door<8;door++) {
        ra_start(&game,9999,mode);game.door=door;ra_pattern(&game.pattern,9999,door,mode);page=GAME;
        for(unsigned phase=0;phase<4;phase++)for(unsigned mark=0;mark<6;mark++) {game.phase=phase;game.last_mark=mark;game.index=game.pattern.count;game.score=800;game.accuracy=100;render();check();}
    }
    page=SETTINGS;selected=0;for(unsigned v=0;v<4;v++) {volume=v;render();check();}
    available=false;render();snap("12-audio-fallback");page=HOME;selected=0;render();menu_key(BSP_BTN_OK);advance(10000);assert(game.phase==RA_READY);
    /* Burst overflow pauses instead of silently grading a dropped key. */
    for(unsigned i=0;i<20;i++)post(BSP_BTN_UP,BSP_BTN_PRESS);advance(1);assert(page==PAUSE);
    soc=-1;advance(10);snap("13-unknown-battery");
    page=HOME;render();advance(120001);assert(sleeping&&brightness==0);unsigned before=plays;menu_key(BSP_BTN_OK);assert(!sleeping&&page==HOME&&plays==before);
    rhythm_agent_exit();post(BSP_BTN_OK,BSP_BTN_CLICK);assert(!screen);
    rhythm_agent_enter(false);snap("14-buttons-unavailable");rhythm_agent_exit();
    for(unsigned i=0;i<20;i++){rhythm_agent_enter(true);menu_key(BSP_BTN_OK);rhythm_agent_exit();}
    puts("Rhythm production UI: Chinese glyphs, bounds, rows, 384 phase/mark combinations, long-hold rollback, replay, failure, wake and lifecycle PASS");
}
