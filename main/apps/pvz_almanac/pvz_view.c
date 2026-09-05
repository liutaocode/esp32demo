#include "pvz_view.h"
#include "pvz_art.h"
#include "ui_pixel.h"

LV_FONT_DECLARE(pvz_zh_12);
LV_FONT_DECLARE(pvz_zh_16);
LV_FONT_DECLARE(pvz_zh_20);

#define PAPER 0xF6F3E8
#define WHITE 0xFFFEF8
#define FOREST 0x264C3B
#define INK 0x293D32
#define MUTED 0x778171
#define LINE 0xDCE1CF
#define MINT 0xE3EDD8
#define GOLD 0xE8BC5B

static lv_obj_t *screen, *body, *battery, *battery_fill, *voice, *voice_dot;
static pvz_page_t current_page;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h,
                     uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *s, int x, int y, int w,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(parent, s, font, color);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_obj_set_style_text_line_space(o, 2, 0);
    return o;
}

static lv_obj_t *small(lv_obj_t *p, const char *s, int x, int y, int w, uint32_t c)
{
    return label(p, s, x, y, w, &pvz_zh_12, c);
}

static void center(lv_obj_t *o)
{
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
}

static void outline(lv_obj_t *o, uint32_t color)
{
    lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(o, 1, 0);
}

static void leaf(lv_obj_t *p, int x, int y, uint32_t color)
{
    box(p, x + 6, y + 5, 2, 13, color, 1);
    box(p, x, y + 2, 8, 6, color, 4);
    box(p, x + 8, y, 8, 7, color, 4);
}

static void footer(const char *left, const char *right, const char *hint)
{
    box(body, 0, 247, 240, 38, FOREST, 0);
    small(body, left, 12, 252, 113, WHITE);
    lv_obj_t *r = small(body, right, 128, 252, 100, 0xF5D98F);
    lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_RIGHT, 0);
    center(small(body, hint, 8, 269, 224, 0xB9CDBA));
}

static void speech_line(void)
{
    voice_dot = box(body, 12, 233, 5, 5, MUTED, 3);
    voice = small(body, "按确定，听听它的故事", 23, 227, 205, MUTED);
}

static void badge(lv_obj_t *p, const char *s, int x, int y, int w,
                   uint32_t background, uint32_t color)
{
    box(p, x, y, w, 21, background, 6);
    center(small(p, s, x + 3, y + 3, w - 6, color));
}

static void scene(int x, int y, int w, int h, uint32_t background)
{
    lv_obj_t *o = box(body, x, y, w, h, background, 12);
    /* Soft botanical backdrop. All geometry stays in the card's own clip. */
    box(o, 7, h - 22, w - 14, 16, 0xCFDEBF, 8);
    box(o, w - 32, 10, 16, 16, 0xF4E6A9, 8);
    for (int i = 0; i < 4; ++i) {
        box(o, 12 + i * 27, 13 + (i % 2) * 12, 2, 2, 0xB4C4A6, 1);
    }
}

lv_obj_t *pvz_view_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(screen, 240, 320);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    box(screen, 0, 0, 240, 35, FOREST, 0);
    leaf(screen, 10, 9, 0xC7DB91);
    label(screen, "草坪研究所", 33, 5, 136, &pvz_zh_20, WHITE);
    lv_obj_t *shell = box(screen, 179, 13, 13, 9, FOREST, 2);
    outline(shell, 0xA8C2A8);
    battery_fill = box(shell, 2, 2, 8, 3, 0xC7DB91, 1);
    box(screen, 192, 16, 2, 3, 0xA8C2A8, 0);
    battery = small(screen, "--%", 199, 10, 35, 0xD2DFCC);
    body = lv_obj_create(screen);
    lv_obj_remove_style_all(body);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(body, 0, 35);
    lv_obj_set_size(body, 240, 285);
    return screen;
}

