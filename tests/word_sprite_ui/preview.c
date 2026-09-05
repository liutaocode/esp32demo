/* Real LVGL/application rendering; simulated buttons, clock, battery and I/O. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps/word_sprite/word_sprite.c"

static int64_t fake_us;
static int backlight, soc = 87, voice_first, voice_second, voice_requests, saves;
static int explained_word = -1;
static bool audio_ok = true, worker_stopped, callback_active;
static ws_progress_t saved;
int64_t esp_timer_get_time(void) { return fake_us; }
uint32_t esp_random(void) { return 123456; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t value) { assert(!callback_active); backlight = value; }
bool ws_runtime_audio_ready(void) { return audio_ok; }
int ws_runtime_save_status(void) { return 1; }
void ws_runtime_speak(int a, int b)
{
    assert(!callback_active); voice_first = a; voice_second = b; voice_requests++;
}
void ws_runtime_stop_audio(void) { ws_runtime_speak(-1, -1); }
void ws_runtime_explain(unsigned word, int cue)
{
    assert(!callback_active && word < WS_WORDS);
    explained_word = (int)word;
    ws_runtime_speak(cue, WS_VOICE_MEANING_BASE + (int)word);
}
void ws_runtime_save(ws_progress_t p) { assert(!callback_active); saved = p; saves++; }
void ws_runtime_shutdown(void) { assert(!callback_active); worker_stopped = false; }
bool ws_runtime_stopped(void) { return worker_stopped; }

static void bounds(lv_obj_t *obj)
{
    lv_area_t a; lv_obj_get_coords(obj, &a);
    if (a.x1 < 0 || a.y1 < 0 || a.x2 >= 240 || a.y2 >= 320) {
        fprintf(stderr, "Out of bounds: %d,%d..%d,%d %s\n", (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2,
                lv_obj_check_type(obj, &lv_label_class) ? lv_label_get_text(obj) : "object");
    }
    assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
    if (lv_obj_check_type(obj, &lv_label_class)) {
        /* Labels must fit their allocated width, including the Chinese font. */
        assert(lv_obj_get_scroll_right(obj) <= 0 && lv_obj_get_scroll_bottom(obj) <= 0);
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++) bounds(lv_obj_get_child(obj, i));
}

static void snap(const char *name)
{
    lv_obj_update_layout(s_screen); bounds(s_screen);
    lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[100]; snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%u %u\n255\n", b->header.w, b->header.h);
    for (unsigned y = 0; y < b->header.h; y++) for (unsigned x = 0; x < b->header.w; x++) {
        uint8_t *p = b->data + y * b->header.stride + x * 3;
        uint8_t rgb[] = {p[2], p[1], p[0]}; fwrite(rgb, 1, 3, f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}

static void advance(unsigned ms) { fake_us += (int64_t)ms * 1000; lv_tick_inc(ms); tick(s_timer); }
static void key_event(bsp_btn_t b, bsp_btn_ev_t ev)
{
    callback_active = true; word_sprite_key(b, ev); callback_active = false; advance(30);
}
static void key(bsp_btn_t b) { key_event(b, BSP_BTN_CLICK); }

int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    word_sprite_prepare(); word_sprite_enter(true, (ws_progress_t){0}); advance(30); snap("home");
    int before = voice_requests;
    key_event(BSP_BTN_OK, BSP_BTN_PRESS); key_event(BSP_BTN_OK, BSP_BTN_DOUBLE);
    assert(s_state.page == WS_HOME && voice_requests == before);
    key(BSP_BTN_OK); assert(s_state.page == WS_QUESTION && voice_first == ws_word(&s_state)); snap("question");
    key_event(BSP_BTN_UP, BSP_BTN_LONG); assert(voice_first == ws_word(&s_state));
    for (unsigned n = 0; n < WS_ROUND; n++) {
        unsigned target = 0;
        while (s_state.options[target] != ws_word(&s_state)) target++;
        unsigned answer = n == 0 ? (target + 1) % 3 : target;
        while (s_state.selected != answer) key(BSP_BTN_DOWN);
        key(BSP_BTN_OK); assert(s_state.page == WS_FEEDBACK);
        assert(voice_first == (n == 0 ? WS_VOICE_RETRY : WS_VOICE_CORRECT));
        assert(voice_second == WS_VOICE_MEANING_BASE + ws_word(&s_state));
        assert(explained_word == ws_word(&s_state));
        key_event(BSP_BTN_UP, BSP_BTN_LONG);
        assert(voice_first == -1 && explained_word == ws_word(&s_state));
        if (!n) snap("correction");
        key(BSP_BTN_OK); assert(s_state.page == WS_FEEDBACK);
        advance(700); key(BSP_BTN_OK);
    }
    assert(s_state.page == WS_RESULT && s_state.correct == 5 && saves == 1); snap("result");
    assert(ws_count(saved.review) == 1);
    key(BSP_BTN_DOWN); assert(s_state.reviewing && s_state.count == 1);
    key_event(BSP_BTN_OK, BSP_BTN_LONG); assert(s_state.page == WS_HOME && saves == 2);
    /* Dark-screen wake cannot also select an island or submit an answer. */
    unsigned world = s_state.world;
    advance(60000); assert(backlight == 20);
    advance(120000); assert(backlight == 0);
    key(BSP_BTN_DOWN); assert(backlight == 100 && s_state.world == world);
    key(BSP_BTN_DOWN); assert(s_state.world == (world + 1) % 3);
    audio_ok = false; key(BSP_BTN_OK); snap("reading-mode");
    assert(strcmp(lv_label_get_text(lv_obj_get_child(s_content, 1)), ws_words[ws_word(&s_state)].english) == 0);
    soc = -1; advance(10001); assert(strcmp(lv_label_get_text(s_battery), "--%") == 0);
    /* Exercise every word's visible answer and both degraded status lines. */
    for (unsigned i = 0; i < WS_WORDS; i++) {
        s_state.page = WS_FEEDBACK; s_state.index = 0; s_state.count = 1; s_state.deck[0] = i;
        render(); lv_obj_update_layout(s_screen); bounds(s_screen);
    }
    s_state.progress = (ws_progress_t){WS_VALID_MASK, 0};
    audio_ok = true; soc = 87; s_last_audio = true;
    ws_home(&s_state); render(); advance(10001); snap("crown");
    for (unsigned i = 0; i < 50; i++) {
        word_sprite_exit(); advance(30); assert(s_screen); /* Wait for worker handshake. */
        key(BSP_BTN_OK); assert(s_closing);
        worker_stopped = true; advance(30); assert(!s_screen && !s_timer);
        word_sprite_enter(true, saved); assert(!s_placeholder);
    }
    word_sprite_exit(); worker_stopped = true; advance(30);
    word_sprite_enter(false, saved); key(BSP_BTN_OK); assert(s_state.page == WS_HOME); snap("no-buttons");
    puts("Word Sprite UI: input, TTS dispatch, save, idle/wake, fallback, bounds, 50 exits PASS");
}
