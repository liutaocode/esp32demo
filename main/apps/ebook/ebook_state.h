// main/apps/ebook/ebook_state.h —— 阅读器的页面状态机与翻页历史。
// 只描述"按了键之后界面该变成什么样",不做文件与网络操作,由主机测试覆盖。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EB_MAX_BOOKS   32
#define EB_MAX_MARKS   8
#define EB_NAME_LEN    64
#define EB_HISTORY_MAX 256   // 翻页历史深度,约 1 KB,够连续读 256 页再往回翻

typedef enum {
    EB_SCREEN_SHELF = 0,   // 书架
    EB_SCREEN_READER,      // 正文
    EB_SCREEN_MENU,        // 阅读菜单
    EB_SCREEN_MARKS,       // 书签列表
    EB_SCREEN_STATS,       // 阅读统计
    EB_SCREEN_XFER,        // Wi-Fi 传书
} eb_screen_t;

typedef enum { EB_FONT_S = 0, EB_FONT_M, EB_FONT_L, EB_FONT_COUNT } eb_font_t;

// 阅读主题。纯黑是反色的:底黑字白,配色表在 ebook.c 里。
typedef enum {
    EB_THEME_WHITE = 0,
    EB_THEME_EYE,
    EB_THEME_GRAY,
    EB_THEME_WOOD,
    EB_THEME_DARK,
    EB_THEME_COUNT,
} eb_theme_id_t;

typedef enum {
    EB_MENU_ADD_MARK = 0,
    EB_MENU_MARKS,
    EB_MENU_STATS,
    EB_MENU_FONT,
    EB_MENU_THEME,
    EB_MENU_BACK,
    EB_MENU_COUNT,
} eb_menu_item_t;

// 状态机要求外部执行的动作。一次按键最多产生一个。
typedef enum {
    EB_ACT_NONE = 0,
    EB_ACT_REDRAW,       // 只是选中项/页面变了
    EB_ACT_OPEN_BOOK,    // 打开 shelf_sel 指向的书,从 open_offset 开始
    EB_ACT_PAGE_NEXT,
    EB_ACT_PAGE_PREV,
    EB_ACT_PAGE_RESCAN,  // 没有来路可退,由运行时往回重新排版找上一页
    EB_ACT_RELAYOUT,     // 字号变了,当前页需要按新字号重排
    EB_ACT_THEME,        // 主题变了,配色与背光需要跟着变
    EB_ACT_ADD_MARK,
    EB_ACT_CLOSE_BOOK,   // 回到书架
    EB_ACT_XFER_START,
    EB_ACT_XFER_STOP,
} eb_action_t;

// 按键。与 bsp_btn_t / bsp_btn_ev_t 一一对应,但不引入 BSP 头文件。
typedef enum { EB_KEY_UP = 0, EB_KEY_DOWN, EB_KEY_OK } eb_key_t;
typedef enum { EB_EV_CLICK = 0, EB_EV_LONG } eb_ev_t;

typedef struct {
    eb_screen_t screen;
    eb_font_t   font;

    int book_count;
    int shelf_sel;

    int menu_sel;

    int mark_count;
    int mark_sel;
    uint32_t marks[EB_MAX_MARKS];

    // 当前页在文件中的起始偏移,以及走过来的路径(供"上一页"精确回退)。
    uint32_t page_offset;
    uint32_t history[EB_HISTORY_MAX];
    int      history_len;
    bool     history_lost;   // 历史被挤掉过,回退只能回到最早保留的一页
    bool     at_eof;         // 当前页已是全书最后一页

    eb_theme_id_t theme;     // 阅读配色

    // 阅读时长。本书与全部书各记一份,单位秒,由运行时累加与持久化。
    uint32_t read_seconds_book;
    uint32_t read_seconds_total;

    uint32_t open_offset;    // EB_ACT_OPEN_BOOK 时希望从哪里开始读
    bool     open_resume;    // true 表示忽略 open_offset,接着上次的位置读
    bool     font_changed;   // 需要外部持久化字号
    bool     theme_changed;  // 需要外部持久化主题
} eb_model_t;

void eb_state_init(eb_model_t *m, eb_font_t font, eb_theme_id_t theme);

// 书架内容刷新(首次进入或传书结束后调用)。选中项被夹回合法范围。
void eb_state_set_books(eb_model_t *m, int count);

// 一次按键。返回外部需要执行的动作。
eb_action_t eb_state_key(eb_model_t *m, eb_key_t key, eb_ev_t ev);

// 翻到下一页:把当前页压入历史,next_offset 成为当前页。
void eb_state_advance(eb_model_t *m, uint32_t next_offset);

// 渲染完一页后由运行时告知这一页是否已经到文末,决定"下一页"是否还能按。
void eb_state_set_eof(eb_model_t *m, bool at_eof);

// 直接跳到某个偏移(打开书、跳书签),清空历史。
void eb_state_seek(eb_model_t *m, uint32_t offset);

// 书签操作。add 满了会丢弃最旧的一条;返回 false 表示重复,未写入。
bool eb_state_add_mark(eb_model_t *m, uint32_t offset);
void eb_state_set_marks(eb_model_t *m, const uint32_t *marks, int count);

// 当前字号对应的字宽(像素)与行高。
int eb_font_px(eb_font_t font);
int eb_line_height(eb_font_t font);
