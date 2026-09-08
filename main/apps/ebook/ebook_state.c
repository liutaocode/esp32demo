// main/apps/ebook/ebook_state.c —— 见 ebook_state.h。纯 C,不碰硬件。
#include "ebook_state.h"

#include <string.h>

static void wrap_sel(int *sel, int count, int delta)
{
    if (count <= 0) { *sel = 0; return; }
    int next = *sel + delta;
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;
    *sel = next;
}

void eb_state_init(eb_model_t *m, eb_font_t font, eb_theme_id_t theme)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->screen = EB_SCREEN_SHELF;
    m->font = (font >= 0 && font < EB_FONT_COUNT) ? font : EB_FONT_M;
    m->theme = (theme >= 0 && theme < EB_THEME_COUNT) ? theme : EB_THEME_WHITE;
}

void eb_state_set_books(eb_model_t *m, int count)
{
    if (!m) return;
    if (count < 0) count = 0;
    if (count > EB_MAX_BOOKS) count = EB_MAX_BOOKS;
    m->book_count = count;
    if (m->shelf_sel >= count) m->shelf_sel = count > 0 ? count - 1 : 0;
}

void eb_state_seek(eb_model_t *m, uint32_t offset)
{
    if (!m) return;
    m->page_offset = offset;
    m->history_len = 0;
    m->history_lost = false;
    m->at_eof = false;
}

void eb_state_advance(eb_model_t *m, uint32_t next_offset)
{
    if (!m) return;
    if (m->history_len == EB_HISTORY_MAX) {
        // 历史满了就丢最旧的一页:再往回翻只能回到保留区的第一页。
        memmove(m->history, m->history + 1, sizeof(m->history[0]) * (EB_HISTORY_MAX - 1));
        m->history_len = EB_HISTORY_MAX - 1;
        m->history_lost = true;
    }
    m->history[m->history_len++] = m->page_offset;
    m->page_offset = next_offset;
}

void eb_state_set_eof(eb_model_t *m, bool at_eof)
{
    if (m) m->at_eof = at_eof;
}

bool eb_state_add_mark(eb_model_t *m, uint32_t offset)
{
    if (!m) return false;
    for (int k = 0; k < m->mark_count; k++) {
        if (m->marks[k] == offset) return false;
    }
    if (m->mark_count == EB_MAX_MARKS) {
        memmove(m->marks, m->marks + 1, sizeof(m->marks[0]) * (EB_MAX_MARKS - 1));
        m->mark_count = EB_MAX_MARKS - 1;
    }
    m->marks[m->mark_count++] = offset;
    return true;
}

void eb_state_set_marks(eb_model_t *m, const uint32_t *marks, int count)
{
    if (!m) return;
    if (count < 0) count = 0;
    if (count > EB_MAX_MARKS) count = EB_MAX_MARKS;
    if (marks && count > 0) memcpy(m->marks, marks, sizeof(m->marks[0]) * (size_t)count);
    m->mark_count = count;
    if (m->mark_sel >= count) m->mark_sel = count > 0 ? count - 1 : 0;
}

int eb_font_px(eb_font_t font)
{
    switch (font) {
    case EB_FONT_S: return 12;
    case EB_FONT_L: return 20;
    default:        return 16;
    }
}

int eb_line_height(eb_font_t font)
{
    switch (font) {
    case EB_FONT_S: return 17;
    case EB_FONT_L: return 27;
    default:        return 22;
    }
}

static eb_action_t key_shelf(eb_model_t *m, eb_key_t key, eb_ev_t ev)
{
    if (key == EB_KEY_OK && ev == EB_EV_LONG) {
        m->screen = EB_SCREEN_XFER;
        return EB_ACT_XFER_START;
    }
    if (ev != EB_EV_CLICK) return EB_ACT_NONE;
    if (key == EB_KEY_UP)   { wrap_sel(&m->shelf_sel, m->book_count, -1); return EB_ACT_REDRAW; }
    if (key == EB_KEY_DOWN) { wrap_sel(&m->shelf_sel, m->book_count, +1); return EB_ACT_REDRAW; }
    if (m->book_count == 0) return EB_ACT_NONE;
    // 从书架打开就接着上次读到的地方,而不是每次从头开始。
    m->open_offset = 0;
    m->open_resume = true;
    m->screen = EB_SCREEN_READER;
    return EB_ACT_OPEN_BOOK;
}

