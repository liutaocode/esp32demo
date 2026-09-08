/* Host regression for Pocket Arcade: the hub, the records and all twelve games.
   Nothing here touches ESP-IDF or LVGL, so the whole product path is covered. */
#include "pa_hub.h"
#include "pa_picture.h"
#include "pa_quiz.h"
#include "pa_verse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Lobby positions, same order as REGISTRY in pa_hub.c. These are display
   positions, not record ids: the two are deliberately independent. */
enum {
    G_MOLE = 0, G_TILES, G_BIRD, G_JUMP, G_REACT, G_DINO, G_SNAKE, G_FROG,
    G_BRICK, G_PONG, G_INVADER, G_BOWL, G_SLING, G_SIMON, G_PAIRS, G_MATHQ,
    G_TETRIS, G_MERGE, G_SLIDE, G_HANOI, G_LIGHTS, G_PEG, G_SOKOBAN, G_MINES,
    G_NONO, G_GUESS, G_POEM, G_QUIZ, G_FOUR, G_GOMOKU, G_REVERSI, G_CARDS
};

static void start(pa_run_t *run, unsigned game, uint32_t seed)
{
    memset(run, 0, sizeof(*run));
    run->rng = seed ? seed : 1U;
    pa_game_at(game)->reset(run);
}

static void tick(pa_run_t *run, unsigned game, uint32_t ms)
{
    pa_game_at(game)->tick(run, ms);
}

static void press(pa_run_t *run, unsigned game, pa_key_t key)
{
    pa_game_at(game)->key(run, key);
}

/* Every frame the renderer sees must fit the pools and stay inside the panel. */
static void check_scene(const pa_scene_t *scene)
{
    assert(scene->rects <= PA_MAX_RECTS);
    assert(scene->texts <= PA_MAX_TEXTS);
    for (unsigned i = 0; i < scene->rects; i++) {
        const pa_rect_t *r = &scene->rect[i];
        assert(r->w > 0 && r->h > 0);
        assert(r->x >= 0 && r->y >= 0);
        assert(r->x + r->w <= PA_VIEW_W);
        assert(r->y + r->h <= PA_VIEW_H);
    }
    for (unsigned i = 0; i < scene->texts; i++) {
        const pa_text_t *t = &scene->text[i];
        assert(t->w > 0 && t->x >= 0 && t->y >= 0);
        assert(t->x + t->w <= PA_VIEW_W);
        assert(t->y + 20 <= PA_VIEW_H);
        assert(t->font <= PA_FONT_NUM20);
        assert(t->align <= PA_RIGHT);
        assert(t->text[0] != '\0');
        assert(strlen(t->text) < PA_TEXT_MAX);
    }
    assert(strlen(scene->footer) < PA_FOOTER_MAX);
}

static void draw_and_check(const pa_run_t *run, unsigned game)
{
    static pa_scene_t scene;
    pa_scene_reset(&scene);
    pa_game_at(game)->draw(run, &scene);
    check_scene(&scene);
}

/* ---- scene helpers ---- */
static void test_scene(void)
{
    static pa_scene_t scene;
    pa_scene_reset(&scene);
    pa_rect(&scene, -20, -20, 10, 10, PA_INK, 0);          /* fully outside */
    assert(scene.rects == 0);
    pa_rect(&scene, -4, -4, 20, 20, PA_INK, 0);            /* clipped to the corner */
    assert(scene.rects == 1 && scene.rect[0].x == 0 && scene.rect[0].w == 16);
    pa_frect(&scene, 0, 0, 400, 400, PA_INK, 0);
    assert(scene.rect[1].x == PA_FIELD_X && scene.rect[1].y == PA_FIELD_Y);
    assert(scene.rect[1].w == PA_FIELD_W && scene.rect[1].h == PA_FIELD_H);
    pa_text(&scene, 0, 0, 100, "", PA_INK, PA_FONT_ZH, PA_LEFT);
    assert(scene.texts == 0);                              /* empty text is dropped */
    pa_textf(&scene, 0, 0, 100, PA_INK, PA_FONT_ZH, PA_LEFT, "分 %d", 42);
    assert(scene.texts == 1 && strcmp(scene.text[0].text, "分 42") == 0);
    for (unsigned i = 0; i < PA_MAX_RECTS + 20; i++)
        pa_rect(&scene, 0, 0, 4, 4, PA_INK, 0);
    assert(scene.rects == PA_MAX_RECTS);
    check_scene(&scene);

    uint32_t state = 7;
    for (unsigned i = 0; i < 5000; i++) assert(pa_below(&state, 13) < 13);
    assert(pa_below(&state, 0) == 0);
}

/* ---- records ---- */
static void test_records(void)
{
    pa_record_t saved = {.plays = 1234};
    for (unsigned i = 0; i < PA_GAME_COUNT; i++) saved.best[i] = (uint16_t)(i * 700 + 5);
    uint8_t bytes[PA_SAVE_SIZE];
    pa_record_encode(&saved, bytes);
    pa_record_t loaded;
    memset(&loaded, 0, sizeof(loaded));
    assert(pa_record_decode(&loaded, bytes));
    assert(memcmp(&saved, &loaded, sizeof(saved)) == 0);
    bytes[0] = 'Z';
    assert(!pa_record_decode(&loaded, bytes));
    bytes[0] = 'A';
    bytes[1] = 9;
    assert(!pa_record_decode(&loaded, bytes));
}

/* ---- the three-key convention ---- */
static void test_key_rows(void)
{
    /* The buttons sit on the device in this order, and bsp_button.h numbers
       them the same way. A game that lays three rows against three keys must
       follow it, or the second row on screen answers to the third key. */
    assert(pa_key_of_row(0) == PA_KEY_UP);
    assert(pa_key_of_row(1) == PA_KEY_DOWN);
    assert(pa_key_of_row(2) == PA_KEY_OK);
    for (unsigned row = 0; row < PA_KEY_ROWS; row++) {
        assert(pa_row_of_key(pa_key_of_row(row)) == row);
        assert(PA_KEY_LABEL[row] && PA_KEY_LABEL[row][0]);
        for (unsigned other = 0; other < row; other++)
            assert(strcmp(PA_KEY_LABEL[row], PA_KEY_LABEL[other]) != 0);
    }
    /* A double click still belongs to the confirm key's row. */
    assert(pa_row_of_key(PA_KEY_OK2) == 2);
}

/* ---- catalogue ---- */
static void test_catalogue(void)
{
    for (unsigned i = 0; i < PA_GAME_COUNT; i++) {
        const pa_game_t *game = pa_game_at(i);
        assert(game->name && game->genre && game->hint && game->keys);
        assert(game->reset && game->tick && game->key && game->draw);
        for (unsigned r = 0; r < 3; r++) assert(game->rule[r] && game->rule[r][0]);
        assert(game->star[0] < game->star[1] && game->star[1] < game->star[2]);
        assert(pa_game_stars(i, 0) == 0);
        assert(pa_game_stars(i, game->star[0]) == 1);
        assert(pa_game_stars(i, game->star[2]) == 3);
        /* Names must be distinct: the menu is the only place to tell them apart. */
        for (unsigned j = 0; j < i; j++)
            assert(strcmp(game->name, pa_game_at(j)->name) != 0);
        /* Record ids are permanent and independent of the lobby order, so they
           have to stay unique and inside the saved array. */
        assert(game->id < PA_GAME_COUNT);
        for (unsigned j = 0; j < i; j++) assert(game->id != pa_game_at(j)->id);
    }
    assert(pa_game_at(PA_GAME_COUNT) == pa_game_at(0));

    /* The lobby opens on the games that map a key straight to a thing on the
       screen; that is what teaches the whole application in one press. */
    assert(pa_game_at(0) == &pa_game_mole && pa_game_at(1) == &pa_game_tiles);
    /* And nothing at the top may end before a person can react: run each of the
       first four with no input at all and check it survives a full second. */
    for (unsigned index = 0; index < 4; index++) {
        pa_run_t run;
        start(&run, index, index * 131U + 7U);
        for (unsigned frame = 0; frame < 40; frame++) tick(&run, index, 25);
        assert(!run.over);
    }
}

/* ---- hub navigation ---- */
static void test_hub(void)
{
    pa_hub_t hub;
    pa_record_t record;
    memset(&record, 0, sizeof(record));
    pa_hub_init(&hub, &record, 99);
    assert(hub.page == PA_PAGE_MENU);

    /* The guard window swallows keys right after a page change. */
    pa_hub_key(&hub, PA_KEY_DOWN);
    assert(hub.cursor == 0);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_DOWN);
    assert(hub.cursor == 1);
    pa_hub_key(&hub, PA_KEY_UP);
    pa_hub_key(&hub, PA_KEY_UP);
    assert(hub.cursor == PA_GAME_COUNT - 1);       /* wraps backwards */
    assert(hub.top == PA_GAME_COUNT - PA_MENU_ROWS);
    pa_hub_key(&hub, PA_KEY_DOWN);
    assert(hub.cursor == 0 && hub.top == 0);

    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.page == PA_PAGE_BRIEF && hub.game == 0);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_UP);
    assert(hub.page == PA_PAGE_MENU);              /* up or down leaves the brief */
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.page == PA_PAGE_PLAY);

    /* Leaving a run early still books the score and the play count. */
    hub.run.score = 260;
    pa_hub_long(&hub, PA_KEY_OK);
    assert(hub.page == PA_PAGE_MENU);
    assert(hub.record.best[pa_game_at(0)->id] == 260 && hub.record.plays == 1);
    assert(hub.record_dirty);
    assert(pa_hub_stars(&hub) == pa_game_stars(0, 260));

    /* Records follow the game, not its place in the list: the slot a run books
       is the one its permanent id names, and no other slot moved. */
    for (unsigned i = 1; i < PA_GAME_COUNT; i++)
        assert(hub.record.best[pa_game_at(i)->id] == 0);

    /* A worse run does not overwrite the record. */
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    hub.run.score = 10;
    pa_hub_long(&hub, PA_KEY_OK);
    assert(hub.record.best[pa_game_at(0)->id] == 260 && hub.record.plays == 2);
    assert(!hub.new_best);
}