static void home(const pvz_state_t *s, bool buttons_ok)
{
    small(body, "植物大战僵尸 / 经典一代", 12, 9, 215, MUTED);
    scene(12, 32, 216, 73, MINT);
    label(body, "你的草坪伙伴", 23, 42, 124, &pvz_zh_20, FOREST);
    small(body, "翻一翻，听一听", 24, 72, 108, 0x60795B);
    /* Compact hero portrait leaves room for the headline. */
    lv_obj_t *portrait = pvz_art(body, 0, 147, 34);
    lv_obj_set_style_transform_pivot_x(portrait, 0, 0);
    lv_obj_set_style_transform_pivot_y(portrait, 0, 0);
    lv_obj_set_style_transform_scale_x(portrait, 205, 0);
    lv_obj_set_style_transform_scale_y(portrait, 205, 0);

    const char *names[] = {"植物图鉴", "僵尸图鉴", "听线索猜角色"};
    const char *notes[] = {"16 位绿色守卫", "8 位入侵者", "五题挑战，传给朋友猜"};
    for (unsigned i = 0; i < 3; ++i) {
        bool selected = s->menu == i;
        int y = 110 + i * 40;
        lv_obj_t *row = box(body, 12, y, 216, 36, selected ? FOREST : WHITE, 7);
        if (!selected) outline(row, LINE);
        box(body, 20, y + 8, 19, 19, selected ? GOLD : MINT, 5);
        lv_obj_t *n = small(body, "", 22, y + 10, 15, FOREST);
        lv_label_set_text_fmt(n, "%u", i + 1); center(n);
        label(body, names[i], 47, y + 1, 169, &pvz_zh_16, selected ? WHITE : INK);
        small(body, notes[i], 48, y + 20, 166, selected ? 0xC5D7BD : MUTED);
    }
    lv_obj_t *seen = small(body, "", 13, 230, 190, MUTED);
    lv_label_set_text_fmt(seen, "本次认识  %u / 24", pvz_seen_count(s));
    box(body, 157, 234, 70, 4, LINE, 2);
    unsigned n = pvz_seen_count(s);
    if (n) box(body, 157, 234, (70 * n + 23) / 24, 4, 0x88A66C, 2);
    footer(buttons_ok ? "上/下  选择" : "按键不可用", "确定  进入", "离线语音 · 同人图鉴");
}

static void detail(const pvz_state_t *s, bool reveal)
{
    const pvz_entry_t *e = &pvz_catalog[s->index];
    small(body, reveal ? (s->correct ? "答对了！新的发现" : "答案揭晓 · 一起认识它") :
          (e->kind ? "僵尸档案" : "植物档案"), 12, 9, 165, MUTED);
    lv_obj_t *number = small(body, "", 184, 9, 44, MUTED);
    lv_label_set_text_fmt(number, "%02u/%02u", s->index - (e->kind ? PVZ_PLANTS : 0) + 1,
                           e->kind ? PVZ_COUNT - PVZ_PLANTS : PVZ_PLANTS);
    scene(12, 31, 216, 108, e->kind ? 0xE6E4DA : MINT);
    pvz_art(body, s->index, 21, 41);
    label(body, e->name, 115, 46, 107, &pvz_zh_20, FOREST);
    small(body, e->role, 117, 77, 100, 0x698060);
    badge(body, e->kind ? "入侵者" : "阳光", 117, 102, e->kind ? 72 : 43,
          e->kind ? 0xD5D8CA : 0xF3E1AD, e->kind ? 0x52674F : 0x906B2C);
    if (e->cost >= 0) {
        lv_obj_t *cost = label(body, "", 165, 101, 53, &pvz_zh_16, 0x906B2C);
        lv_label_set_text_fmt(cost, "%d", e->cost);
    }
    small(body, "招牌能力", 13, 151, 180, MUTED);
    label(body, e->ability, 13, 168, 216, &pvz_zh_16, INK);
    box(body, 12, 195, 216, 1, LINE, 0);
    small(body, "草坪小贴士", 13, 202, 180, 0x819071);
    /* Tips are one line in the current curated catalog. */
    label(body, e->tip, 13, 218, 216, &pvz_zh_16, 0x4B6746);
    footer(reveal ? "长按上  重听" : "上/下  翻页",
           reveal ? "确定  下一题" : "确定  讲解", "长按确定返回 · 长按上重听");
    /* Speech status sits in the hero, away from the reading content. */
    voice_dot = box(body, 118, 130, 4, 4, MUTED, 2);
    voice = small(body, "可以听讲解", 127, 125, 94, MUTED);
}

