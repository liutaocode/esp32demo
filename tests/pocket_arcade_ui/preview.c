/* Execute the production LVGL page with simulated board I/O:
   every page of the hub and every one of the twelve games is laid out with the
   real fonts, checked for overflow and missing glyphs, and captured. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/pocket_arcade/pocket_arcade.c"
#include "apps/pocket_arcade/pa_picture.h"
#include "src/misc/lv_text_private.h"

static int64_t fake_us;
static int backlight, soc = 71;
static bool callback_active;
static pa_record_t persisted;
static unsigned saves;

int64_t esp_timer_get_time(void) { return fake_us; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t value) { assert(!callback_active); backlight = value; }
pa_record_t pa_storage_init(void) { assert(!callback_active); return persisted; }
void pa_storage_save(const pa_record_t *record) { persisted = *record; saves++; }
unsigned pa_storage_status(void) { return 0; }

/* A Chinese interface may still show a single-letter card rank, but never an
   English word, so two letters in a row is the thing to catch. */
static void no_english(const char *text)
{
    unsigned run = 0;
    for (const char *c = text; *c; c++) {
        bool letter = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z');
        run = letter ? run + 1 : 0;
        if (run >= 2) {
            fprintf(stderr, "English leaked into the interface: %s\n", text);
            assert(run < 2);
        }
    }
}

static void bounds(lv_obj_t *object)
{
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    assert(area.x1 >= 0 && area.y1 >= 0 && area.x2 < 240 && area.y2 < 320);
    if (lv_obj_check_type(object, &lv_label_class) &&
        !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
        const char *text = lv_label_get_text(object);
        const lv_font_t *font = lv_obj_get_style_text_font(object, 0);
        lv_point_t size;
        lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(object))
            fprintf(stderr, "Text overflow: %s (%d > %d)\n", text, (int)size.x,
                    (int)lv_obj_get_width(object));
        assert(size.x <= lv_obj_get_width(object));
        uint32_t index = 0;
        while (text[index]) {
            uint32_t code = lv_text_encoded_next(text, &index);
            if (code == '\n') continue;
            lv_font_glyph_dsc_t glyph;
            if (!lv_font_get_glyph_dsc(font, &glyph, code, 0) || glyph.is_placeholder)
                fprintf(stderr, "Missing glyph U+%04X in: %s\n", (unsigned)code, text);
            assert(lv_font_get_glyph_dsc(font, &glyph, code, 0) && !glyph.is_placeholder);
        }
        no_english(text);
        /* Text always sits above the blocks: the two layers guarantee it. */
        assert(lv_obj_get_parent(object) != s_rect_layer);
        if (lv_obj_get_parent(object) == s_text_layer) {
            lv_area_t panel;
            lv_obj_get_content_coords(s_content, &panel);
            if (area.y2 > panel.y2)
                fprintf(stderr, "Panel clips %s at %d > %d\n", text, (int)area.y2,
                        (int)panel.y2);
            assert(area.x1 >= panel.x1 && area.x2 <= panel.x2);
            assert(area.y1 >= panel.y1 && area.y2 <= panel.y2);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(object); i++)
        bounds(lv_obj_get_child(object, i));
}

static void check(void)
{
    lv_obj_update_layout(s_screen);
    bounds(s_screen);
}

static void snap(const char *name)
{
    check();
    lv_draw_buf_t *buffer = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888);
    assert(buffer);
    char path[100];
    snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n%u %u\n255\n", buffer->header.w, buffer->header.h);
    for (unsigned y = 0; y < buffer->header.h; y++)
        for (unsigned x = 0; x < buffer->header.w; x++) {
            uint8_t *pixel = buffer->data + y * buffer->header.stride + x * 3;
            uint8_t rgb[] = {pixel[2], pixel[1], pixel[0]};
            fwrite(rgb, 1, 3, file);
        }
    fclose(file);
    lv_draw_buf_destroy(buffer);
}

static void advance(unsigned ms)
{
    fake_us += (int64_t)ms * 1000;
    lv_tick_inc(ms);
    frame(s_timer);
}

static void post(bsp_btn_t button, bsp_btn_ev_t event)
{
    callback_active = true;
    pocket_arcade_key(button, event);
    callback_active = false;
}

/* Deliberate presses wait for the page guard to open first, which is exactly
   what a person does: look at the new screen, then press again. */
static void settle(void)
{
    for (unsigned i = 0; i < 14; i++) advance(FRAME_MS);
}

static void tap_fast(bsp_btn_t button)
{
    post(button, BSP_BTN_PRESS);
    if (button == BSP_BTN_OK) post(button, BSP_BTN_CLICK);
    advance(FRAME_MS);
}