/* Turn-based games take the confirm key as a click, and a double click must not
   also fire a single one. */
static void test_click_buffer(void)
{
    pa_hub_t hub;
    pa_hub_init(&hub, NULL, 5);
    hub.cursor = G_MERGE;
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.page == PA_PAGE_PLAY);
    assert(pa_hub_wants_click(&hub));
    pa_hub_tick(&hub, PA_GUARD_MS);

    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.click_pending);
    pa_hub_key(&hub, PA_KEY_OK2);
    assert(!hub.click_pending);                    /* the pending click is cancelled */
    pa_hub_tick(&hub, PA_CLICK_WAIT_MS);

    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.click_pending);
    pa_hub_tick(&hub, PA_CLICK_WAIT_MS);
    assert(!hub.click_pending);                    /* fires on its own when alone */

    /* Action games keep the press edge, with no waiting at all. */
    pa_hub_long(&hub, PA_KEY_OK);
    hub.cursor = G_BIRD;
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.page == PA_PAGE_PLAY && !pa_hub_wants_click(&hub));
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    assert(!hub.click_pending);
}

/* A finished run moves to the result page by itself, and confirm replays it. */
static void test_run_lifecycle(void)
{
    pa_hub_t hub;
    pa_hub_init(&hub, NULL, 4242);
    hub.cursor = G_BIRD;
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.page == PA_PAGE_PLAY);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);                   /* take off, then never flap */
    for (unsigned i = 0; i < 400 && hub.page == PA_PAGE_PLAY; i++)
        pa_hub_tick(&hub, 33);
    assert(hub.page == PA_PAGE_OVER);
    assert(hub.run.note && hub.run.note[0]);
    assert(hub.record.plays == 1);
    pa_hub_tick(&hub, PA_GUARD_MS);
    pa_hub_key(&hub, PA_KEY_OK);
    assert(hub.page == PA_PAGE_PLAY && !hub.run.over);

    static pa_scene_t scene;
    for (pa_page_t page = PA_PAGE_MENU; page <= PA_PAGE_OVER; page++) {
        hub.page = page;
        pa_hub_draw(&hub, &scene);
        check_scene(&scene);
        assert(scene.footer[0]);
    }
}

/* ---- 1 snake ---- */
static void test_snake(void)
{
    pa_run_t run;
    start(&run, G_SNAKE, 11);
    pa_snake_t *s = &run.u.snake;
    assert(s->length == 4 && s->dir == 0);
    uint16_t step = s->step_ms;

    /* Left then right inside one beat must not fold the snake back on itself. */
    press(&run, G_SNAKE, PA_KEY_UP);
    press(&run, G_SNAKE, PA_KEY_DOWN);
    assert(s->dir == 3);
    s->dir = 0;
    s->turned = 0;

    /* Walk the head onto the food and confirm the snake grows and speeds up. */
    s->food_x = (uint8_t)(s->x[0] + 1);
    s->food_y = s->y[0];
    s->food_gold = 0;
    tick(&run, G_SNAKE, step);
    assert(s->length == 5 && run.score == 10 && s->step_ms < step);

    /* Confirm steals a step without waiting for the beat. */
    uint8_t before = s->x[0];
    press(&run, G_SNAKE, PA_KEY_OK);
    tick(&run, G_SNAKE, 1);
    assert(s->x[0] == before + 1);

    /* Running into the wall ends the run. */
    for (unsigned i = 0; i < PA_SNAKE_COLS + 2 && !run.over; i++) tick(&run, G_SNAKE, step);
    assert(run.over && run.note);

    /* A coil tight enough to bite its own neck ends the run, but a square that
       the tail is about to vacate is safe. */
    static const uint8_t COIL_X[5] = {5, 5, 6, 6, 6};
    static const uint8_t COIL_Y[5] = {4, 3, 3, 4, 5};
    start(&run, G_SNAKE, 3);
    memcpy(s->x, COIL_X, sizeof(COIL_X));
    memcpy(s->y, COIL_Y, sizeof(COIL_Y));
    s->length = 4;                       /* (6,4) is the tail: it moves away */
    s->dir = 0;
    s->food_x = 0; s->food_y = 0;
    tick(&run, G_SNAKE, s->step_ms);
    assert(!run.over && s->x[0] == 6 && s->y[0] == 4);

    start(&run, G_SNAKE, 3);
    memcpy(s->x, COIL_X, sizeof(COIL_X));
    memcpy(s->y, COIL_Y, sizeof(COIL_Y));
    s->length = 5;                       /* now (6,4) is body, not tail */
    s->dir = 0;
    s->food_x = 0; s->food_y = 0;
    tick(&run, G_SNAKE, s->step_ms);
    assert(run.over && run.note);
}

/* ---- 2 tetris ---- */
static void test_tetris(void)
{
    pa_run_t run;
    start(&run, G_TETRIS, 21);
    pa_tetris_t *t = &run.u.tetris;
    assert(t->fall_ms > 0 && !run.over);

    /* Moves stop at the wall instead of leaving the well. */
    for (unsigned i = 0; i < 12; i++) press(&run, G_TETRIS, PA_KEY_UP);
    assert(t->px >= -1);
    for (unsigned i = 0; i < 24; i++) press(&run, G_TETRIS, PA_KEY_DOWN);
    assert(t->px < PA_TET_COLS);

    /* A full row clears, everything above drops and the score jumps. */
    start(&run, G_TETRIS, 21);
    for (unsigned col = 0; col < PA_TET_COLS; col++)
        t->cell[PA_TET_ROWS - 1][col] = 3;
    t->cell[PA_TET_ROWS - 2][0] = 5;
    long before = run.score;
    t->py = PA_TET_ROWS - 4;
    for (unsigned i = 0; i < 200 && t->lines == 0 && !run.over; i++)
        tick(&run, G_TETRIS, t->fall_ms);
    assert(t->lines >= 1);
    assert(run.score > before);
    /* The marker that sat above the cleared row has dropped into it. */
    assert(t->cell[PA_TET_ROWS - 1][0] == 5);

    /* A stack that reaches the top ends the run. */
    start(&run, G_TETRIS, 7);
    for (unsigned row = 0; row < PA_TET_ROWS; row++)
        for (unsigned col = 0; col < PA_TET_COLS; col++)
            t->cell[row][col] = (col == 7) ? 0 : 4;
    for (unsigned i = 0; i < 400 && !run.over; i++) tick(&run, G_TETRIS, t->fall_ms);
    assert(run.over);
}

/* ---- 3 merge ---- */
static void test_merge(void)
{
    pa_run_t run;
    start(&run, G_MERGE, 5);
    pa_merge_t *m = &run.u.merge;
    unsigned filled = 0;
    for (unsigned y = 0; y < 4; y++)
        for (unsigned x = 0; x < 4; x++) filled += m->cell[y][x] ? 1U : 0U;
    assert(filled == 2);

    memset(m->cell, 0, sizeof(m->cell));
    run.score = 0;
    m->cell[0][0] = 1; m->cell[0][1] = 1; m->cell[0][2] = 2; m->cell[0][3] = 2;
    press(&run, G_MERGE, PA_KEY_OK);                 /* slide left */
    assert(m->cell[0][0] == 2 && m->cell[0][1] == 3);
    assert(run.score == 4 + 8);
    assert(m->top == 3);

    /* Three in a row merges only the closest pair. */
    memset(m->cell, 0, sizeof(m->cell));
    m->cell[1][0] = 1; m->cell[1][1] = 1; m->cell[1][2] = 1;
    press(&run, G_MERGE, PA_KEY_OK);
    assert(m->cell[1][0] == 2 && m->cell[1][1] == 1 && m->cell[1][2] == 0);

    /* Right, up and down all move. */
    memset(m->cell, 0, sizeof(m->cell));
    m->cell[2][0] = 4;
    press(&run, G_MERGE, PA_KEY_OK2);
    assert(m->cell[2][3] == 4);
    memset(m->cell, 0, sizeof(m->cell));
    m->cell[3][1] = 4;
    press(&run, G_MERGE, PA_KEY_UP);
    assert(m->cell[0][1] == 4);
    memset(m->cell, 0, sizeof(m->cell));
    m->cell[0][2] = 4;
    press(&run, G_MERGE, PA_KEY_DOWN);
    assert(m->cell[3][2] == 4);

    /* A move that changes nothing must not spawn a tile. */
    memset(m->cell, 0, sizeof(m->cell));
    m->cell[0][0] = 3;
    unsigned moves = m->moves;
    press(&run, G_MERGE, PA_KEY_UP);
    assert(m->moves == moves);

    /* One slide fills the last hole and leaves a board with no equal
       neighbours anywhere, which is the end of the run. */
    static const uint8_t LOCKED[4][4] = {
        {4, 3, 4, 0}, {4, 3, 4, 3}, {3, 4, 3, 4}, {4, 3, 4, 3}
    };
    memcpy(m->cell, LOCKED, sizeof(LOCKED));
    run.over = false;
    press(&run, G_MERGE, PA_KEY_OK2);          /* slide right */
    assert(m->cell[0][0] == 1 || m->cell[0][0] == 2);
    assert(run.over && run.note);
}