static void quiz(const pvz_state_t *s)
{
    small(body, "听线索猜角色", 12, 9, 150, MUTED);
    lv_obj_t *round = small(body, "", 189, 9, 39, MUTED);
    lv_label_set_text_fmt(round, "%u / %u", s->round + 1, PVZ_ROUNDS);
    for (unsigned i = 0; i < PVZ_ROUNDS; ++i)
        box(body, 12 + i * 44, 29, 39, 3, i <= s->round ? 0x85A16D : LINE, 2);
    lv_obj_t *card = box(body, 12, 42, 216, 89, WHITE, 10);
    outline(card, LINE);
    badge(body, "线索", 24, 50, 41, MINT, FOREST);
    small(body, "谁藏在这句话里？", 73, 53, 143, MUTED);
    label(body, pvz_catalog[s->deck[s->round]].clue, 24, 80, 192, &pvz_zh_16, INK);
    for (unsigned i = 0; i < PVZ_OPTIONS; ++i) {
        int y = 141 + i * 28;
        bool chosen = s->choice == i;
        lv_obj_t *o = box(body, 12, y, 216, 24, chosen ? FOREST : WHITE, 6);
        if (!chosen) outline(o, LINE);
        box(body, 21, y + 7, 9, 9, chosen ? GOLD : MINT, 5);
        label(body, pvz_catalog[s->options[i]].name, 40, y + 2, 174,
              &pvz_zh_16, chosen ? WHITE : INK);
    }
    speech_line();
    footer("上/下  选答案", "确定  提交", "长按上重听 · 长按确定返回");
}

static void result(const pvz_state_t *s)
{
    center(small(body, "草坪研究报告", 12, 10, 216, MUTED));
    lv_obj_t *card = box(body, 12, 36, 216, 188, WHITE, 12);
    outline(card, LINE);
    box(body, 76, 46, 88, 88, MINT, 44);
    pvz_art(body, 1, 78, 43);
    center(label(body, pvz_rank(s->score), 20, 137, 200, &pvz_zh_20, FOREST));
    lv_obj_t *score = label(body, "", 23, 168, 194, &pvz_zh_16, 0x916D2E);
    lv_label_set_text_fmt(score, "答对 %u / 5  ·  挑战完成", s->score); center(score);
    lv_obj_t *code = small(body, "", 23, 199, 194, MUTED);
    lv_label_set_text_fmt(code, "题组 %04lu · 邀朋友挑战同题", (unsigned long)s->seed); center(code);
    center(small(body, "把这张成绩卡，分享给朋友", 12, 230, 216, MUTED));
    footer("长按确定  返回", "确定  再战", "同一组题目，看看谁更厉害");
}

void pvz_view_render(const pvz_state_t *s, bool buttons_ok)
{
    voice = voice_dot = NULL;
    current_page = s->page;
    lv_obj_clean(body);
    switch (s->page) {
    case PVZ_HOME: home(s, buttons_ok); break;
    case PVZ_BROWSE: case PVZ_REVEAL: detail(s, s->page == PVZ_REVEAL); break;
    case PVZ_QUIZ: quiz(s); break;
    default: result(s); break;
    }
}

void pvz_view_status(int status)
{
    if (!voice) return;
    bool compact = current_page == PVZ_BROWSE || current_page == PVZ_REVEAL;
    const char *message;
    if (status < 0) message = compact ? "语音不可用" : "语音不可用，可继续答题";
    else if (status == 1) message = compact ? "正在讲解…" : "正在读线索…";
    else if (status == 2) message = compact ? "讲解完毕" : "听完了，选出你的答案";
    else message = compact ? "可以听讲解" : "长按上键，再听一次";
    lv_label_set_text(voice, message);
    lv_obj_set_style_bg_color(voice_dot, lv_color_hex(status == 1 ? 0x77A258 : MUTED), 0);
}

void pvz_view_battery(int soc)
{
    if (soc < 0) lv_label_set_text(battery, "--%");
    else lv_label_set_text_fmt(battery, "%d%%", soc > 100 ? 100 : soc);
    lv_obj_set_width(battery_fill, soc < 0 ? 1 : LV_MAX(1, LV_MIN(100, soc) * 8 / 100));
    lv_obj_set_style_bg_opa(battery_fill, soc < 0 ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
}

void pvz_view_destroy(void)
{
    if (screen) lv_obj_delete(screen);
    screen = body = battery = battery_fill = voice = voice_dot = NULL;
}
