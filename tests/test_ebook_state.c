#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ebook_state.h"

int main(void) {
    eb_model_t m;
    eb_state_init(&m, EB_FONT_M, EB_THEME_WHITE);
    assert(m.screen == EB_SCREEN_SHELF && m.font == EB_FONT_M);
    eb_state_init(&m, (eb_font_t)99, (eb_theme_id_t)99);
    assert(m.font == EB_FONT_M);   // 非法字号回落到中号
    assert(m.theme == EB_THEME_WHITE);   // 非法主题回落到纯白

    // 空书架:确定键不该打开任何东西,长按仍然可以去传书。
    eb_state_init(&m, EB_FONT_M, EB_THEME_WHITE);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_NONE);
    assert(m.screen == EB_SCREEN_SHELF);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_LONG) == EB_ACT_XFER_START);
    assert(m.screen == EB_SCREEN_XFER);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_NONE);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_LONG) == EB_ACT_XFER_STOP);
    assert(m.screen == EB_SCREEN_SHELF);

    // 书架选择环绕;传书后书变少时选中项被夹回范围内。
    eb_state_set_books(&m, 3);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_REDRAW && m.shelf_sel == 2);
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_CLICK) == EB_ACT_REDRAW && m.shelf_sel == 0);
    m.shelf_sel = 2;
    eb_state_set_books(&m, 1);
    assert(m.shelf_sel == 0);
    eb_state_set_books(&m, 999);
    assert(m.book_count == EB_MAX_BOOKS);
    eb_state_set_books(&m, -1);
    assert(m.book_count == 0 && m.shelf_sel == 0);

    // 打开一本书,从头开始读。
    eb_state_set_books(&m, 2);
    m.shelf_sel = 1;
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_OPEN_BOOK);
    // 从书架打开:要求接着上次的位置,而不是指定偏移。
    assert(m.screen == EB_SCREEN_READER && m.open_resume);
    eb_state_seek(&m, 0);

    // 翻页历史:向前走三页再往回翻,偏移必须精确还原。
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_NONE);   // 第一页没有上一页
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_CLICK) == EB_ACT_PAGE_NEXT);
    eb_state_advance(&m, 300);
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_CLICK) == EB_ACT_PAGE_NEXT);
    eb_state_advance(&m, 640);
    assert(m.page_offset == 640 && m.history_len == 2);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_PAGE_PREV && m.page_offset == 300);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_PAGE_PREV && m.page_offset == 0);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_NONE);

    // 上下键按得略久会被报成长按,翻页必须照常发生,否则手感上就是"时灵时不灵"。
    eb_state_seek(&m, 0);
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_LONG) == EB_ACT_PAGE_NEXT);
    eb_state_advance(&m, 220);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_LONG) == EB_ACT_PAGE_PREV);
    assert(m.page_offset == 0);

    // 文末不再往下翻;回退一页后又可以往下翻。
    eb_state_set_eof(&m, true);
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_CLICK) == EB_ACT_NONE);
    eb_state_advance(&m, 100);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_PAGE_PREV);
    assert(!m.at_eof);

    // 历史溢出:最旧的一页被挤掉并留下标记,回退不会越界。
    eb_state_seek(&m, 0);
    for (int k = 1; k <= EB_HISTORY_MAX + 5; k++) eb_state_advance(&m, (uint32_t)k * 10);
    assert(m.history_len == EB_HISTORY_MAX && m.history_lost);
    for (int k = 0; k < EB_HISTORY_MAX; k++) {
        assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_PAGE_PREV);
    }
    assert(m.history_len == 0);
    // 历史被挤光之后同样不能没反应:连读两百多页的人往回翻,必须交给重排。
    assert(m.page_offset > 0);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_PAGE_RESCAN);

    // 菜单:确定键进,长按退,选项环绕。
    eb_state_seek(&m, 512);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_REDRAW);
    assert(m.screen == EB_SCREEN_MENU && m.menu_sel == EB_MENU_ADD_MARK);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_REDRAW);
    assert(m.menu_sel == EB_MENU_COUNT - 1);
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_CLICK) == EB_ACT_REDRAW && m.menu_sel == 0);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_LONG) == EB_ACT_REDRAW);
    assert(m.screen == EB_SCREEN_READER);

    // 字号三档循环,每次都要求重排并标记需要持久化。
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_REDRAW);
    m.menu_sel = EB_MENU_FONT;
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_RELAYOUT);
    assert(m.font == EB_FONT_L && m.font_changed && m.screen == EB_SCREEN_MENU);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_RELAYOUT && m.font == EB_FONT_S);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_RELAYOUT && m.font == EB_FONT_M);
    assert(eb_font_px(EB_FONT_S) == 12 && eb_font_px(EB_FONT_M) == 16 && eb_font_px(EB_FONT_L) == 20);
    for (int f = 0; f < EB_FONT_COUNT; f++) assert(eb_line_height((eb_font_t)f) > eb_font_px((eb_font_t)f));

    // 主题:五档循环一圈回到原处,每次都要求换配色并标记需要持久化。
    m.menu_sel = EB_MENU_THEME;
    eb_theme_id_t first = m.theme;
    for (int step = 1; step <= EB_THEME_COUNT; step++) {
        m.theme_changed = false;
        assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_THEME);
        assert(m.theme_changed && m.screen == EB_SCREEN_MENU);
        assert(m.theme == (eb_theme_id_t)((first + step) % EB_THEME_COUNT));
    }
    assert(m.theme == first);

    // 阅读统计:进得去、只看不选、长按确定回菜单。
    m.menu_sel = EB_MENU_STATS;
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_REDRAW);
    assert(m.screen == EB_SCREEN_STATS);
    assert(eb_state_key(&m, EB_KEY_UP, EB_EV_CLICK) == EB_ACT_NONE);
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_CLICK) == EB_ACT_NONE);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_NONE);
    assert(m.screen == EB_SCREEN_STATS);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_LONG) == EB_ACT_REDRAW);
    assert(m.screen == EB_SCREEN_MENU);

    // 没有书签时"书签列表"只重绘,不进空列表页。
    m.menu_sel = EB_MENU_MARKS;
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_REDRAW);
    assert(m.screen == EB_SCREEN_MENU);

    // 添加书签:回到正文,重复的偏移不会存两次,满了丢最旧的一条。
    m.menu_sel = EB_MENU_ADD_MARK;
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_ADD_MARK);
    assert(m.screen == EB_SCREEN_READER);
    assert(eb_state_add_mark(&m, 512) && m.mark_count == 1);
    assert(!eb_state_add_mark(&m, 512) && m.mark_count == 1);
    for (uint32_t k = 1; k < EB_MAX_MARKS + 3; k++) eb_state_add_mark(&m, k);
    assert(m.mark_count == EB_MAX_MARKS);
    assert(m.marks[EB_MAX_MARKS - 1] == EB_MAX_MARKS + 2);

    // 书签列表:选中一条跳过去,当作重新打开这本书。
    uint32_t marks[3] = { 40, 80, 120 };
    eb_state_set_marks(&m, marks, 3);
    assert(m.mark_count == 3 && m.mark_sel == 0);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_REDRAW);   // 进菜单
    m.menu_sel = EB_MENU_MARKS;
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_REDRAW);
    assert(m.screen == EB_SCREEN_MARKS);
    assert(eb_state_key(&m, EB_KEY_DOWN, EB_EV_CLICK) == EB_ACT_REDRAW && m.mark_sel == 1);
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_OPEN_BOOK);
    assert(m.open_offset == 80 && !m.open_resume && m.screen == EB_SCREEN_READER);

    // 正文长按确定回书架。
    assert(eb_state_key(&m, EB_KEY_OK, EB_EV_LONG) == EB_ACT_CLOSE_BOOK);
    assert(m.screen == EB_SCREEN_SHELF);

    // 空指针不崩。
    assert(eb_state_key(NULL, EB_KEY_OK, EB_EV_CLICK) == EB_ACT_NONE);
    eb_state_seek(NULL, 0);
    eb_state_advance(NULL, 0);
    eb_state_set_marks(NULL, NULL, 0);
    assert(!eb_state_add_mark(NULL, 0));

    printf("ebook state machine: all transitions ok\n");
    return 0;
}