/* ---- 4 bird ---- */
static void test_bird(void)
{
    pa_run_t run;
    start(&run, G_BIRD, 13);
    pa_bird_t *b = &run.u.bird;
    assert(!b->flying);

    /* Before the first press the bird only bobs: no gravity, no pipes moving,
       and nothing that can kill it. The page guard alone is longer than the
       fall used to take, so without this the game was unplayable. */
    int32_t parked = b->pipe_x_m[0];
    for (unsigned i = 0; i < 400; i++) {
        tick(&run, G_BIRD, 25);
        assert(!run.over);
        int y = (int)(b->y_m / 1000);
        assert(y > 50 && y < 90);
    }
    assert(b->pipe_x_m[0] == parked);

    press(&run, G_BIRD, PA_KEY_OK);
    assert(b->flying && b->vy < 0);
    int32_t start_y = b->y_m;
    tick(&run, G_BIRD, 33);
    assert(b->y_m < start_y);                         /* the flap really lifts */

    /* Once flying, no further input reaches the ground and stops the run. */
    for (unsigned i = 0; i < 400 && !run.over; i++) tick(&run, G_BIRD, 33);
    assert(run.over && run.note);

    /* An autopilot that aims at the next gap keeps scoring, so the pipes
       recycle correctly and the gaps stay reachable. */
    start(&run, G_BIRD, 31);
    press(&run, G_BIRD, PA_KEY_OK);
    for (unsigned i = 0; i < 6000 && !run.over && run.score < 12; i++) {
        int nearest = -1;
        for (unsigned pipe = 0; pipe < PA_BIRD_PIPES; pipe++) {
            if (b->pipe_x_m[pipe] < 20000) continue;      /* already behind the bird */
            if (nearest < 0 || b->pipe_x_m[pipe] < b->pipe_x_m[nearest]) nearest = (int)pipe;
        }
        if (nearest < 0) nearest = 0;
        int aim = b->gap_y[nearest] + PA_BIRD_GAP - 20;
        if ((int)(b->y_m / 1000) > aim && b->vy >= 0) press(&run, G_BIRD, PA_KEY_OK);
        tick(&run, G_BIRD, 20);
        draw_and_check(&run, G_BIRD);
    }
    assert(run.score >= 12);
}

/* ---- 5 dino ---- */
static void test_dino(void)
{
    pa_run_t run;
    start(&run, G_DINO, 17);
    pa_dino_t *d = &run.u.dino;
    press(&run, G_DINO, PA_KEY_OK);
    assert(d->y_m < 0);
    int32_t airborne = d->y_m;
    press(&run, G_DINO, PA_KEY_OK);
    assert(d->y_m == airborne);                       /* no double jump */
    press(&run, G_DINO, PA_KEY_DOWN);
    assert(d->duck_ms > 0);

    /* Standing still in front of an obstacle always ends in a crash. */
    start(&run, G_DINO, 17);
    for (unsigned i = 0; i < 600 && !run.over; i++) tick(&run, G_DINO, 33);
    assert(run.over && run.note);
    assert(run.score > 0);
}

/* ---- 6 tiles ---- */
static void test_tiles(void)
{
    pa_run_t run;
    start(&run, G_TILES, 23);
    pa_tiles_t *t = &run.u.tiles;
    assert(t->lives == 3);

    /* Pressing an empty lane breaks the combo but costs no life. */
    t->combo = 5;
    press(&run, G_TILES, PA_KEY_UP);
    assert(t->combo == 0 && t->lives == 3);

    /* A tile parked in the judgement box scores when the key that sits in that
       row is pressed, and only that key. */
    for (unsigned row = 0; row < PA_TILE_ROWS; row++) {
        start(&run, G_TILES, 23);
        memset(t->live, 0, sizeof(t->live));
        t->x[0] = (int32_t)(PA_TILE_HIT_X + 2) * 1000;
        t->row[0] = (uint8_t)row;
        t->kind[0] = 0;
        t->live[0] = 1;
        press(&run, G_TILES, pa_key_of_row((row + 1U) % PA_TILE_ROWS));
        assert(t->live[0]);                           /* the wrong key misses */
        press(&run, G_TILES, pa_key_of_row(row));
        assert(!t->live[0] && run.score >= 10);
    }

    /* Three missed tiles end the run. */
    start(&run, G_TILES, 23);
    for (unsigned i = 0; i < 3000 && !run.over; i++) {
        tick(&run, G_TILES, 25);
        draw_and_check(&run, G_TILES);
    }
    assert(run.over && run.note);
}

/* ---- 7 mole ---- */
static void test_mole(void)
{
    pa_run_t run;
    start(&run, G_MOLE, 29);
    pa_mole_t *m = &run.u.mole;
    /* Hole n on screen belongs to key n in the hand, top to bottom. */
    for (unsigned hole = 0; hole < 3; hole++) {
        start(&run, G_MOLE, 29);
        m->kind[hole] = 2;                             /* golden */
        m->show_ms[hole] = m->live_ms[hole] = 900;
        press(&run, G_MOLE, pa_key_of_row((hole + 1U) % 3U));
        assert(m->kind[hole] == 2 && run.score == 0);  /* the wrong key misses */
        press(&run, G_MOLE, pa_key_of_row(hole));
        assert(run.score == 30 && m->hits == 1 && m->kind[hole] == 0);
    }

    start(&run, G_MOLE, 29);
    m->kind[2] = 3;                                    /* bomb */
    m->show_ms[2] = m->live_ms[2] = 900;
    run.score = 30;
    press(&run, G_MOLE, pa_key_of_row(2));
    assert(run.score == 10 && m->combo == 0);

    /* The score never goes below zero. */
    run.score = 5;
    m->kind[1] = 3;
    m->show_ms[1] = m->live_ms[1] = 900;
    press(&run, G_MOLE, pa_key_of_row(1));
    assert(run.score == 0);

    /* The round is exactly sixty seconds long. */
    start(&run, G_MOLE, 29);
    unsigned elapsed = 0;
    while (!run.over && elapsed < 90000) {
        tick(&run, G_MOLE, 50);
        draw_and_check(&run, G_MOLE);
        elapsed += 50;
    }
    assert(run.over && elapsed >= 59000 && elapsed <= 61000);
}

/* ---- 8 invader ---- */
static void test_invader(void)
{
    pa_run_t run;
    start(&run, G_INVADER, 37);
    pa_invader_t *v = &run.u.invader;
    assert(v->lives == 3 && v->wave == 1 && v->shot_col < 0);

    press(&run, G_INVADER, PA_KEY_UP);
    assert(v->ship_col == 1);
    for (unsigned i = 0; i < 9; i++) press(&run, G_INVADER, PA_KEY_UP);
    assert(v->ship_col == 0);
    for (unsigned i = 0; i < 9; i++) press(&run, G_INVADER, PA_KEY_DOWN);
    assert(v->ship_col == PA_INV_COLS - 1);

    /* One shot at a time, and it takes the closest alien in that column. */
    v->ship_col = 2;
    press(&run, G_INVADER, PA_KEY_OK);
    assert(v->shot_col == 2);
    press(&run, G_INVADER, PA_KEY_OK);
    assert(v->shot_col == 2);
    for (unsigned i = 0; i < 40 && v->shot_col >= 0; i++) tick(&run, G_INVADER, 20);
    assert(!v->alive[PA_INV_ROWS - 1][2] && run.score > 0);

    /* Clearing every alien starts a faster wave and pays a bonus. */
    start(&run, G_INVADER, 41);
    memset(v->alive, 0, sizeof(v->alive));
    v->alive[0][0] = 1;
    v->ship_col = 0;
    press(&run, G_INVADER, PA_KEY_OK);
    for (unsigned i = 0; i < 60 && v->wave == 1; i++) tick(&run, G_INVADER, 20);
    assert(v->wave == 2 && run.score >= 200);

    /* Left alone, the fleet reaches the ground. */
    start(&run, G_INVADER, 43);
    for (unsigned i = 0; i < 6000 && !run.over; i++) tick(&run, G_INVADER, 25);
    assert(run.over && run.note);
}

/* ---- 9 jump ---- */
/* The power that puts the landing exactly on the middle of the next platform. */
static int centre_power(const pa_jump_t *j)
{
    int here = PA_JUMP_HOME_X + j->here_w / 2;
    int there = j->next_x + j->next_w / 2;
    return there - here - 34;
}

static void test_jump(void)
{
    pa_run_t run;
    start(&run, G_JUMP, 53);
    pa_jump_t *j = &run.u.jump;
    for (unsigned i = 0; i < 400; i++) {
        tick(&run, G_JUMP, 20);
        assert(j->power <= 100);
    }
    /* The bar really does sweep both ways. */
    unsigned low = 200, high = 0;
    for (unsigned i = 0; i < 400; i++) {
        tick(&run, G_JUMP, 20);
        if (j->power < low) low = j->power;
        if (j->power > high) high = j->power;
    }
    assert(low == 0 && high == 100);

    /* Releasing at the marked power lands dead centre. */
    start(&run, G_JUMP, 53);
    int power = centre_power(j);
    assert(power >= 0 && power <= 100);
    j->power = (uint16_t)power;
    press(&run, G_JUMP, PA_KEY_OK);
    for (unsigned i = 0; i < 60 && j->phase != 0 && !run.over; i++) tick(&run, G_JUMP, 20);
    assert(!run.over && run.score >= 30 && j->combo == 1);

    /* Releasing at zero power always falls short and ends the run. */
    start(&run, G_JUMP, 59);
    j->power = 0;
    press(&run, G_JUMP, PA_KEY_OK);
    for (unsigned i = 0; i < 120 && !run.over; i++) {
        tick(&run, G_JUMP, 20);
        draw_and_check(&run, G_JUMP);
    }
    assert(run.over && run.note);
}