static eb_action_t key_reader(eb_model_t *m, eb_key_t key, eb_ev_t ev)
{
    if (key == EB_KEY_OK) {
        if (ev == EB_EV_LONG) {
            m->screen = EB_SCREEN_SHELF;
            return EB_ACT_CLOSE_BOOK;
        }
        m->screen = EB_SCREEN_MENU;
        m->menu_sel = 0;
        return EB_ACT_REDRAW;
    }
    // 上下键的长按也当翻页:按键组件把"按得略久"报成长按而不是单击,
    // 只认单击的话,同样一下翻页时灵时不灵,用起来像卡住。
    if (key == EB_KEY_DOWN) {
        if (m->at_eof) return EB_ACT_NONE;
        return EB_ACT_PAGE_NEXT;
    }
    // 上一页:历史里存着走过来的每一页起点,不必反向重新排版。
    // 但从书架续读、跳书签进来的那一页没有来路,只能让运行时往回重排。
    if (m->history_len == 0) {
        return m->page_offset > 0 ? EB_ACT_PAGE_RESCAN : EB_ACT_NONE;
    }
    m->page_offset = m->history[--m->history_len];
    m->at_eof = false;
    return EB_ACT_PAGE_PREV;
}

static eb_action_t key_menu(eb_model_t *m, eb_key_t key, eb_ev_t ev)
{
    if (key == EB_KEY_OK && ev == EB_EV_LONG) {
        m->screen = EB_SCREEN_READER;
        return EB_ACT_REDRAW;
    }
    if (ev != EB_EV_CLICK) return EB_ACT_NONE;
    if (key == EB_KEY_UP)   { wrap_sel(&m->menu_sel, EB_MENU_COUNT, -1); return EB_ACT_REDRAW; }
    if (key == EB_KEY_DOWN) { wrap_sel(&m->menu_sel, EB_MENU_COUNT, +1); return EB_ACT_REDRAW; }

    switch ((eb_menu_item_t)m->menu_sel) {
    case EB_MENU_ADD_MARK:
        m->screen = EB_SCREEN_READER;
        return EB_ACT_ADD_MARK;
    case EB_MENU_MARKS:
        if (m->mark_count == 0) return EB_ACT_REDRAW;   // 界面提示"还没有书签"
        m->screen = EB_SCREEN_MARKS;
        m->mark_sel = 0;
        return EB_ACT_REDRAW;
    case EB_MENU_STATS:
        m->screen = EB_SCREEN_STATS;
        return EB_ACT_REDRAW;
    case EB_MENU_FONT:
        // 三档循环,停在菜单里让用户连按比较,当前页立刻按新字号重排。
        m->font = (eb_font_t)((m->font + 1) % EB_FONT_COUNT);
        m->font_changed = true;
        return EB_ACT_RELAYOUT;
    case EB_MENU_THEME:
        // 同样停在菜单里:几套配色要能连按一路比过去,不必来回进出正文。
        m->theme = (eb_theme_id_t)((m->theme + 1) % EB_THEME_COUNT);
        m->theme_changed = true;
        return EB_ACT_THEME;
    default:
        m->screen = EB_SCREEN_READER;
        return EB_ACT_REDRAW;
    }
}

static eb_action_t key_marks(eb_model_t *m, eb_key_t key, eb_ev_t ev)
{
    if (key == EB_KEY_OK && ev == EB_EV_LONG) {
        m->screen = EB_SCREEN_MENU;
        return EB_ACT_REDRAW;
    }
    if (ev != EB_EV_CLICK) return EB_ACT_NONE;
    if (key == EB_KEY_UP)   { wrap_sel(&m->mark_sel, m->mark_count, -1); return EB_ACT_REDRAW; }
    if (key == EB_KEY_DOWN) { wrap_sel(&m->mark_sel, m->mark_count, +1); return EB_ACT_REDRAW; }
    if (m->mark_count == 0) { m->screen = EB_SCREEN_MENU; return EB_ACT_REDRAW; }
    m->open_offset = m->marks[m->mark_sel];
    m->open_resume = false;   // 书签是明确指定的位置,不受上次读到哪影响
    m->screen = EB_SCREEN_READER;
    return EB_ACT_OPEN_BOOK;
}

// 统计页只看不选,长按确定回菜单。
static eb_action_t key_stats(eb_model_t *m, eb_key_t key, eb_ev_t ev)
{
    if (key == EB_KEY_OK && ev == EB_EV_LONG) {
        m->screen = EB_SCREEN_MENU;
        return EB_ACT_REDRAW;
    }
    return EB_ACT_NONE;
}

static eb_action_t key_xfer(eb_model_t *m, eb_key_t key, eb_ev_t ev)
{
    if (key == EB_KEY_OK && ev == EB_EV_LONG) {
        m->screen = EB_SCREEN_SHELF;
        return EB_ACT_XFER_STOP;
    }
    return EB_ACT_NONE;
}

eb_action_t eb_state_key(eb_model_t *m, eb_key_t key, eb_ev_t ev)
{
    if (!m) return EB_ACT_NONE;
    switch (m->screen) {
    case EB_SCREEN_SHELF:  return key_shelf(m, key, ev);
    case EB_SCREEN_READER: return key_reader(m, key, ev);
    case EB_SCREEN_MENU:   return key_menu(m, key, ev);
    case EB_SCREEN_MARKS:  return key_marks(m, key, ev);
    case EB_SCREEN_STATS:  return key_stats(m, key, ev);
    default:               return key_xfer(m, key, ev);
    }
}