static void tap(bsp_btn_t button)
{
    settle();
    tap_fast(button);
}

static void double_tap(void)
{
    settle();
    post(BSP_BTN_OK, BSP_BTN_PRESS);
    post(BSP_BTN_OK, BSP_BTN_PRESS);
    post(BSP_BTN_OK, BSP_BTN_DOUBLE);
    advance(FRAME_MS);
    advance(PA_CLICK_WAIT_MS);
}

static void back_to_menu(void)
{
    settle();
    post(BSP_BTN_OK, BSP_BTN_LONG);
    advance(FRAME_MS);
    assert(s_hub.page == PA_PAGE_MENU);
}

static void hold(bsp_btn_t button)
{
    settle();
    post(button, BSP_BTN_LONG);
    advance(FRAME_MS);
}

static void select_game(unsigned index)
{
    assert(s_hub.page == PA_PAGE_MENU);
    while (s_hub.cursor != index) tap(BSP_BTN_DOWN);
    tap(BSP_BTN_OK);
    assert(s_hub.page == PA_PAGE_BRIEF && s_hub.game == index);
    tap(BSP_BTN_OK);
    assert(s_hub.page == PA_PAGE_PLAY);
}

/* Play a game the way a distracted passenger would: keep pressing, keep
   checking that nothing overflows or falls out of the panel. */
static void play_a_while(unsigned frames, uint32_t seed)
{
    uint32_t state = seed ? seed : 1U;
    for (unsigned i = 0; i < frames; i++) {
        unsigned roll = pa_below(&state, 8);
        if (roll == 0) tap_fast(BSP_BTN_UP);
        else if (roll == 1) tap_fast(BSP_BTN_DOWN);
        else if (roll == 2) tap_fast(BSP_BTN_OK);
        else advance(FRAME_MS);
        check();
        if (s_hub.page != PA_PAGE_PLAY) return;
    }
}