/* ---- 10 sokoban ---- */
static void test_sokoban(void)
{
    /* Every level is well formed: one player, matching boxes and goals, walled in. */
    for (unsigned level = 0; level < PA_SOK_LEVELS; level++) {
        unsigned boxes = 0, goals = 0, players = 0;
        for (unsigned y = 0; y < PA_SOK_H; y++) {
            assert(strlen(PA_SOK_LEVEL[level][y]) == PA_SOK_W);
            for (unsigned x = 0; x < PA_SOK_W; x++) {
                char c = PA_SOK_LEVEL[level][y][x];
                assert(strchr("# .$*@+", c) != NULL);
                if (c == '$' || c == '*') boxes++;
                if (c == '.' || c == '*' || c == '+') goals++;
                if (c == '@' || c == '+') players++;
                bool border = (x == 0 || y == 0 || x == PA_SOK_W - 1 || y == PA_SOK_H - 1);
                if (border) assert(c == '#');
            }
        }
        assert(players == 1);
        assert(boxes == goals && boxes > 0);
    }

    pa_run_t run;
    start(&run, G_SOKOBAN, 61);
    pa_sokoban_t *s = &run.u.sokoban;
    assert(s->level == 0);
    assert(s->cell[s->py][s->px] == 0 || (s->cell[s->py][s->px] & PA_SOK_GOAL));

    /* Confirm walks in the arrow direction; turning costs no move. */
    press(&run, G_SOKOBAN, PA_KEY_UP);
    assert(s->dir == 3 && s->moves == 0);
    press(&run, G_SOKOBAN, PA_KEY_DOWN);
    assert(s->dir == 0);

    /* Pushing the box and then undoing puts the world back exactly. */
    pa_sokoban_t before = *s;
    unsigned box_x = s->px + 2U;
    press(&run, G_SOKOBAN, PA_KEY_OK);
    assert(s->moves == 1 && s->px == before.px + 1U);
    assert(s->cell[s->py][box_x] & PA_SOK_BOX);
    press(&run, G_SOKOBAN, PA_KEY_OK2);
    assert(s->px == before.px && s->py == before.py && s->moves == 0);
    assert(memcmp(s->cell, before.cell, sizeof(s->cell)) == 0);

    /* Finishing level one scores and loads level two. */
    press(&run, G_SOKOBAN, PA_KEY_OK);
    press(&run, G_SOKOBAN, PA_KEY_OK);
    assert(s->clear_ms > 0 && run.score >= 100 && s->cleared == 1);
    for (unsigned i = 0; i < 100 && s->level == 0; i++) tick(&run, G_SOKOBAN, 33);
    assert(s->level == 1 && s->moves == 0 && s->undos == 0);

    /* Walking into a wall does nothing at all. */
    start(&run, G_SOKOBAN, 61);
    s->px = 1; s->py = 1; s->dir = 2;
    press(&run, G_SOKOBAN, PA_KEY_OK);
    assert(s->px == 1 && s->moves == 0);

    /* The undo stack never desynchronises once it is full. */
    start(&run, G_SOKOBAN, 61);
    s->dir = 1;
    for (unsigned i = 0; i < PA_SOK_UNDO + 20; i++) {
        s->dir = (uint8_t)((i / 2) % 4);
        press(&run, G_SOKOBAN, PA_KEY_OK);
    }
    assert(s->undos <= PA_SOK_UNDO);
    /* If that wandering happened to finish the level, let it load the next one
       first: undo is deliberately blocked while the clear banner is up. */
    for (unsigned i = 0; i < 200 && s->clear_ms; i++) tick(&run, G_SOKOBAN, 33);
    for (unsigned i = 0; i < PA_SOK_UNDO + 20; i++) press(&run, G_SOKOBAN, PA_KEY_OK2);
    assert(s->undos == 0);
    assert(s->px < PA_SOK_W && s->py < PA_SOK_H);
    assert(!(s->cell[s->py][s->px] & (PA_SOK_WALL | PA_SOK_BOX)));
}

/* ---- 11 cards ---- */
static void test_cards(void)
{
    pa_run_t run;
    start(&run, G_CARDS, 71);
    pa_cards_t *c = &run.u.cards;
    assert(c->chips == 100 && c->phase == 0 && run.score == 100);

    press(&run, G_CARDS, PA_KEY_UP);
    assert(c->bet == 20);
    press(&run, G_CARDS, PA_KEY_UP);
    assert(c->bet == 50);
    press(&run, G_CARDS, PA_KEY_UP);
    assert(c->bet == 10);
    press(&run, G_CARDS, PA_KEY_DOWN);
    assert(c->bet == 50);

    /* Busting loses the stake immediately. */
    c->bet = 10;
    press(&run, G_CARDS, PA_KEY_OK);
    assert(c->phase == 1 || c->phase == 2);
    for (unsigned i = 0; i < 12 && c->phase == 1; i++) press(&run, G_CARDS, PA_KEY_OK);
    assert(c->phase == 2);
    assert(c->chips <= 115 && run.score == c->chips);

    /* Twelve hands is the whole game. */
    start(&run, G_CARDS, 73);
    unsigned guard = 0;
    while (!run.over && guard++ < 2000) {
        if (c->phase == 0) press(&run, G_CARDS, PA_KEY_OK);
        else if (c->phase == 1) press(&run, G_CARDS, PA_KEY_DOWN);
        else press(&run, G_CARDS, PA_KEY_OK);
        draw_and_check(&run, G_CARDS);
    }
    assert(run.over && run.note);
    assert(c->hand >= PA_CARD_HANDS || c->chips == 0);
}

/* ---- 12 pong ---- */
static void test_pong(void)
{
    pa_run_t run;
    start(&run, G_PONG, 83);
    pa_pong_t *p = &run.u.pong;
    assert(p->serve_ms > 0);
    int16_t start_x = p->px;
    press(&run, G_PONG, PA_KEY_UP);
    assert(p->px == start_x - PA_PONG_STEP);
    for (unsigned i = 0; i < 40; i++) press(&run, G_PONG, PA_KEY_UP);
    assert(p->px == 0);
    for (unsigned i = 0; i < 40; i++) press(&run, G_PONG, PA_KEY_DOWN);
    assert(p->px == PA_FIELD_W - PA_PONG_PAD);

    /* Confirm serves early. */
    press(&run, G_PONG, PA_KEY_OK);
    assert(p->serve_ms <= 1);

    /* A player who tracks the ball wins rallies; one who never moves loses. */
    start(&run, G_PONG, 89);
    for (unsigned i = 0; i < 8000 && !run.over; i++) {
        tick(&run, G_PONG, 25);
        draw_and_check(&run, G_PONG);
    }
    assert(run.over && p->cpu >= 7);
}

/* Random hammering must never break a game's invariants or the scene budget. */
static void test_soak(void)
{
    for (unsigned game = 0; game < PA_GAME_COUNT; game++) {
        for (uint32_t seed = 1; seed <= 6; seed++) {
            pa_run_t run;
            start(&run, game, seed * 2654435761U + 7U);
            uint32_t input = seed * 97U + 3U;
            for (unsigned frame = 0; frame < 2400 && !run.over; frame++) {
                unsigned roll = pa_below(&input, 10);
                if (roll < 3) press(&run, game, (pa_key_t)pa_below(&input, 4));
                tick(&run, game, 25);
                draw_and_check(&run, game);
                assert(run.score >= 0);
            }
        }
    }
}

/* ---- 13 four in a row ---- */
static void test_four(void)
{
    pa_run_t run;
    start(&run, G_FOUR, 101);
    pa_four_t *f = &run.u.four;
    assert(f->col == PA_FOUR_W / 2 && !f->turn);
    for (unsigned i = 0; i < PA_FOUR_W + 2; i++) press(&run, G_FOUR, PA_KEY_DOWN);
    assert(f->col < PA_FOUR_W);                       /* the column wraps */

    /* A piece lands on the lowest free row of its column. */
    f->col = 0;
    press(&run, G_FOUR, PA_KEY_OK);
    assert(f->cell[PA_FOUR_H - 1][0] == 1);
    assert(f->turn == 1);

    /* Four in a row is spotted the moment it is completed. */
    start(&run, G_FOUR, 101);
    for (unsigned col = 0; col < 3; col++) f->cell[PA_FOUR_H - 1][col] = 1;
    f->col = 3;
    press(&run, G_FOUR, PA_KEY_OK);
    assert(f->result == 1);
    long before = run.score;
    for (unsigned i = 0; i < 80 && f->result; i++) tick(&run, G_FOUR, 33);
    assert(run.score > before && f->round == 2 && !run.over);

    /* Three in a row with only one open end has to be blocked there. */
    start(&run, G_FOUR, 7);
    for (unsigned col = 0; col <= 2; col++) f->cell[PA_FOUR_H - 1][col] = 1;
    f->turn = 1;
    f->think_ms = 0;
    for (unsigned i = 0; i < 40 && f->turn; i++) tick(&run, G_FOUR, 33);
    assert(f->cell[PA_FOUR_H - 1][3] == 2);

    /* Losing ends the run. */
    start(&run, G_FOUR, 9);
    for (unsigned col = 0; col < 4; col++) f->cell[PA_FOUR_H - 1][col] = 2;
    f->result = 2;
    f->think_ms = 0;
    tick(&run, G_FOUR, 33);
    assert(run.over && run.note);
}

/* ---- 14 reversi ---- */
static void test_reversi(void)
{
    pa_run_t run;
    start(&run, G_REVERSI, 103);
    pa_reversi_t *r = &run.u.reversi;
    assert(r->cell[3][3] == 2 && r->cell[3][4] == 1);
    assert(r->count == 4);                            /* black opens with four */

    uint8_t pick = r->pick;
    press(&run, G_REVERSI, PA_KEY_DOWN);
    assert(r->pick != pick);
    press(&run, G_REVERSI, PA_KEY_UP);
    assert(r->pick == pick);

    /* A move flips at least one disc and hands over to the computer. */
    unsigned mine = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) mine += (r->cell[y][x] == 1) ? 1U : 0U;
    press(&run, G_REVERSI, PA_KEY_OK);
    unsigned after = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) after += (r->cell[y][x] == 1) ? 1U : 0U;
    assert(after >= mine + 2);
    assert(r->turn == 1);
    for (unsigned i = 0; i < 60 && r->turn; i++) tick(&run, G_REVERSI, 33);
    assert(!r->turn && r->count > 0);

    /* A full game always terminates and either scores or ends the run. */
    start(&run, G_REVERSI, 107);
    for (unsigned i = 0; i < 4000 && !run.over && r->round == 1; i++) {
        if (!r->turn && r->count) press(&run, G_REVERSI, PA_KEY_OK);
        tick(&run, G_REVERSI, 33);
        draw_and_check(&run, G_REVERSI);
    }
    assert(run.over || r->round > 1);
}

