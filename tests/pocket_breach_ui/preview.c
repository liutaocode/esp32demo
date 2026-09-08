#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/pocket_breach/pocket_breach.c"
#include "src/misc/lv_text_private.h"
static int64_t fake_us;
static int soc=87,backlight,mv=3300;
static bool in_callback,audio=true,runtime_active;
int64_t esp_timer_get_time(void){return fake_us;}
int bsp_button_read_mv(void){assert(!in_callback);return mv;}
void bsp_display_backlight(uint8_t v){assert(!in_callback);backlight=v;}
int pb_runtime_battery(void){assert(!in_callback);return soc;}
bool pb_runtime_audio_ok(void){return audio;}
void pb_runtime_active(bool a){assert(!in_callback);runtime_active=a;}
void pb_runtime_sound(unsigned s){assert(!in_callback);(void)s;}
static void bounds(lv_obj_t *o)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t a; lv_obj_get_coords(o, &a);
    if(a.x1<0||a.y1<0||a.x2>=240||a.y2>=320)fprintf(stderr,"Bounds %d,%d %d,%d text=%s\n",a.x1,a.y1,a.x2,a.y2,lv_obj_check_type(o,&lv_label_class)?lv_label_get_text(o):"object");
    assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *str = lv_label_get_text(o);
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_point_t size;
        lv_text_get_size(&size, str, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o)) fprintf(stderr, "Text overflow: %s (%d > %d)\n", str, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        uint32_t i = 0;
        while (str[i]) {
            uint32_t cp = lv_text_encoded_next(str, &i);
            if (cp == '\n') continue;
            lv_font_glyph_dsc_t d;
            assert(lv_font_get_glyph_dsc(font, &d, cp, 0) && !d.is_placeholder);
        }
        assert(lv_obj_get_scroll_bottom(o) <= 0);
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) bounds(lv_obj_get_child(o, i));
}

static void check(void) { lv_obj_update_layout(screen); bounds(screen); }
static void snap(const char *name)
{
    check();
    lv_draw_buf_t *b = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[100]; snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%u %u\n255\n", b->header.w, b->header.h);
    for (unsigned y = 0; y < b->header.h; y++) for (unsigned x = 0; x < b->header.w; x++) {
        uint8_t *p = b->data + y * b->header.stride + x * 3;
        uint8_t rgb[] = {p[2], p[1], p[0]}; fwrite(rgb, 1, 3, f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
static void advance(unsigned ms){fake_us+=(int64_t)ms*1000;lv_tick_inc(ms);frame(timer);}
static void post(bsp_btn_t b,bsp_btn_ev_t e){in_callback=true;pocket_breach_key(b,e);in_callback=false;}
static void key(bsp_btn_t b){post(b,BSP_BTN_PRESS);advance(50);}
int main(void){
    lv_init();assert(lv_display_create(240,320));pocket_breach_enter(true);snap("home");
    for(unsigned m=0;m<3;m++){game.map=m;render();check();key(BSP_BTN_OK);snap(m==0?"sand":m==1?"harbor":"neon");game.page=PB_HOME;overlay();render();}
    game.map=0;key(BSP_BTN_OK);assert(game.page==PB_FIGHT);
    unsigned shots=game.shots;post(BSP_BTN_OK,BSP_BTN_CLICK);post(BSP_BTN_OK,BSP_BTN_DOUBLE);advance(50);assert(game.shots==shots);
    key(BSP_BTN_UP);float a=game.aim;mv=0;advance(300);assert(game.aim!=a);mv=3300;advance(50);a=game.aim;advance(400);assert(a==game.aim||game.locked>=0);
    post(BSP_BTN_OK,BSP_BTN_LONG);advance(50);assert(game.page==PB_PAUSE);snap("pause");
    unsigned elapsed=game.elapsed;advance(60000);assert(game.elapsed==elapsed&&backlight==20);
    key(BSP_BTN_OK);assert(backlight==100&&game.page==PB_PAUSE);key(BSP_BTN_OK);assert(game.page==PB_FIGHT);
    for(unsigned i=0;i<20;i++){advance(500);assert(game.page==PB_FIGHT);check();}
    game.aim=PB_PI;game.locked=-1;render();snap("behind");
    game.aim=-1.1f;render();snap("right-threat");
    game.aim=1.1f;render();snap("left-threat");
    game.hurt_bearing=game.base-1;game.hurt_ms=1400;game.aim=0;render();snap("attacked-left");
    game.hurt_ms=0;game.aim=pb_bearing(&game,0)+game.aim;game.locked=0;render();snap("locked");
    assert(lv_obj_has_flag(threat,LV_OBJ_FLAG_HIDDEN));
    game.enemy[0].hide_ms=800;game.locked=-1;render();snap("behind-crate");
    game.enemy[0].hide_ms=0;game.enemy[0].rise_ms=100;render();snap("peeking");
    game.enemy[0].rise_ms=0;game.locked=0;render();snap("crate-target");
    for(int i=1;i<PB_ENEMIES;i++)game.enemy[i].hp=0;
    game.enemy[0].hp=1;game.cooldown=game.reload=game.settle=0;game.ammo=6;
    pb_fire(&game);assert(!pb_alive(&game));advance(50);snap("last-falling");
    for(int i=0;i<7;i++)advance(50);assert(!game.enemy[0].death_ms);snap("cleared");
    game.page=PB_FIGHT;game.settle=0;
    game.ammo=0;game.reload=700;render();snap("reload");
    game.damage=200;game.health=1;game.wave=12;game.ammo=6;game.reload=0;render();snap("danger");
    game.page=PB_TRAVEL;overlay();render();snap("travel");
    game.page=PB_RESULT;game.won=true;game.score=12345;game.best[0]=12345;game.kills=40;game.max_combo=40;game.hits=48;game.shots=50;overlay();render();snap("victory");
    game.won=false;overlay();render();snap("loss");uint32_t seed=game.seed;key(BSP_BTN_OK);assert(game.seed==seed&&game.page==PB_FIGHT);
    for(soc=-1;soc<=101;soc++){render();check();}
    for(int i=0;i<40;i++){pocket_breach_exit();assert(!screen&&!timer&&!runtime_active);post(BSP_BTN_OK,BSP_BTN_PRESS);assert(queue->count==0);pocket_breach_enter(true);key(BSP_BTN_OK);check();}
    pocket_breach_exit();audio=false;pocket_breach_enter(false);key(BSP_BTN_OK);assert(game.page==PB_HOME);snap("fallback");pocket_breach_exit();
    puts("Pocket Breach UI: production renders, Chinese glyphs, bounds, input, holds, pause, wake, lifecycle and fallback PASS");
}
