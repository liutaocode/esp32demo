#pragma once
#include <stdbool.h>
#include <stdint.h>

/* 场景 = 一屏矩形和文字的清单。游戏只往清单里写,不碰 LVGL 对象,
   所以每个游戏都能在主机上跑排版与规则回归。坐标是内容面板的局部像素。 */

enum {
    PA_VIEW_W = 198, PA_VIEW_H = 204,   /* 内容面板可用区 */
    PA_FIELD_X = 0, PA_FIELD_Y = 20,    /* 游戏画面在面板里的原点 */
    PA_FIELD_W = 198, PA_FIELD_H = 160,
    PA_ROW_TOP = 0, PA_ROW_BOTTOM = 184,
    PA_MAX_RECTS = 128, PA_MAX_TEXTS = 48,
    PA_TEXT_MAX = 64, PA_FOOTER_MAX = 56,
};

/* 中文档带英数回退,数字档是 LVGL 内置字体,中文放进去会缺字。
   一行 198 像素:16 号最多 12 个汉字,12 号最多 16 个,底栏 232 像素放 14 个。 */
typedef enum { PA_FONT_ZH = 0, PA_FONT_ZH12, PA_FONT_NUM14, PA_FONT_NUM20 } pa_font_t;
typedef enum { PA_LEFT = 0, PA_CENTER, PA_RIGHT } pa_align_t;

#define PA_INK     0x17202AU
#define PA_PAPER   0xF4F4EAU
#define PA_WHITE   0xFFFFFFU
#define PA_CREAM   0xFFF3D0U
#define PA_SKY     0x1689E8U
#define PA_GRASS   0x82BE2DU
#define PA_GRASSD  0x55951DU
#define PA_YELLOW  0xFFD928U
#define PA_ORANGE  0xFFB23EU
#define PA_RED     0xE43B2FU
#define PA_GREEN   0x168B79U
#define PA_BLUE    0x2F6FD0U
#define PA_PURPLE  0x7557D9U
#define PA_PINK    0xE8639BU
#define PA_GRAY    0x9AA0A6U
#define PA_SLATE   0x2B3947U
#define PA_SAND    0xE3D5A8U

typedef struct { int16_t x, y, w, h; uint32_t color; uint8_t radius; } pa_rect_t;

typedef struct {
    int16_t x, y, w;
    uint32_t color;
    uint8_t font, align;
    char text[PA_TEXT_MAX];
} pa_text_t;

typedef struct {
    pa_rect_t rect[PA_MAX_RECTS];
    pa_text_t text[PA_MAX_TEXTS];
    uint8_t rects, texts;
    char footer[PA_FOOTER_MAX];   /* 屏幕最下方的按键提示,画在面板外面 */
} pa_scene_t;

void pa_scene_reset(pa_scene_t *scene);

/* 面板坐标。超出可用区的部分被裁掉,越界的矩形直接丢弃。 */
void pa_rect(pa_scene_t *scene, int x, int y, int w, int h, uint32_t color, int radius);
/* 游戏画面坐标,自动加上画面原点并裁到画面内。 */
void pa_frect(pa_scene_t *scene, int x, int y, int w, int h, uint32_t color, int radius);

void pa_text(pa_scene_t *scene, int x, int y, int w, const char *text,
             uint32_t color, pa_font_t font, pa_align_t align);
void pa_num(pa_scene_t *scene, int x, int y, int w, long value,
            uint32_t color, pa_font_t font, pa_align_t align);
void pa_footer(pa_scene_t *scene, const char *text);

/* 带格式的文字。超过一行长度会被截断,不会写坏缓冲区。 */
void pa_textf(pa_scene_t *scene, int x, int y, int w, uint32_t color,
              pa_font_t font, pa_align_t align, const char *format, ...)
    __attribute__((format(printf, 8, 9)));

/* 全部游戏共用的确定性随机数,便于用题号复现同一局。 */
/* 定点三角函数:输入整度,返回 -1000..1000。抛射类游戏共用。 */
int pa_sin(int degrees);
int pa_cos(int degrees);

uint32_t pa_rand(uint32_t *state);
unsigned pa_below(uint32_t *state, unsigned bound);