/* ---- 15 minesweeper ---- */
static void test_mines(void)
{
    pa_run_t run;
    start(&run, G_MINES, 109);
    pa_mines_t *m = &run.u.mines;
    assert(!m->seeded && m->mines == 6);

    press(&run, G_MINES, PA_KEY_UP);
    assert(m->cy == 1 && m->cx == 0);
    press(&run, G_MINES, PA_KEY_DOWN);
    assert(m->cx == 1 && m->cy == 1);

    /* Flagging is reversible and never opens a cell. */
    press(&run, G_MINES, PA_KEY_OK2);
    assert(m->cell[1][1] & PA_MINE_FLAG);
    press(&run, G_MINES, PA_KEY_OK2);
    assert(!(m->cell[1][1] & PA_MINE_FLAG));

    /* The very first cell opened is never a mine, from any starting square. */
    for (uint32_t seed = 1; seed <= 40; seed++) {
        start(&run, G_MINES, seed * 37U + 1U);
        m->cx = (uint8_t)(seed % PA_MINE_W);
        m->cy = (uint8_t)((seed / 3U) % PA_MINE_H);
        press(&run, G_MINES, PA_KEY_OK);
        assert(!run.over);
        assert(m->seeded && m->opened > 0);
        unsigned bombs = 0;
        for (int y = 0; y < PA_MINE_H; y++)
            for (int x = 0; x < PA_MINE_W; x++)
                bombs += (m->cell[y][x] & PA_MINE_BOMB) ? 1U : 0U;
        assert(bombs == m->mines);
    }

    /* Stepping on a mine ends the run. */
    start(&run, G_MINES, 113);
    press(&run, G_MINES, PA_KEY_OK);
    for (int y = 0; y < PA_MINE_H && !run.over; y++)
        for (int x = 0; x < PA_MINE_W && !run.over; x++) {
            if (!(m->cell[y][x] & PA_MINE_BOMB)) continue;
            m->cx = (uint8_t)x;
            m->cy = (uint8_t)y;
            press(&run, G_MINES, PA_KEY_OK);
        }
    assert(run.over && m->boom);
}

/* ---- 16 sliding puzzle ---- */
static void test_slide(void)
{
    pa_run_t run;
    start(&run, G_SLIDE, 127);
    pa_slide_t *s = &run.u.slide;
    assert(s->count >= 2 && s->count <= 4);
    unsigned digits = 0;
    for (unsigned i = 0; i < 16; i++)
        for (unsigned j = 0; j < 16; j++)
            if (s->tile[j] == i) { digits++; break; }
    assert(digits == 16);                             /* every tile exactly once */

    uint8_t moved = s->option[s->pick];
    uint8_t value = s->tile[moved];
    press(&run, G_SLIDE, PA_KEY_OK);
    assert(!s->tile[moved] && s->moves == 1);
    bool landed = false;
    for (unsigned i = 0; i < 16; i++) landed |= (s->tile[i] == value);
    assert(landed);

    /* Solving three boards finishes the run and pays a move bonus. The board is
       posed one push from solved, with the option list pointed at that push. */
    start(&run, G_SLIDE, 131);
    for (unsigned board = 0; board < 3 && !run.over; board++) {
        for (uint8_t i = 0; i < 15; i++) s->tile[i] = (uint8_t)(i + 1);
        s->tile[14] = 0;
        s->tile[15] = 15;
        s->option[0] = 15;
        s->count = 1;
        s->pick = 0;
        s->moves = 0;
        long before = run.score;
        press(&run, G_SLIDE, PA_KEY_OK);
        assert(s->tile[14] == 15 && s->tile[15] == 0);
        assert(run.score > before && s->clear_ms > 0);
        for (unsigned i = 0; i < 100 && s->clear_ms; i++) tick(&run, G_SLIDE, 33);
    }
    assert(run.over && run.note);
}

/* ---- 17 hanoi ---- */
static void test_hanoi(void)
{
    pa_run_t run;
    start(&run, G_HANOI, 137);
    pa_hanoi_t *h = &run.u.hanoi;
    assert(h->disks == 3 && h->height[0] == 3 && h->peg[0][2] == 1);

    press(&run, G_HANOI, PA_KEY_OK);
    assert(h->held == 1 && h->height[0] == 2);
    press(&run, G_HANOI, PA_KEY_DOWN);
    press(&run, G_HANOI, PA_KEY_OK);
    assert(!h->held && h->height[1] == 1 && h->moves == 1);

    /* A big disc may not sit on a small one. */
    press(&run, G_HANOI, PA_KEY_UP);
    press(&run, G_HANOI, PA_KEY_OK);
    assert(h->held == 2);
    press(&run, G_HANOI, PA_KEY_DOWN);
    unsigned moves = h->moves;
    press(&run, G_HANOI, PA_KEY_OK);
    assert(h->held == 2 && h->moves == moves);

    /* Stacking everything on the right peg clears the tower. */
    start(&run, G_HANOI, 139);
    memset(h->peg, 0, sizeof(h->peg));
    memset(h->height, 0, sizeof(h->height));
    h->peg[0][0] = 1;
    h->height[0] = 1;
    h->peg[2][0] = 3;
    h->peg[2][1] = 2;
    h->height[2] = 2;
    h->cursor = 0;
    press(&run, G_HANOI, PA_KEY_OK);
    h->cursor = 2;
    press(&run, G_HANOI, PA_KEY_OK);
    assert(h->cleared == 1 && run.score > 0 && h->clear_ms > 0);
    for (unsigned i = 0; i < 100 && h->clear_ms; i++) tick(&run, G_HANOI, 33);
    assert(h->disks == 4 && h->height[0] == 4);
}

/* ---- 18 frogger ---- */
static void test_frog(void)
{
    pa_run_t run;
    start(&run, G_FROG, 149);
    pa_frog_t *f = &run.u.frog;
    assert(f->lives == 3 && f->row == 0);
    for (unsigned i = 0; i < 30; i++) press(&run, G_FROG, PA_KEY_UP);
    assert(f->fx == 0);
    for (unsigned i = 0; i < 30; i++) press(&run, G_FROG, PA_KEY_DOWN);
    assert(f->fx == PA_FIELD_W - 15);

    /* Every lane leaves at least one gap, or the crossing would be impossible. */
    for (uint32_t seed = 1; seed <= 30; seed++) {
        start(&run, G_FROG, seed * 991U + 3U);
        for (unsigned lane = 0; lane < PA_FROG_LANES; lane++) {
            assert(f->pattern[lane] != 15);
            assert(f->speed[lane] != 0);
        }
    }

    /* Hopping all the way across scores and starts a faster round. */
    start(&run, G_FROG, 151);
    for (unsigned i = 0; i < 9; i++) {                /* eight hops up, one to cross */
        f->safe_ms = 5000;                            /* ignore traffic for this check */
        press(&run, G_FROG, PA_KEY_OK);
    }
    assert(run.score >= 100 && f->round == 2 && f->row == 0);

    /* Standing in traffic costs every life. */
    start(&run, G_FROG, 157);
    f->row = 3;
    f->safe_ms = 0;
    for (unsigned i = 0; i < 4000 && !run.over; i++) {
        if (f->row == 0) f->row = 3;
        f->safe_ms = 0;
        tick(&run, G_FROG, 25);
        draw_and_check(&run, G_FROG);
    }
    assert(run.over && run.note);
}

/* ---- 19 bowling ---- */
static void test_bowl(void)
{
    pa_run_t run;
    start(&run, G_BOWL, 163);
    pa_bowl_t *b = &run.u.bowl;
    assert(b->frame == 0 && b->ball == 0 && b->phase == 0);

    press(&run, G_BOWL, PA_KEY_OK);
    assert(b->phase == 1);
    press(&run, G_BOWL, PA_KEY_OK);
    assert(b->phase == 2);
    for (unsigned i = 0; i < 200 && b->phase == 2; i++) tick(&run, G_BOWL, 33);
    assert(b->phase == 3 && b->rolled == 1);

    /* A centred, full-power ball takes the whole rack. */
    start(&run, G_BOWL, 167);
    for (unsigned frame = 0; frame < 3; frame++) {
        b->phase = 0;
        b->aim = 0;
        b->power = 100;
        press(&run, G_BOWL, PA_KEY_OK);
        b->aim = 0;
        b->power = 100;
        press(&run, G_BOWL, PA_KEY_OK);
        for (unsigned i = 0; i < 200 && b->phase == 2; i++) tick(&run, G_BOWL, 33);
        assert(b->rolls[b->rolled - 1] == 10);
        for (unsigned i = 0; i < 200 && b->phase == 3; i++) tick(&run, G_BOWL, 33);
    }
    assert(run.score >= 60);                          /* three strikes already pay */

    /* Ten frames always end, whatever the player does. */
    start(&run, G_BOWL, 173);
    for (unsigned i = 0; i < 8000 && !run.over; i++) {
        if (b->phase == 0 || b->phase == 1) press(&run, G_BOWL, PA_KEY_OK);
        tick(&run, G_BOWL, 33);
        draw_and_check(&run, G_BOWL);
    }
    assert(run.over && run.note && b->frame + 1U == 10);
}