int main(void)
{
    lv_init();
    assert(lv_display_create(240, 320));
    pocket_arcade_prepare();
    pocket_arcade_enter(true);
    assert(s_hub.page == PA_PAGE_MENU);
    snap("menu");

    /* The list scrolls, wraps and keeps the cursor on screen the whole way. */
    for (unsigned i = 0; i < PA_GAME_COUNT + 3; i++) {
        assert(s_hub.cursor >= s_hub.top && s_hub.cursor < s_hub.top + PA_MENU_ROWS);
        check();
        tap(BSP_BTN_DOWN);
    }
    while (s_hub.cursor) tap(BSP_BTN_UP);
    snap("menu-top");

    /* Holding a direction jumps a whole page, which is the only sane way to
       reach the far end of a twenty-four game list. */
    hold(BSP_BTN_DOWN);
    assert(s_hub.cursor == PA_MENU_ROWS);
    hold(BSP_BTN_UP);
    assert(s_hub.cursor == 0);
    hold(BSP_BTN_UP);
    assert(s_hub.cursor == PA_GAME_COUNT - PA_MENU_ROWS);
    assert(s_hub.cursor >= s_hub.top && s_hub.cursor < s_hub.top + PA_MENU_ROWS);
    snap("menu-page");
    while (s_hub.cursor) tap(BSP_BTN_DOWN);

    /* Every game: read its rules, play it, capture it, come back. */
    for (unsigned index = 0; index < PA_GAME_COUNT; index++) {
        while (s_hub.cursor != index) tap(BSP_BTN_DOWN);
        tap(BSP_BTN_OK);
        assert(s_hub.page == PA_PAGE_BRIEF);
        check();
        if (index == 0) snap("brief");
        tap(BSP_BTN_OK);
        assert(s_hub.page == PA_PAGE_PLAY);
        /* Capture a live frame. Random hammering can finish a short game, so
           restart and try again rather than photographing the result page. */
        for (unsigned attempt = 0; attempt < 4; attempt++) {
            play_a_while(50, index * 977U + attempt * 31U + 13U);
            if (s_hub.page == PA_PAGE_PLAY) break;
            assert(s_hub.page == PA_PAGE_OVER);
            check();
            tap(BSP_BTN_OK);
            for (unsigned i = 0; i < 6; i++) advance(FRAME_MS);
        }
        assert(s_hub.page == PA_PAGE_PLAY);
        /* Mole Holes spends part of every second with all three holes empty;
           wait for a frame that actually shows the game. */
        if (pa_game_at(index) == &pa_game_mole)
            for (unsigned i = 0; i < 200; i++) {
                const pa_mole_t *mole = &s_hub.run.u.mole;
                if (mole->kind[0] || mole->kind[1] || mole->kind[2]) break;
                advance(FRAME_MS);
            }
        /* Nonogram only prints the picture's name once a picture is finished,
           so finish one: that line is the last place a glyph could be missing. */
        if (pa_game_at(index) == &pa_game_nono) {
            pa_nono_t *nono = &s_hub.run.u.nono;
            /* Wipe whatever the random hammering left behind first. */
            memset(nono->fill, 0, sizeof(nono->fill));
            memset(nono->cross, 0, sizeof(nono->cross));
            nono->paints = 0;
            nono->budget = 400;
            for (unsigned y = 0; y < PA_NONO && !nono->clear_ms; y++)
                for (unsigned x = 0; x < PA_NONO && !nono->clear_ms; x++) {
                    if (!((nono->target[y] >> x) & 1U)) continue;
                    nono->cy = (uint8_t)y;
                    nono->cx = (uint8_t)x;
                    pa_hub_key(&s_hub, PA_KEY_OK);
                    /* A click is held back until the double-click window closes,
                       and one frame only advances a bounded slice of time. */
                    for (unsigned i = 0; i < 20 && s_hub.click_pending; i++)
                        advance(FRAME_MS);
                }
            assert(nono->clear_ms > 0);
            check();
            snap("nonogram-solved");
        }
        char name[40];
        snprintf(name, sizeof(name), "game-%02u", index + 1U);
        snap(name);
        back_to_menu();
    }
    snap("menu-played");

    /* The result page, reached the way players reach it: by losing. */
    unsigned bird_index = PA_GAME_COUNT;
    for (unsigned i = 0; i < PA_GAME_COUNT; i++)
        if (pa_game_at(i) == &pa_game_bird) bird_index = i;
    assert(bird_index < PA_GAME_COUNT);
    select_game(bird_index);
    /* tap() waits out the page guard first, which is exactly what a person
       does. A press inside the guard is dropped, and before the bird had a
       ready state that dropped press was the whole game. */
    tap(BSP_BTN_OK);                      /* take off, then never flap again */
    for (unsigned i = 0; i < 400 && s_hub.page == PA_PAGE_PLAY; i++) advance(FRAME_MS);
    assert(s_hub.page == PA_PAGE_OVER);
    snap("over");
    assert(s_hub.record.plays == 1 || s_hub.record.plays > 1);
    tap(BSP_BTN_OK);
    assert(s_hub.page == PA_PAGE_PLAY);
    back_to_menu();

    /* Turn-based games really do take a double click as its own action. */
    unsigned merge_index = PA_GAME_COUNT;
    for (unsigned i = 0; i < PA_GAME_COUNT; i++)
        if (pa_game_at(i)->ok_mode == PA_OK_CLICK) merge_index = i;
    assert(merge_index < PA_GAME_COUNT);
    select_game(merge_index);
    assert(pa_hub_wants_click(&s_hub));
    double_tap();
    check();
    back_to_menu();

    /* Records survive a save and reload of the page. */
    unsigned saves_before = saves;
    for (unsigned i = 0; i < 200; i++) advance(FRAME_MS);
    assert(saves > saves_before);
    pa_record_t kept = persisted;
    pocket_arcade_exit();
    pocket_arcade_enter(true);
    assert(memcmp(&s_hub.record, &kept, sizeof(kept)) == 0);
    check();

    /* Dimming, blackout and wake-only on the first key. */
    advance(FRAME_MS);
    assert(backlight == 100);
    s_last_activity -= 61000;
    advance(FRAME_MS);
    assert(backlight == 20);
    s_last_activity -= 181000;
    advance(FRAME_MS);
    assert(backlight == 0);
    unsigned cursor = s_hub.cursor;
    tap(BSP_BTN_DOWN);
    assert(backlight == 100 && s_hub.cursor == cursor);

    /* A board with no working buttons still draws, and says so. */
    pocket_arcade_exit();
    pocket_arcade_enter(false);
    check();
    assert(s_hub.page == PA_PAGE_MENU);
    snap("no-buttons");
    post(BSP_BTN_OK, BSP_BTN_PRESS);
    advance(FRAME_MS);
    assert(s_hub.page == PA_PAGE_MENU);

    /* No leaks or dangling pointers across repeated visits. */
    for (unsigned i = 0; i < 30; i++) {
        pocket_arcade_exit();
        pocket_arcade_enter(true);
        advance(FRAME_MS);
        check();
    }
    pocket_arcade_exit();
    printf("pocket arcade page: PASS (%d games captured)\n", PA_GAME_COUNT);
    return 0;
}
