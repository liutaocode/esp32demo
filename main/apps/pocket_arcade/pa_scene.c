#include "pa_scene.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void pa_scene_reset(pa_scene_t *scene)
{
    scene->rects = 0;
    scene->texts = 0;
    scene->footer[0] = '\0';
}

/* 把矩形裁进 [0,w)x[0,h),空矩形返回 false。裁剪保证渲染层不会画出面板。 */
static bool clip(int *x, int *y, int *w, int *h, int limit_w, int limit_h)
{
    int x2 = *x + *w, y2 = *y + *h;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (x2 > limit_w) x2 = limit_w;
    if (y2 > limit_h) y2 = limit_h;
    *w = x2 - *x;
    *h = y2 - *y;
    return *w > 0 && *h > 0;
}

void pa_rect(pa_scene_t *scene, int x, int y, int w, int h, uint32_t color, int radius)
{
    if (scene->rects >= PA_MAX_RECTS) return;
    if (!clip(&x, &y, &w, &h, PA_VIEW_W, PA_VIEW_H)) return;
    scene->rect[scene->rects++] = (pa_rect_t){
        (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, color, (uint8_t)radius
    };
}

void pa_frect(pa_scene_t *scene, int x, int y, int w, int h, uint32_t color, int radius)
{
    if (scene->rects >= PA_MAX_RECTS) return;
    if (!clip(&x, &y, &w, &h, PA_FIELD_W, PA_FIELD_H)) return;
    scene->rect[scene->rects++] = (pa_rect_t){
        (int16_t)(x + PA_FIELD_X), (int16_t)(y + PA_FIELD_Y),
        (int16_t)w, (int16_t)h, color, (uint8_t)radius
    };
}

void pa_text(pa_scene_t *scene, int x, int y, int w, const char *text,
             uint32_t color, pa_font_t font, pa_align_t align)
{
    if (scene->texts >= PA_MAX_TEXTS || !text || !text[0]) return;
    pa_text_t *slot = &scene->text[scene->texts++];
    slot->x = (int16_t)x;
    slot->y = (int16_t)y;
    slot->w = (int16_t)w;
    slot->color = color;
    slot->font = (uint8_t)font;
    slot->align = (uint8_t)align;
    snprintf(slot->text, sizeof(slot->text), "%s", text);
}

void pa_num(pa_scene_t *scene, int x, int y, int w, long value,
            uint32_t color, pa_font_t font, pa_align_t align)
{
    char buffer[PA_TEXT_MAX];
    snprintf(buffer, sizeof(buffer), "%ld", value);
    pa_text(scene, x, y, w, buffer, color, font, align);
}

void pa_textf(pa_scene_t *scene, int x, int y, int w, uint32_t color,
              pa_font_t font, pa_align_t align, const char *format, ...)
{
    char buffer[PA_TEXT_MAX];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    pa_text(scene, x, y, w, buffer, color, font, align);
}

void pa_footer(pa_scene_t *scene, const char *text)
{
    snprintf(scene->footer, sizeof(scene->footer), "%s", text ? text : "");
}

/* 每度一格的四分之一周期正弦表,其余三个象限靠对称补出来。 */
static const int16_t SINE[91] = {
       0,   17,   35,   52,   70,   87,  105,  122,  139,  156,
     174,  191,  208,  225,  242,  259,  276,  292,  309,  326,
     342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
     500,  515,  530,  545,  559,  574,  588,  602,  616,  629,
     643,  656,  669,  682,  695,  707,  719,  731,  743,  755,
     766,  777,  788,  799,  809,  819,  829,  839,  848,  857,
     866,  875,  883,  891,  899,  906,  914,  921,  927,  934,
     940,  946,  951,  956,  961,  966,  970,  974,  978,  982,
     985,  988,  990,  993,  995,  996,  998,  999,  999, 1000,
    1000,
};

int pa_sin(int degrees)
{
    int angle = degrees % 360;
    if (angle < 0) angle += 360;
    if (angle <= 90) return SINE[angle];
    if (angle <= 180) return SINE[180 - angle];
    if (angle <= 270) return -SINE[angle - 180];
    return -SINE[360 - angle];
}

int pa_cos(int degrees) { return pa_sin(degrees + 90); }

uint32_t pa_rand(uint32_t *state)
{
    uint32_t x = *state ? *state : 0x9E3779B9U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

unsigned pa_below(uint32_t *state, unsigned bound)
{
    return bound ? (unsigned)(pa_rand(state) % bound) : 0U;
}