/* ---- 20 slingshot ---- */
static void test_sling(void)
{
    pa_run_t run;
    start(&run, G_SLING, 179);
    pa_sling_t *s = &run.u.sling;
    assert(s->phase == 0 && s->target_x > 0);
    press(&run, G_SLING, PA_KEY_OK);
    assert(s->phase == 1);
    press(&run, G_SLING, PA_KEY_OK);
    assert(s->phase == 2);
    for (unsigned i = 0; i < 400 && s->phase == 2; i++) tick(&run, G_SLING, 25);
    assert(s->phase == 3);                            /* the shot always resolves */

    /* Every target the game poses must actually be reachable with some angle
       and power, wind included; otherwise the shot is a lottery. */
    for (uint32_t seed = 1; seed <= 5; seed++) {
        bool reachable = false;
        for (unsigned angle = 0; angle <= 100 && !reachable; angle += 5)
            for (unsigned power = 20; power <= 100 && !reachable; power += 5) {
                start(&run, G_SLING, seed * 811U + 5U);
                s->angle = (int16_t)angle;
                press(&run, G_SLING, PA_KEY_OK);
                s->power = (int16_t)power;
                press(&run, G_SLING, PA_KEY_OK);
                for (unsigned i = 0; i < 400 && s->phase == 2; i++) tick(&run, G_SLING, 25);
                reachable = s->hit;
            }
        assert(reachable);
    }

    /* Ten shots always end the run, whatever the player does. */
    start(&run, G_SLING, 181);
    for (unsigned i = 0; i < 20000 && !run.over; i++) {
        if (s->phase == 0 || s->phase == 1) press(&run, G_SLING, PA_KEY_OK);
        tick(&run, G_SLING, 25);
        draw_and_check(&run, G_SLING);
    }
    assert(run.over && run.note);
}

/* ---- 21 reaction ---- */
static void test_react(void)
{
    pa_run_t run;
    start(&run, G_REACT, 191);
    pa_react_t *r = &run.u.react;
    assert(r->phase == 0);
    press(&run, G_REACT, PA_KEY_OK);
    assert(r->phase == 1 && r->wait_ms >= 1100);

    /* Pressing before the light counts as a foul and pays nothing. */
    press(&run, G_REACT, PA_KEY_OK);
    assert(r->phase == 3 && r->jumped && run.score == 0);
    for (unsigned i = 0; i < 100 && r->phase == 3; i++) tick(&run, G_REACT, 33);
    assert(r->round == 1 && r->phase == 0);

    /* A fast press scores; five rounds end the run. */
    start(&run, G_REACT, 193);
    for (unsigned round = 0; round < PA_REACT_ROUNDS && !run.over; round++) {
        press(&run, G_REACT, PA_KEY_OK);
        for (unsigned i = 0; i < 400 && r->phase == 1; i++) tick(&run, G_REACT, 25);
        assert(r->phase == 2);
        press(&run, G_REACT, PA_KEY_OK);
        assert(r->phase == 3 && !r->jumped);
        for (unsigned i = 0; i < 100 && r->phase == 3; i++) tick(&run, G_REACT, 33);
        draw_and_check(&run, G_REACT);
    }
    assert(run.over && run.score > 0);
}

/* ---- 22 memory pairs ---- */
static void test_pairs(void)
{
    pa_run_t run;
    start(&run, G_PAIRS, 197);
    pa_pairs_t *p = &run.u.pairs;
    unsigned seen[8] = {0};
    for (unsigned i = 0; i < 16; i++) seen[p->face[i]]++;
    for (unsigned i = 0; i < 8; i++) assert(seen[i] == 2);

    /* Two different faces turn back over on their own. */
    int a = -1, b = -1;
    for (unsigned i = 0; i < 16 && b < 0; i++)
        for (unsigned j = i + 1; j < 16 && b < 0; j++)
            if (p->face[i] != p->face[j]) { a = (int)i; b = (int)j; }
    assert(a >= 0 && b >= 0);
    p->cy = (uint8_t)(a / 4); p->cx = (uint8_t)(a % 4);
    press(&run, G_PAIRS, PA_KEY_OK);
    p->cy = (uint8_t)(b / 4); p->cx = (uint8_t)(b % 4);
    press(&run, G_PAIRS, PA_KEY_OK);
    assert(p->flip_ms > 0);
    for (unsigned i = 0; i < 100 && p->flip_ms; i++) tick(&run, G_PAIRS, 33);
    assert(!p->state[a] && !p->state[b] && p->first < 0);

    /* A player with a perfect memory finishes all three boards. */
    start(&run, G_PAIRS, 199);
    for (unsigned guard = 0; guard < 4000 && !run.over; guard++) {
        if (p->flip_ms || p->clear_ms) { tick(&run, G_PAIRS, 33); continue; }
        int first = -1, second = -1;
        for (unsigned i = 0; i < 16 && second < 0; i++) {
            if (p->state[i] == 2) continue;
            if (first < 0) { first = (int)i; continue; }
            if (p->face[i] == p->face[first]) second = (int)i;
        }
        assert(first >= 0 && second >= 0);
        p->cy = (uint8_t)(first / 4); p->cx = (uint8_t)(first % 4);
        press(&run, G_PAIRS, PA_KEY_OK);
        p->cy = (uint8_t)(second / 4); p->cx = (uint8_t)(second % 4);
        press(&run, G_PAIRS, PA_KEY_OK);
        draw_and_check(&run, G_PAIRS);
    }
    assert(run.over && run.score > 0);
}

/* ---- 23 breakout ---- */
static unsigned remaining_bricks(const pa_brick_t *b)
{
    unsigned count = 0;
    for (unsigned row = 0; row < PA_BRICK_ROWS; row++)
        for (unsigned col = 0; col < PA_BRICK_W; col++)
            count += b->brick[row][col] ? 1U : 0U;
    return count;
}

static void test_brick(void)
{
    pa_run_t run;
    start(&run, G_BRICK, 211);
    pa_brick_t *b = &run.u.brick;
    assert(b->lives == 3 && b->level == 1);
    for (unsigned i = 0; i < 40; i++) press(&run, G_BRICK, PA_KEY_UP);
    assert(b->px == 0);
    for (unsigned i = 0; i < 40; i++) press(&run, G_BRICK, PA_KEY_DOWN);
    assert(b->px == PA_FIELD_W - 40);

    /* A brick under the ball is removed and paid for. */
    start(&run, G_BRICK, 223);
    b->serve_ms = 0;
    memset(b->brick, 0, sizeof(b->brick));
    b->brick[3][2] = 1;
    b->brick[0][0] = 1;                               /* keeps the wall from clearing */
    b->bx_m = (int32_t)(3 + 2 * 24 + 8) * 1000;
    b->by_m = (int32_t)(10 + 3 * 12 + 14) * 1000;
    b->vx = 0;
    b->vy = -120;
    long before = run.score;
    for (unsigned i = 0; i < 20 && b->brick[3][2]; i++) tick(&run, G_BRICK, 25);
    assert(!b->brick[3][2] && run.score > before && b->level == 1);

    /* An empty wall opens a faster level with a fresh one. */
    start(&run, G_BRICK, 229);
    b->serve_ms = 0;
    memset(b->brick, 0, sizeof(b->brick));
    before = run.score;
    tick(&run, G_BRICK, 25);
    assert(b->level == 2 && run.score >= before + 200);
    assert(remaining_bricks(b) == PA_BRICK_ROWS * PA_BRICK_W);

    /* A player who never moves loses every life. */
    start(&run, G_BRICK, 227);
    for (unsigned i = 0; i < 6000 && !run.over; i++) {
        if (b->serve_ms) press(&run, G_BRICK, PA_KEY_OK);
        tick(&run, G_BRICK, 25);
        draw_and_check(&run, G_BRICK);
    }
    assert(run.over && run.note);
}

/* ---- 24 bulls and cows ---- */
static void test_guess(void)
{
    pa_run_t run;
    start(&run, G_GUESS, 229);
    pa_guess_t *g = &run.u.guess;
    for (unsigned i = 0; i < PA_GUESS_LEN; i++)
        for (unsigned j = i + 1; j < PA_GUESS_LEN; j++)
            assert(g->secret[i] != g->secret[j]);     /* four different digits */

    press(&run, G_GUESS, PA_KEY_UP);
    assert(g->entry[0] == 1);
    press(&run, G_GUESS, PA_KEY_DOWN);
    assert(g->entry[0] == 0);
    press(&run, G_GUESS, PA_KEY_DOWN);
    assert(g->entry[0] == 9);                         /* wraps downwards */

    /* Two right digits in the wrong place read as cows, not bulls. */
    start(&run, G_GUESS, 233);
    g->entry[0] = g->secret[1];
    g->entry[1] = g->secret[0];
    g->entry[2] = g->secret[2];
    g->entry[3] = (uint8_t)((g->secret[3] + 1U) % 10U);
    if (g->entry[3] == g->secret[0] || g->entry[3] == g->secret[1] ||
        g->entry[3] == g->secret[2])
        g->entry[3] = (uint8_t)((g->secret[3] + 5U) % 10U);
    for (unsigned i = 0; i < PA_GUESS_LEN; i++) press(&run, G_GUESS, PA_KEY_OK);
    assert(g->tries == 1 && g->bulls[0] == 1 && g->cows[0] == 2);

    /* The exact code solves it and opens the next round. */
    memcpy(g->entry, g->secret, PA_GUESS_LEN);
    for (unsigned i = 0; i < PA_GUESS_LEN; i++) press(&run, G_GUESS, PA_KEY_OK);
    assert(g->solved && run.score > 0 && g->clear_ms > 0);
    for (unsigned i = 0; i < 100 && g->clear_ms; i++) tick(&run, G_GUESS, 33);
    assert(g->round == 2 && g->tries == 0);

    /* Eight wrong guesses end the run. */
    start(&run, G_GUESS, 239);
    for (unsigned attempt = 0; attempt < PA_GUESS_TRIES && !run.over; attempt++) {
        for (unsigned d = 0; d < PA_GUESS_LEN; d++)
            g->entry[d] = (uint8_t)((g->secret[d] + 1U + attempt) % 10U);
        for (unsigned i = 0; i < PA_GUESS_LEN; i++) press(&run, G_GUESS, PA_KEY_OK);
        draw_and_check(&run, G_GUESS);
    }
    assert(run.over && run.note);
}

/* ---- 25 lights out ---- */
static void test_lights(void)
{
    pa_run_t run;
    start(&run, G_LIGHTS, 251);
    pa_lights_t *l = &run.u.lights;
    assert(l->budget > 0 && l->moves == 0);
    bool any = false;
    for (unsigned y = 0; y < 5; y++) any |= (l->cell[y] != 0);
    assert(any);                                      /* never opens already solved */

    /* One press flips the cell and its four neighbours, no more. */
    l->cx = 2;
    l->cy = 2;
    uint8_t before[5];
    memcpy(before, l->cell, sizeof(before));
    press(&run, G_LIGHTS, PA_KEY_OK);
    unsigned changed = 0;
    for (unsigned y = 0; y < 5; y++)
        for (unsigned x = 0; x < 5; x++)
            if (((before[y] >> x) & 1U) != ((l->cell[y] >> x) & 1U)) changed++;
    assert(changed == 5);

    /* Every board it poses is solvable, because it was scrambled from dark. */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        start(&run, G_LIGHTS, seed * 613U + 11U);
        l->budget = 250;                              /* undo the step limit here */
        uint8_t scrambled[5];
        memcpy(scrambled, l->cell, sizeof(scrambled));
        /* Chase the lights: press under every lit cell row by row, then the top
           row pattern determines the rest. Instead, brute force the 32 first-row
           presses, which always contains a solution. */
        bool solved = false;
        for (unsigned mask = 0; mask < 32 && !solved; mask++) {
            memcpy(l->cell, scrambled, sizeof(scrambled));
            for (unsigned x = 0; x < 5; x++) {
                if (!((mask >> x) & 1U)) continue;
                l->cx = (uint8_t)x;
                l->cy = 0;
                press(&run, G_LIGHTS, PA_KEY_OK);
            }
            for (unsigned y = 1; y < 5; y++)
                for (unsigned x = 0; x < 5; x++) {
                    if (!((l->cell[y - 1] >> x) & 1U)) continue;
                    l->cx = (uint8_t)x;
                    l->cy = (uint8_t)y;
                    press(&run, G_LIGHTS, PA_KEY_OK);
                }
            solved = (l->cell[0] | l->cell[1] | l->cell[2] | l->cell[3] | l->cell[4]) == 0;
        }
        assert(solved);
    }

    /* Running out of moves ends the run. */
    start(&run, G_LIGHTS, 257);
    for (unsigned i = 0; i < 200 && !run.over && !l->clear_ms; i++) {
        l->cx = 0;
        l->cy = 0;
        press(&run, G_LIGHTS, PA_KEY_OK);
        press(&run, G_LIGHTS, PA_KEY_UP);
        press(&run, G_LIGHTS, PA_KEY_OK);
        press(&run, G_LIGHTS, PA_KEY_DOWN);
    }
    assert(run.over || l->clear_ms);
}

/* ---- 26 simon ---- */
static void test_simon(void)
{
    pa_run_t run;
    start(&run, G_SIMON, 263);
    pa_simon_t *s = &run.u.simon;
    assert(s->length == 3 && s->phase == 0);
    for (unsigned i = 0; i < 400 && s->phase == 0; i++) tick(&run, G_SIMON, 33);
    assert(s->phase == 1 && s->step == 0);

    /* Repeating the sequence correctly grows it by one. */
    unsigned length = s->length;
    for (unsigned i = 0; i < length; i++)
        press(&run, G_SIMON, pa_key_of_row(s->seq[i]));
    assert(s->phase == 2 && run.score > 0);
    for (unsigned i = 0; i < 100 && s->phase == 2; i++) tick(&run, G_SIMON, 33);
    assert(s->length == length + 1 && s->round == 2);

    /* One wrong pad ends the run. */
    for (unsigned i = 0; i < 400 && s->phase == 0; i++) tick(&run, G_SIMON, 33);
    assert(s->phase == 1);
    press(&run, G_SIMON, pa_key_of_row((s->seq[0] + 1U) % 3U));
    assert(s->phase == 3);
    for (unsigned i = 0; i < 100 && !run.over; i++) tick(&run, G_SIMON, 33);
    assert(run.over && run.note);
}

/* ---- 27 gomoku ---- */
static void test_gomoku(void)
{
    pa_run_t run;
    start(&run, G_GOMOKU, 269);
    pa_gomoku_t *g = &run.u.gomoku;
    assert(g->cx == PA_GOMOKU / 2 && !g->turn);
    press(&run, G_GOMOKU, PA_KEY_UP);
    assert(g->cy == PA_GOMOKU / 2 + 1);
    press(&run, G_GOMOKU, PA_KEY_DOWN);
    assert(g->cx == PA_GOMOKU / 2 + 1);

    /* An occupied point refuses a second stone. */
    press(&run, G_GOMOKU, PA_KEY_OK);
    assert(g->cell[g->cy][g->cx] == 1 && g->turn == 1);
    g->turn = 0;
    press(&run, G_GOMOKU, PA_KEY_OK);
    assert(g->cell[g->cy][g->cx] == 1);

    /* Five in a row is spotted and pays. */
    start(&run, G_GOMOKU, 271);
    for (unsigned i = 0; i < 4; i++) g->cell[4][i] = 1;
    g->cy = 4;
    g->cx = 4;
    press(&run, G_GOMOKU, PA_KEY_OK);
    assert(g->result == 1);
    long before = run.score;
    for (unsigned i = 0; i < 80 && g->result; i++) tick(&run, G_GOMOKU, 33);
    assert(run.score > before && g->round == 2);

    /* Four of the player's stones in a row must be answered. */
    start(&run, G_GOMOKU, 277);
    for (unsigned i = 2; i < 6; i++) g->cell[4][i] = 1;
    g->turn = 1;
    g->think_ms = 0;
    for (unsigned i = 0; i < 40 && g->turn; i++) tick(&run, G_GOMOKU, 33);
    assert(g->cell[4][1] == 2 || g->cell[4][6] == 2);
}

/* ---- 28 mental maths ---- */
static void test_mathq(void)
{
    pa_run_t run;
    start(&run, G_MATHQ, 281);
    pa_mathq_t *m = &run.u.mathq;

    /* Answering correctly pays, answering wrongly costs and breaks the streak. */
    for (unsigned i = 0; i < 12; i++) {
        bool truth = (m->correct != 0);
        long before = run.score;
        press(&run, G_MATHQ, truth ? PA_KEY_UP : PA_KEY_DOWN);
        assert(run.score > before && m->combo == i + 1U);
    }
    long before = run.score;
    bool truth = (m->correct != 0);
    press(&run, G_MATHQ, truth ? PA_KEY_DOWN : PA_KEY_UP);
    assert(run.score < before && m->combo == 0);

    /* Skipping costs nothing but the streak. */
    press(&run, G_MATHQ, PA_KEY_UP);
    unsigned combo = m->combo;
    long held = run.score;
    press(&run, G_MATHQ, PA_KEY_OK);
    assert(run.score == held && m->combo == 0 && combo == 1);

    /* Half the statements are false, so the answer cannot be guessed. */
    start(&run, G_MATHQ, 283);
    unsigned trues = 0;
    for (unsigned i = 0; i < 200; i++) {
        trues += m->correct ? 1U : 0U;
        press(&run, G_MATHQ, PA_KEY_OK);
    }
    assert(trues > 60 && trues < 140);

    /* The round is exactly sixty seconds. */
    start(&run, G_MATHQ, 287);
    unsigned elapsed = 0;
    while (!run.over && elapsed < 90000) {
        tick(&run, G_MATHQ, 50);
        draw_and_check(&run, G_MATHQ);
        elapsed += 50;
    }
    assert(run.over && elapsed >= 59000 && elapsed <= 61000);
}

/* ---- 29 nonogram ---- */
static unsigned line_runs(uint8_t line)
{
    unsigned count = 0, run = 0;
    for (unsigned i = 0; i < PA_NONO; i++) {
        if ((line >> i) & 1U) { run++; continue; }
        if (run) count++;
        run = 0;
    }
    return count + (run ? 1U : 0U);
}

static void test_nono(void)
{
    /* The layout only has room for three clues a line, so every picture in the
       pack has to stay inside that in both directions. */
    assert(PA_PICTURE_COUNT >= 12);
    for (unsigned i = 0; i < PA_PICTURE_COUNT; i++) {
        const pa_picture_t *picture = &PA_PICTURE[i];
        assert(picture->name && picture->name[0]);
        unsigned marked = 0;
        for (unsigned y = 0; y < PA_NONO; y++) {
            assert((picture->row[y] & 0x80U) == 0);
            assert(line_runs(picture->row[y]) <= PA_NONO_CLUES);
            for (unsigned x = 0; x < PA_NONO; x++) marked += (picture->row[y] >> x) & 1U;
        }
        for (unsigned x = 0; x < PA_NONO; x++) {
            uint8_t column = 0;
            for (unsigned y = 0; y < PA_NONO; y++)
                if ((picture->row[y] >> x) & 1U) column |= (uint8_t)(1U << y);
            assert(line_runs(column) <= PA_NONO_CLUES);
        }
        assert(marked >= 6 && marked < PA_NONO * PA_NONO);
        for (unsigned j = 0; j < i; j++)
            assert(memcmp(picture->row, PA_PICTURE[j].row, PA_NONO) != 0);
    }

    pa_run_t run;
    start(&run, G_NONO, 293);
    pa_nono_t *n = &run.u.nono;
    assert(n->budget > 0 && n->paints == 0);

    /* A cross clears a fill and the other way round, and crossing is free. */
    press(&run, G_NONO, PA_KEY_OK);
    assert((n->fill[0] & 1U) && n->paints == 1);
    press(&run, G_NONO, PA_KEY_OK2);
    assert(!(n->fill[0] & 1U) && (n->cross[0] & 1U) && n->paints == 1);
    press(&run, G_NONO, PA_KEY_OK);
    assert((n->fill[0] & 1U) && !(n->cross[0] & 1U));

    /* Painting the picture exactly always satisfies the clues, for every
       picture the pack can pose. */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        start(&run, G_NONO, seed * 457U + 3U);
        for (unsigned y = 0; y < PA_NONO && !n->clear_ms; y++)
            for (unsigned x = 0; x < PA_NONO && !n->clear_ms; x++) {
                if (!((n->target[y] >> x) & 1U)) continue;
                n->cy = (uint8_t)y;
                n->cx = (uint8_t)x;
                press(&run, G_NONO, PA_KEY_OK);
            }
        assert(n->clear_ms > 0 && run.score > 0);
        draw_and_check(&run, G_NONO);
    }

    /* Wasting the paint budget ends the run. */
    start(&run, G_NONO, 311);
    for (unsigned i = 0; i < 400 && !run.over && !n->clear_ms; i++) {
        n->cx = 0;
        n->cy = 0;
        press(&run, G_NONO, PA_KEY_OK);
    }
    assert(run.over && run.note);

    /* Eight pictures finish the run. */
    start(&run, G_NONO, 307);
    for (unsigned guard = 0; guard < 400 && !run.over; guard++) {
        if (n->clear_ms) { tick(&run, G_NONO, 100); continue; }
        for (unsigned y = 0; y < PA_NONO && !n->clear_ms; y++)
            for (unsigned x = 0; x < PA_NONO && !n->clear_ms; x++) {
                if (!((n->target[y] >> x) & 1U)) continue;
                n->cy = (uint8_t)y;
                n->cx = (uint8_t)x;
                press(&run, G_NONO, PA_KEY_OK);
            }
    }
    assert(run.over && run.note);
}

/* ---- 30 peg solitaire ---- */
static void test_peg(void)
{
    pa_run_t run;
    start(&run, G_PEG, 311);
    pa_peg_t *p = &run.u.peg;
    assert(p->pegs == 32 && p->cell[3][3] == PA_PEG_HOLE);
    assert(p->count == 4);                            /* the cross opens with four */

    /* A jump removes exactly one peg and lands where it said it would. */
    uint8_t from = p->from[p->pick], dir = p->dir[p->pick];
    int x = from % PA_PEG_W, y = from / PA_PEG_W;
    static const int8_t SX[4] = {1, 0, -1, 0};
    static const int8_t SY[4] = {0, 1, 0, -1};
    press(&run, G_PEG, PA_KEY_OK);
    assert(p->pegs == 31 && p->jumps == 1);
    assert(p->cell[y][x] == PA_PEG_HOLE);
    assert(p->cell[y + SY[dir]][x + SX[dir]] == PA_PEG_HOLE);
    assert(p->cell[y + SY[dir] * 2][x + SX[dir] * 2] == PA_PEG_STONE);

    /* Playing until no jump remains always terminates and books a score. */
    start(&run, G_PEG, 313);
    for (unsigned guard = 0; guard < 400 && !run.over; guard++) {
        if (p->count) press(&run, G_PEG, PA_KEY_OK);
        tick(&run, G_PEG, 100);
        draw_and_check(&run, G_PEG);
    }
    assert(run.over && run.note && p->pegs + p->jumps == 32);
    assert(run.score >= (long)p->jumps * 10);
}

/* ---- 31 verse completion ---- */
static void test_poem(void)
{
    /* The catalogue itself has to be sound: no line is empty, and no two
       different verses share a second line, or a question would have two
       right answers. */
    assert(PA_VERSE_COUNT >= 60);
    for (unsigned i = 0; i < PA_VERSE_COUNT; i++) {
        const pa_verse_t *verse = &PA_VERSE[i];
        assert(verse->title[0] && verse->author[0]);
        assert(verse->up[0] && verse->down[0]);
        assert(verse->len >= 4 && verse->len <= 7);
        /* Simplified Chinese is three bytes a character in UTF-8. */
        assert(strlen(verse->up) == (size_t)verse->len * 3U);
        assert(strlen(verse->down) == (size_t)verse->len * 3U);
        for (unsigned j = 0; j < i; j++)
            assert(strcmp(verse->down, PA_VERSE[j].down) != 0);
    }

    pa_run_t run;
    start(&run, G_POEM, 401);
    pa_poem_t *p = &run.u.poem;
    assert(p->lives == 3 && p->phase == 0);

    /* Every question offers four different lines, one of them the right one,
       and the decoys match the answer's length whenever the catalogue allows. */
    for (unsigned round = 0; round < 300; round++) {
        assert(p->answer < PA_POEM_OPTIONS);
        assert(p->option[p->answer] == p->verse);
        uint8_t want = PA_VERSE[p->verse].len;
        unsigned same = 0;
        for (unsigned i = 0; i < PA_VERSE_COUNT; i++)
            if (PA_VERSE[i].len == want && i != p->verse) same++;
        for (unsigned i = 0; i < PA_POEM_OPTIONS; i++) {
            for (unsigned j = 0; j < i; j++)
                assert(strcmp(PA_VERSE[p->option[i]].down,
                              PA_VERSE[p->option[j]].down) != 0);
            if (same >= PA_POEM_OPTIONS - 1U)
                assert(PA_VERSE[p->option[i]].len == want);
        }
        draw_and_check(&run, G_POEM);
        while (p->pick != p->answer) press(&run, G_POEM, PA_KEY_DOWN);
        long before = run.score;
        press(&run, G_POEM, PA_KEY_OK);
        assert(p->correct && run.score > before && p->lives == 3);
        for (unsigned i = 0; i < 200 && p->phase == 1; i++) tick(&run, G_POEM, 33);
    }

    /* Three wrong answers end the run. */
    start(&run, G_POEM, 409);
    for (unsigned i = 0; i < 6 && !run.over; i++) {
        while (p->pick == p->answer) press(&run, G_POEM, PA_KEY_UP);
        press(&run, G_POEM, PA_KEY_OK);
        assert(!p->correct);
        for (unsigned j = 0; j < 200 && p->phase == 1; j++) tick(&run, G_POEM, 33);
    }
    assert(run.over && run.note && p->lives == 0);
}

/* ---- 32 general knowledge ---- */
static void test_quiz(void)
{
    /* The bank has to be sound: two question lines that fit, three different
       answers, and the right one first. */
    assert(PA_QUIZ_COUNT >= 60);
    for (unsigned i = 0; i < PA_QUIZ_COUNT; i++) {
        const pa_quiz_item_t *item = &PA_QUIZ[i];
        assert(item->ask1 && item->ask1[0] && item->ask2);
        for (unsigned c = 0; c < 3; c++) {
            assert(item->choice[c] && item->choice[c][0]);
            for (unsigned d = 0; d < c; d++)
                assert(strcmp(item->choice[c], item->choice[d]) != 0);
        }
        for (unsigned j = 0; j < i; j++)
            assert(strcmp(item->ask1, PA_QUIZ[j].ask1) != 0 ||
                   strcmp(item->ask2, PA_QUIZ[j].ask2) != 0);
    }

    pa_run_t run;
    start(&run, G_QUIZ, 419);
    pa_quiz_t *q = &run.u.quiz;
    assert(q->lives == 3);

    /* The right answer really does move around the three rows. */
    unsigned seen_row[3] = {0, 0, 0};
    for (unsigned round = 0; round < 150; round++) {
        unsigned correct = 3;
        for (unsigned row = 0; row < 3; row++)
            if (q->order[row] == 0) correct = row;
        assert(correct < 3);
        seen_row[correct]++;
        draw_and_check(&run, G_QUIZ);
        long before = run.score;
        press(&run, G_QUIZ, pa_key_of_row(correct));
        assert(q->correct && run.score > before && q->lives == 3);
        for (unsigned i = 0; i < 200 && q->phase == 1; i++) tick(&run, G_QUIZ, 33);
    }
    for (unsigned row = 0; row < 3; row++) assert(seen_row[row] > 20);

    /* Three wrong answers end the run. */
    start(&run, G_QUIZ, 421);
    for (unsigned i = 0; i < 6 && !run.over; i++) {
        unsigned wrong = (q->order[0] == 0) ? 1U : 0U;
        press(&run, G_QUIZ, pa_key_of_row(wrong));
        assert(!q->correct);
        for (unsigned j = 0; j < 200 && q->phase == 1; j++) tick(&run, G_QUIZ, 33);
    }
    assert(run.over && run.note && q->lives == 0);
}

int main(void)
{
    test_scene();
    test_records();
    test_key_rows();
    test_catalogue();
    test_hub();
    test_click_buffer();
    test_run_lifecycle();
    test_snake();
    test_tetris();
    test_merge();
    test_bird();
    test_dino();
    test_tiles();
    test_mole();
    test_invader();
    test_jump();
    test_sokoban();
    test_cards();
    test_pong();
    test_four();
    test_reversi();
    test_mines();
    test_slide();
    test_hanoi();
    test_frog();
    test_bowl();
    test_sling();
    test_react();
    test_pairs();
    test_brick();
    test_guess();
    test_lights();
    test_simon();
    test_gomoku();
    test_mathq();
    test_nono();
    test_peg();
    test_poem();
    test_quiz();
    test_soak();
    printf("pocket arcade state: PASS (%d games)\n", PA_GAME_COUNT);
    return 0;
}
