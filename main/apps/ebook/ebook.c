// main/apps/ebook/ebook.c —— 电子书阅读器界面。
//
// 三个执行环境分工固定,和扫码配网页一致:
//   * 按键回调 / HTTP 回调 —— 只把事件投进队列;
//   * 工作任务 eb_worker   —— 独占状态机,做所有会阻塞的事(读闪存、联网);
//   * LVGL 定时器          —— 只读工作任务发布的快照,不碰文件系统与射频。
#include "ebook.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "ebook_audio.h"
#include "ebook_fs.h"
#include "ebook_name.h"
#include "ebook_net.h"
#include "ebook_state.h"
#include "ebook_text.h"
#include "ui_pixel.h"

LV_FONT_DECLARE(ebook_zh_12);
LV_FONT_DECLARE(ebook_zh_16);
LV_FONT_DECLARE(ebook_zh_20);

static const char *TAG = "ebook";

#define EB_ROWS_VISIBLE  6
#define EB_RAW_BYTES     4096   // 一页正文最多先读这么多原始字节
#define EB_PAGE_TEXT     2048   // 排好版、带换行符的一页
#define EB_MAX_LINES     20
// 正文区有两套尺寸:刚进正文时留出标题栏和进度行,过几秒收起来铺满整屏。
#define EB_TEXT_X        10
#define EB_TEXT_Y        28
#define EB_TEXT_WIDTH    216
#define EB_TEXT_HEIGHT   264
#define EB_FULL_X        8
#define EB_FULL_Y        6
#define EB_FULL_WIDTH    224
#define EB_FULL_HEIGHT   308
#define EB_FOOT_Y        302
// 进入一个页面后按键说明显示这么久,然后让位给状态行。
#define EB_HINT_MS       4000
// 进入正文后标题栏与进度行停留这么久,然后自动进入全屏。
#define EB_CHROME_MS     2500
// 两次按键之间最多按这么久计入阅读时长。人放下机器去干别的时屏幕还亮着,
// 不封顶的话一晚上就会记成"读了十小时"。
#define EB_IDLE_CAP_S    600
// 翻页后过这么久没有新动作才把进度写进 NVS。每翻一页写一次会让 NVS 时不时
// 做一次页面整理,那一下要几百毫秒,工作任务被堵住,界面看着就是卡住了。
#define EB_SAVE_IDLE_MS  3000
#define EB_QUEUE_DEPTH   8
// 书架槽位:一格书脊加它下面那块隔板,六格正好填满页眉与页脚之间。
#define EB_SLOT_PITCH    44
#define EB_SLOT_TOP      34
#define EB_SLOT_HEIGHT   32

// 工作任务发布、LVGL 定时器读取的一份界面快照。
typedef struct {
    uint32_t    revision;
    eb_screen_t screen;
    eb_font_t   font;
    char        head[EB_NAME_LEN];
    char        foot[96];     // 常驻状态:进度、本数、剩余空间
    char        hint[80];     // 按键说明,只在刚进入页面时顶替状态行
    eb_theme_id_t theme;
    bool        full_screen;   // 正文页已经收起标题栏与页脚
    int         rows;
    int         row_sel;                       // -1 表示当前页没有选中项
    char        row_text[EB_ROWS_VISIBLE][EB_NAME_LEN];
    char        row_sub[EB_ROWS_VISIBLE][40];
    char        page[EB_PAGE_TEXT];
    char        qr[40];
    char        space[64];   // 传书页的容量行
    char        note[80];    // 传书页的最近一次结果
    bool        note_ok;
    bool        show_qr;
} eb_view_t;

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } eb_input_t;

static QueueHandle_t     s_queue;
static SemaphoreHandle_t s_view_lock;
static eb_view_t         s_view;
static eb_model_t        s_model;
static eb_book_t         s_books[EB_MAX_BOOKS];
static char              s_open_name[EB_NAME_LEN];
static size_t            s_page_bytes;
static int               s_list_top;
static bool              s_storage_ok;
static bool              s_buttons_ok;
static uint32_t          s_seen_uploads;
// 正文页是否已经收起标题栏与页脚。归工作任务管:收起来之后每页能多排几行,
// 得由它重新分页,界面只负责显示。
static bool              s_reader_full;
// 正文页开始计时的时刻(微秒),0 表示当前没在读。
static int64_t           s_read_since;
// 进度已经变了但还没落盘。离开正文、进菜单、传书前都会立刻补写。
static bool              s_progress_dirty;

static lv_obj_t *s_screen, *s_bar, *s_head, *s_batt, *s_foot;
static lv_obj_t *s_rows[EB_ROWS_VISIBLE], *s_row_text[EB_ROWS_VISIBLE], *s_row_sub[EB_ROWS_VISIBLE];
static lv_obj_t *s_page_label, *s_qr, *s_qr_box;
static lv_obj_t *s_shelf_frame, *s_boards[EB_ROWS_VISIBLE], *s_ribbons[EB_ROWS_VISIBLE];
static lv_obj_t *s_xfer_space, *s_xfer_note;
static eb_screen_t s_drawn_screen = EB_SCREEN_XFER;   // 与初始页不同,首次必定触发提示
static uint32_t  s_hint_until;
static char      s_foot_text[96], s_hint_text[80];
static lv_timer_t *s_timer, *s_batt_timer;
static uint32_t  s_drawn_revision = UINT32_MAX;
static char      s_drawn_qr[40];

static const char *MENU_TEXT[EB_MENU_COUNT] = { "添加书签", "书签列表", "阅读统计", "字号", "主题", "返回书架" };
static const char *FONT_TEXT[EB_FONT_COUNT] = { "小", "中", "大" };

// 五套阅读配色。每套都连背光一起定:长时间盯着看时,底色和背光是同一件事,
// 只换底色不动背光,暗色主题反而更晃眼。纯黑是反色的,底黑字白。
// frame/board 是书架的木框与隔板,深色主题下也得跟着暗,否则一页里两种明度。
typedef struct {
    uint32_t bg, ink, bar, sub, frame, board;
    uint8_t  backlight;
} eb_theme_t;
static const eb_theme_t THEME[EB_THEME_COUNT] = {
    [EB_THEME_WHITE] = { 0xFFFFFF, UI_INK,   UI_SKY_DARK, 0x5A6A72, 0x76502D, 0x5A3A24, 100 },
    [EB_THEME_EYE]   = { 0xF2E2C4, 0x3B2E22, 0x8A6A3A,    0x6B5B45, 0x8A6A3A, 0x6B4F28, 65  },
    [EB_THEME_GRAY]  = { 0xD6D8D5, 0x22262A, 0x5F6A70,    0x4A5257, 0x6E6A62, 0x4E4B45, 80  },
    [EB_THEME_WOOD]  = { 0xC8A272, 0x2E2114, 0x7A5327,    0x4A3A22, 0x7A5327, 0x5A3A18, 75  },
    [EB_THEME_DARK]  = { 0x000000, 0xE6E6E6, 0x1A1A1A,    0x8A8A8A, 0x2A2A2A, 0x1A1A1A, 45  },
};
static const char *THEME_TEXT[EB_THEME_COUNT] = { "纯白", "护眼", "灰色", "木色", "纯黑" };

// ---------------------------------------------------------------- 事件投递

void ebook_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_queue) return;
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) return;
    eb_input_t input = { .button = btn, .event = ev };
    xQueueSend(s_queue, &input, 0);
}

// ---------------------------------------------------------------- 排版

static const lv_font_t *font_of(eb_font_t font)
{
    switch (font) {
    case EB_FONT_S: return &ebook_zh_12;
    case EB_FONT_L: return &ebook_zh_20;
    default:        return &ebook_zh_16;
    }
}

static int lines_per_page(eb_font_t font, bool full)
{
    int count = (full ? EB_FULL_HEIGHT : EB_TEXT_HEIGHT) / eb_line_height(font);
    return count > EB_MAX_LINES ? EB_MAX_LINES : count;
}

// 读一页原始字节,排好版写进快照。只在工作任务里调用。
static void build_page(eb_view_t *view)
{
    static char raw[EB_RAW_BYTES];
    static eb_line_t lines[EB_MAX_LINES];

    view->page[0] = '\0';
    s_page_bytes = 0;

    uint32_t total = eb_fs_size();
    if (total == 0) {
        snprintf(view->page, sizeof(view->page), "这本书打不开了。\n\n长按确定回书架。");
        eb_state_set_eof(&s_model, true);
        return;
    }

    size_t got = eb_fs_read(s_model.page_offset, raw, sizeof(raw));
    bool tail = (s_model.page_offset + got >= total);
    eb_layout_t layout = {
        .width_px = view->full_screen ? EB_FULL_WIDTH : EB_TEXT_WIDTH,
        .glyph_px = eb_font_px(s_model.font),
        .lines = lines_per_page(s_model.font, view->full_screen),
    };
    int count = 0;
    s_page_bytes = eb_wrap(raw, got, tail, &layout, lines, EB_MAX_LINES, &count);

    size_t used = 0;
    for (int i = 0; i < count; i++) {
        size_t need = lines[i].bytes + 1;
        if (used + need >= sizeof(view->page)) break;
        memcpy(view->page + used, raw + lines[i].offset, lines[i].bytes);
        used += lines[i].bytes;
        view->page[used++] = '\n';
    }
    view->page[used] = '\0';

    eb_state_set_eof(&s_model, tail && s_model.page_offset + s_page_bytes >= total);
}

// ---------------------------------------------------------------- 快照发布

static void fill_list_window(eb_view_t *view, int count, int sel)
{
    if (sel < s_list_top) s_list_top = sel;
    if (sel >= s_list_top + EB_ROWS_VISIBLE) s_list_top = sel - EB_ROWS_VISIBLE + 1;
    if (s_list_top > count - EB_ROWS_VISIBLE) s_list_top = count - EB_ROWS_VISIBLE;
    if (s_list_top < 0) s_list_top = 0;

    view->rows = count - s_list_top;
    if (view->rows > EB_ROWS_VISIBLE) view->rows = EB_ROWS_VISIBLE;
    if (view->rows < 0) view->rows = 0;
    view->row_sel = (count > 0) ? sel - s_list_top : -1;
}

static void publish(void)
{
    xSemaphoreTake(s_view_lock, portMAX_DELAY);
    eb_view_t *view = &s_view;
    view->screen = s_model.screen;
    view->font = s_model.font;
    view->rows = 0;
    view->row_sel = -1;
    view->show_qr = false;
    view->page[0] = '\0';
    view->hint[0] = '\0';
    view->theme = s_model.theme;
    view->full_screen = s_reader_full && s_model.screen == EB_SCREEN_READER;

    switch (s_model.screen) {
    case EB_SCREEN_SHELF: {
        snprintf(view->head, sizeof(view->head), "我的书架");
        fill_list_window(view, s_model.book_count, s_model.shelf_sel);
        for (int i = 0; i < view->rows; i++) {
            const eb_book_t *book = &s_books[s_list_top + i];
            eb_name_title(book->name, view->row_text[i], sizeof(view->row_text[i]));
            // 书脊上直接给读到几成,不然"接着上次读"这件事在书架上看不出来。
            uint32_t resume = 0;
            eb_store_load_progress(book->name, &resume, NULL);
            char size[24];
            eb_format_size(book->size, size, sizeof(size));
            // 副位只有一小格:读过的书显示进度,没读过的显示大小。
            if (resume > 0 && book->size > 0) {
                snprintf(view->row_sub[i], sizeof(view->row_sub[i]), "%d%%",
                         (int)((uint64_t)resume * 100 / book->size));
            } else {
                snprintf(view->row_sub[i], sizeof(view->row_sub[i]), "%s", size);
            }
        }
        if (!s_storage_ok) {
            snprintf(view->foot, sizeof(view->foot), "存储不可用,请重新烧录固件");
        } else {
            // 剩余空间在空书架上也要显示:传之前就该知道还能放多大的书。
            uint64_t total = 0, freespace = 0;
            eb_fs_usage(&total, &freespace);
            char have[24];
            eb_format_size(freespace, have, sizeof(have));
            if (s_model.book_count == 0) {
                snprintf(view->foot, sizeof(view->foot), "书架是空的 · 余 %s", have);
            } else {
                snprintf(view->foot, sizeof(view->foot), "%d 本 · 余 %s", s_model.book_count, have);
            }
        }
        snprintf(view->hint, sizeof(view->hint), "上下选书 · 确定打开 · 长按确定去传书");
        break;
    }
    case EB_SCREEN_READER: {
        char title[EB_NAME_LEN];
        eb_name_title(s_open_name, title, sizeof(title));
        uint32_t total = eb_fs_size();
        int percent = total ? (int)((uint64_t)s_model.page_offset * 100 / total) : 0;
        snprintf(view->head, sizeof(view->head), "%s", title);
        // 这一行只在正文刚打开的两秒多里出现,所以进度和按键说明写在一起。
        snprintf(view->foot, sizeof(view->foot), "%d%%%s · 上下翻页 · 确定菜单",
                 percent, s_model.at_eof ? " 完" : "");
        build_page(view);
        break;
    }
    case EB_SCREEN_MENU:
        snprintf(view->head, sizeof(view->head), "阅读菜单");
        view->rows = EB_MENU_COUNT;
        view->row_sel = s_model.menu_sel;
        for (int i = 0; i < EB_MENU_COUNT; i++) {
            snprintf(view->row_text[i], sizeof(view->row_text[i]), "%s", MENU_TEXT[i]);
            view->row_sub[i][0] = '\0';
        }
        snprintf(view->row_sub[EB_MENU_FONT], sizeof(view->row_sub[EB_MENU_FONT]),
                 "%s", FONT_TEXT[s_model.font]);
        snprintf(view->row_sub[EB_MENU_MARKS], sizeof(view->row_sub[EB_MENU_MARKS]),
                 "%d 条", s_model.mark_count);
        snprintf(view->row_sub[EB_MENU_THEME], sizeof(view->row_sub[EB_MENU_THEME]),
                 "%s", THEME_TEXT[s_model.theme]);
        eb_format_duration(s_model.read_seconds_book,
                           view->row_sub[EB_MENU_STATS], sizeof(view->row_sub[EB_MENU_STATS]));
        view->foot[0] = '\0';
        snprintf(view->hint, sizeof(view->hint), "上下选择 · 确定执行 · 长按确定回正文");
        break;
    case EB_SCREEN_STATS: {
        char title[EB_NAME_LEN];
        eb_name_title(s_open_name, title, sizeof(title));
        uint32_t total = eb_fs_size();
        int percent = total ? (int)((uint64_t)s_model.page_offset * 100 / total) : 0;

        snprintf(view->head, sizeof(view->head), "阅读统计");
        view->rows = 4;
        view->row_sel = -1;
        snprintf(view->row_text[0], sizeof(view->row_text[0]), "%s", title);
        snprintf(view->row_sub[0], sizeof(view->row_sub[0]), "%d%%", percent);
        snprintf(view->row_text[1], sizeof(view->row_text[1]), "这本读了");
        eb_format_duration(s_model.read_seconds_book, view->row_sub[1], sizeof(view->row_sub[1]));
        snprintf(view->row_text[2], sizeof(view->row_text[2]), "累计阅读");
        eb_format_duration(s_model.read_seconds_total, view->row_sub[2], sizeof(view->row_sub[2]));
        snprintf(view->row_text[3], sizeof(view->row_text[3]), "书签");
        snprintf(view->row_sub[3], sizeof(view->row_sub[3]), "%d 条", s_model.mark_count);
        snprintf(view->hint, sizeof(view->hint), "长按确定回菜单");
        break;
    }
    case EB_SCREEN_MARKS:
        snprintf(view->head, sizeof(view->head), "书签");
        fill_list_window(view, s_model.mark_count, s_model.mark_sel);
        for (int i = 0; i < view->rows; i++) {
            uint32_t offset = s_model.marks[s_list_top + i];
            uint32_t total = eb_fs_size();
            snprintf(view->row_text[i], sizeof(view->row_text[i]), "第 %d 条",
                     s_list_top + i + 1);
            snprintf(view->row_sub[i], sizeof(view->row_sub[i]), "%d%%",
                     total ? (int)((uint64_t)offset * 100 / total) : 0);
        }
        view->foot[0] = '\0';
        snprintf(view->hint, sizeof(view->hint), "确定跳转 · 长按确定返回菜单");
        break;
    default: {
        eb_net_status_t net;
        eb_net_status(&net);
        snprintf(view->head, sizeof(view->head), "Wi-Fi 传书");
        view->rows = 2;
        view->row_sel = -1;
        // 热点名和网址是这一页最要紧的两串字,各自独占一行的主位,
        // 说明文字降到副行。它们比书名长得多,挤在两列布局里会被截断。
        switch (net.state) {
        case EB_NET_CONNECTING:
            snprintf(view->row_text[0], sizeof(view->row_text[0]), "%s", net.ssid);
            snprintf(view->row_sub[0], sizeof(view->row_sub[0]), "正在连接,请稍候");
            view->rows = 1;
            break;
        case EB_NET_STA_READY:
            snprintf(view->row_text[0], sizeof(view->row_text[0]), "%s", net.ssid);
            snprintf(view->row_sub[0], sizeof(view->row_sub[0]), "已连上这个网络");
            snprintf(view->row_text[1], sizeof(view->row_text[1]), "%s", net.url);
            snprintf(view->row_sub[1], sizeof(view->row_sub[1]), "手机在同一网络下打开");
            break;
        case EB_NET_AP_READY:
            snprintf(view->row_text[0], sizeof(view->row_text[0]), "%s", net.ssid);
            snprintf(view->row_sub[0], sizeof(view->row_sub[0]), "本机热点,无密码");
            snprintf(view->row_text[1], sizeof(view->row_text[1]), "%s", net.url);
            snprintf(view->row_sub[1], sizeof(view->row_sub[1]), "连上热点后打开");
            break;
        case EB_NET_FAILED:
            snprintf(view->row_text[0], sizeof(view->row_text[0]), "无线功能无法启动");
            view->row_sub[0][0] = '\0';
            view->rows = 1;
            break;
        default:
            snprintf(view->row_text[0], sizeof(view->row_text[0]), "正在启动无线");
            view->row_sub[0][0] = '\0';
            view->rows = 1;
            break;
        }
        if (net.url[0]) {
            snprintf(view->qr, sizeof(view->qr), "%s", net.url);
            view->show_qr = true;
        }
        // 容量和上一本书的结果都放在二维码下面,不用盯着手机看结果。
        if (net.total > 0) {
            char have[24], all[24];
            eb_format_size(net.freespace, have, sizeof(have));
            eb_format_size(net.total, all, sizeof(all));
            snprintf(view->space, sizeof(view->space), "剩余 %s / %s", have, all);
        }
        snprintf(view->note, sizeof(view->note), "%s", net.note);
        view->note_ok = net.note_ok;
        view->foot[0] = '\0';
        snprintf(view->hint, sizeof(view->hint), "长按确定结束传书");
        break;
    }
    }

    view->revision++;
    xSemaphoreGive(s_view_lock);
}

// ---------------------------------------------------------------- 工作任务

// 阅读计时:进正文开始,离开正文或按下任意键时结算一次。按键结算让"封顶"
// 变成两次按键之间的间隔,于是放下不管的那段时间最多只记 EB_IDLE_CAP_S。
static void reading_start(void)
{
    s_read_since = esp_timer_get_time();
}

static void reading_stop(void)
{
    if (!s_read_since) return;
    int64_t seconds = (esp_timer_get_time() - s_read_since) / 1000000;
    s_read_since = 0;
    if (seconds <= 0) return;
    if (seconds > EB_IDLE_CAP_S) seconds = EB_IDLE_CAP_S;
    s_model.read_seconds_book += (uint32_t)seconds;
    s_model.read_seconds_total += (uint32_t)seconds;
    s_progress_dirty = true;   // 时长也要落盘,和阅读位置写在同一条记录里
}

// 每次翻页和每次离开正文都落盘:这是块电池设备,随时可能没电或被长按关机,
// 攒着不写等于把"读到哪儿了"押在下一次正常退出上。
// 只标记,不写盘:翻页要立刻出下一页,不能停下来等闪存。
static void mark_progress_dirty(void)
{
    s_progress_dirty = true;
}

static void save_progress(void)
{
    if (!s_progress_dirty || !s_open_name[0]) return;
    s_progress_dirty = false;
    eb_store_save_progress(s_open_name, s_model.page_offset, s_model.read_seconds_book);
    eb_store_save_total_seconds(s_model.read_seconds_total);
}

static void reload_books(void)
{
    int count = s_storage_ok ? eb_fs_list(s_books, EB_MAX_BOOKS) : 0;
    eb_state_set_books(&s_model, count);
}

static void open_current_book(void)
{
    if (s_model.shelf_sel >= s_model.book_count) return;
    const char *name = s_books[s_model.shelf_sel].name;
    uint32_t resume = 0, seconds = 0;
    if (strcmp(name, s_open_name) != 0) {
        snprintf(s_open_name, sizeof(s_open_name), "%s", name);
        eb_fs_close();
        if (!eb_fs_open(s_open_name)) {
            s_open_name[0] = '\0';
            s_model.screen = EB_SCREEN_SHELF;
            return;
        }
        uint32_t marks[EB_MAX_MARKS];
        int count = eb_store_load_marks(s_open_name, marks, EB_MAX_MARKS);
        eb_state_set_marks(&s_model, marks, count);
        eb_store_load_progress(s_open_name, &resume, &seconds);
        s_model.read_seconds_book = seconds;
    } else {
        eb_store_load_progress(s_open_name, &resume, NULL);
    }

    uint32_t offset = s_model.open_offset;
    if (s_model.open_resume) {
        // 存档可能是上一版更短的同名书留下的,越界就当没读过,别开在文件外面。
        offset = (resume < eb_fs_size()) ? resume : 0;
    }
    eb_state_seek(&s_model, offset);
}

static void run_action(eb_action_t action)
{
    switch (action) {
    case EB_ACT_OPEN_BOOK:
        open_current_book();
        break;
    case EB_ACT_PAGE_NEXT:
        // s_page_bytes 是上一次排版实际消耗的字节数,下一页从那里开始。
        if (s_page_bytes > 0) eb_state_advance(&s_model, s_model.page_offset + (uint32_t)s_page_bytes);
        eb_audio_play(EB_SOUND_PAGE_NEXT);
        mark_progress_dirty();
        break;
    case EB_ACT_PAGE_RESCAN: {
        // 没有来路:把当前页之前的一段读回来重新排版,找出上一页的起点。
        // 窗口取三页左右,重排的页数越少,和真实分页的偏差越小。
        static char back[EB_RAW_BYTES];
        size_t want = s_page_bytes > 0 ? s_page_bytes * 3 : 1024;
        if (want < 1024) want = 1024;
        if (want > sizeof(back)) want = sizeof(back);
        if (want > s_model.page_offset) want = s_model.page_offset;
        uint32_t from = s_model.page_offset - (uint32_t)want;
        size_t got = eb_fs_read(from, back, want);
        eb_layout_t layout = {
            .width_px = s_reader_full ? EB_FULL_WIDTH : EB_TEXT_WIDTH,
            .glyph_px = eb_font_px(s_model.font),
            .lines = lines_per_page(s_model.font, s_reader_full),
        };
        size_t start = eb_prev_page_start(back, got, from == 0, &layout);
        s_model.page_offset = from + (uint32_t)start;
        s_model.at_eof = false;
        eb_audio_play(EB_SOUND_PAGE_PREV);
        mark_progress_dirty();
        break;
    }
    case EB_ACT_PAGE_PREV:
        // 状态机已经把页码退回去了,这里只补上声音。
        eb_audio_play(EB_SOUND_PAGE_PREV);
        mark_progress_dirty();
        break;
    case EB_ACT_ADD_MARK:
        if (eb_state_add_mark(&s_model, s_model.page_offset)) {
            eb_store_save_marks(s_open_name, s_model.marks, s_model.mark_count);
        }
        break;
    case EB_ACT_RELAYOUT:
        if (s_model.font_changed) {
            eb_store_save_font(s_model.font);
            s_model.font_changed = false;
        }
        break;
    case EB_ACT_THEME:
        if (s_model.theme_changed) {
            eb_store_save_theme(s_model.theme);
            s_model.theme_changed = false;
        }
        break;
    case EB_ACT_CLOSE_BOOK:
        save_progress();
        reload_books();
        break;
    case EB_ACT_XFER_START:
        if (!eb_net_start()) ESP_LOGE(TAG, "wi-fi start failed");
        break;
    case EB_ACT_XFER_STOP:
        eb_net_stop();
        reload_books();
        s_seen_uploads = 0;
        break;
    default:
        break;
    }
}

static void eb_worker(void *arg)
{
    (void)arg;
    reload_books();
    s_model.read_seconds_total = eb_store_load_total_seconds();
    publish();

    for (;;) {
        eb_input_t input;
        // 传书页要定期刷新联网状态;正文页在收起标题栏之前要等那几秒。
        // 其余情况老老实实阻塞,不做无谓的轮询。
        TickType_t wait = portMAX_DELAY;
        if (s_model.screen == EB_SCREEN_XFER) wait = pdMS_TO_TICKS(400);
        else if (s_model.screen == EB_SCREEN_READER && !s_reader_full) wait = pdMS_TO_TICKS(EB_CHROME_MS);
        else if (s_progress_dirty) wait = pdMS_TO_TICKS(EB_SAVE_IDLE_MS);

        if (xQueueReceive(s_queue, &input, wait) == pdTRUE) {
            eb_screen_t before = s_model.screen;
            eb_key_t key = (input.button == BSP_BTN_UP) ? EB_KEY_UP
                         : (input.button == BSP_BTN_DOWN) ? EB_KEY_DOWN : EB_KEY_OK;
            eb_ev_t ev = (input.event == BSP_BTN_LONG) ? EB_EV_LONG : EB_EV_CLICK;
            eb_action_t action = eb_state_key(&s_model, key, ev);
            if (action == EB_ACT_NONE) {
                // 正文页翻不动(第一页往上、末页往下)时全屏状态下毫无反应,
                // 看着就像死机。把标题栏和进度行亮回来,至少告诉他在哪一页。
                if (s_model.screen == EB_SCREEN_READER && s_reader_full) {
                    s_reader_full = false;
                    publish();
                }
                continue;
            }
            // 结算到这一刻为止的阅读时长,再执行动作:动作可能换书或退出正文。
            if (before == EB_SCREEN_READER) reading_stop();
            run_action(action);
            if (before == EB_SCREEN_READER && s_model.screen != EB_SCREEN_READER) save_progress();
            if (s_model.screen == EB_SCREEN_READER) reading_start();
            // 每次重新进入正文都先把标题栏和进度亮出来:刚从菜单出来的人
            // 正想确认改动生效了没有,翻页途中则不再打扰。
            if (s_model.screen == EB_SCREEN_READER && before != EB_SCREEN_READER) s_reader_full = false;
            publish();
            continue;
        }

        if (s_model.screen == EB_SCREEN_READER) {
            // 等够了:收起标题栏与页脚,按新的行数把这一页重排一遍。
            // 页首偏移不变,所以文字只会往下多填几行,不会跳位。
            if (!s_reader_full) {
                s_reader_full = true;
                publish();
                continue;
            }
            // 手停下来了,这时候写盘不会挡住任何一次翻页。
            save_progress();
            continue;
        }
        if (s_model.screen != EB_SCREEN_XFER) continue;
        eb_net_status_t net;
        eb_net_status(&net);
        if (net.uploads != s_seen_uploads) {
            // 手机传完一本书就立刻把书架读一遍,离开传书页时不必再等。
            s_seen_uploads = net.uploads;
            reload_books();
        }
        publish();
    }
}

// ---------------------------------------------------------------- 界面绘制

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static void show(lv_obj_t *obj, bool visible)
{
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void render(const eb_view_t *view)
{
    const eb_theme_t *theme = &THEME[view->theme < EB_THEME_COUNT ? view->theme : EB_THEME_WHITE];
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(theme->bg), 0);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(theme->bar), 0);
    lv_obj_set_style_text_color(s_page_label, lv_color_hex(theme->ink), 0);
    lv_obj_set_style_text_color(s_foot, lv_color_hex(theme->sub), 0);
    lv_obj_set_style_text_color(s_xfer_space, lv_color_hex(theme->ink), 0);
    // 背光只在真的变了的时候调:render 每次重绘都会走到这里。
    static uint8_t drawn_backlight;
    if (drawn_backlight != theme->backlight) {
        drawn_backlight = theme->backlight;
        bsp_display_backlight(theme->backlight);
    }

    lv_label_set_text(s_head, view->head);

    bool reading = (view->screen == EB_SCREEN_READER);
    if (reading) {
        // 全屏时正文铺满整块屏幕,标题栏与页脚整个让位,不是只把字往上挪。
        if (view->full_screen) {
            lv_obj_set_pos(s_page_label, EB_FULL_X, EB_FULL_Y);
            lv_obj_set_size(s_page_label, EB_FULL_WIDTH + 4, EB_FULL_HEIGHT);
        } else {
            lv_obj_set_pos(s_page_label, EB_TEXT_X, EB_TEXT_Y);
            lv_obj_set_size(s_page_label, EB_TEXT_WIDTH + 4, EB_TEXT_HEIGHT);
        }
        lv_obj_set_style_text_font(s_page_label, font_of(view->font), 0);
        lv_obj_set_style_text_line_space(s_page_label,
                                         eb_line_height(view->font) - eb_font_px(view->font), 0);
        lv_label_set_text(s_page_label, view->page);
    }
    show(s_page_label, reading);
    show(s_bar, !view->full_screen);
    show(s_foot, !view->full_screen);

    // 书架页画成一个真的架子:木框、六块隔板,书是躺在隔板上的书脊。
    // 没有书时架子仍然在,空的是格子而不是整块屏幕。
    bool shelf = (view->screen == EB_SCREEN_SHELF);
    bool transferring = (view->screen == EB_SCREEN_XFER);
    show(s_shelf_frame, shelf);
    lv_obj_set_style_bg_color(s_shelf_frame, lv_color_hex(theme->frame), 0);
    for (int i = 0; i < EB_ROWS_VISIBLE; i++) {
        show(s_boards[i], shelf);
        lv_obj_set_style_bg_color(s_boards[i], lv_color_hex(theme->board), 0);
    }

    for (int i = 0; i < EB_ROWS_VISIBLE; i++) {
        show(s_rows[i], i < view->rows);
        show(s_ribbons[i], shelf && i < view->rows && i == view->row_sel);
        if (i >= view->rows) continue;

        bool selected = (i == view->row_sel);
        // 书脊按槽位轮换颜色,一眼能看出书架上有几本、翻到哪一本;
        // 菜单和传书页复用同一批行,那里就回到普通白底面板。
        static const uint32_t SPINE[] = { 0xC2452F, 0x1F6FB2, 0x5E8C3A, 0xC98A2B, 0x8A5A9E, 0x2F7F79 };
        uint32_t fill = shelf ? SPINE[i % (sizeof(SPINE) / sizeof(SPINE[0]))]
                              : (selected ? UI_YELLOW : theme->bg);
        lv_obj_set_style_bg_color(s_rows[i], lv_color_hex(fill), 0);
        // 纯黑主题下墨色边框和底色一样黑,框就没了,所以边框跟着字色走。
        lv_obj_set_style_border_color(s_rows[i], lv_color_hex(shelf ? UI_INK : theme->ink), 0);
        lv_obj_set_style_border_width(s_rows[i], selected ? 3 : 1, 0);
        lv_obj_set_style_text_color(s_row_text[i], lv_color_hex(shelf ? 0xFFFFFF : theme->ink), 0);
        lv_obj_set_style_text_color(s_row_sub[i], lv_color_hex(shelf ? 0xF0E6D2 : theme->sub), 0);

        // 传书页的主字是热点名和网址,必须整串看得见,所以换成上下两行、
        // 占满整行宽度;书架和菜单的字短,保持左右两列更好读。
        if (transferring) {
            lv_obj_set_pos(s_row_text[i], 10, 1);
            lv_obj_set_width(s_row_text[i], 190);
            lv_obj_set_pos(s_row_sub[i], 10, 18);
            lv_obj_set_width(s_row_sub[i], 190);
            lv_obj_set_style_text_align(s_row_sub[i], LV_TEXT_ALIGN_LEFT, 0);
        } else {
            lv_obj_set_pos(s_row_text[i], shelf ? 22 : 14, 7);
            lv_obj_set_width(s_row_text[i], shelf ? 122 : 112);
            lv_obj_set_pos(s_row_sub[i], shelf ? 148 : 132, 10);
            // 书架副位只放 "43%",统计页要放 "1 小时 12 分",宽度不一样。
            lv_obj_set_width(s_row_sub[i], shelf ? 56 : 76);
            lv_obj_set_style_text_align(s_row_sub[i], LV_TEXT_ALIGN_RIGHT, 0);
        }
        lv_label_set_text(s_row_text[i], view->row_text[i]);
        lv_label_set_text(s_row_sub[i], view->row_sub[i]);
    }

    show(s_xfer_space, transferring && view->space[0]);
    show(s_xfer_note, transferring && view->note[0]);
    if (transferring) {
        lv_label_set_text(s_xfer_space, view->space);
        lv_label_set_text(s_xfer_note, view->note);
        lv_obj_set_style_text_color(s_xfer_note,
                                    lv_color_hex(view->note_ok ? theme->ink : UI_RED), 0);
    }

    if (view->show_qr && view->qr[0]) {
        if (strcmp(s_drawn_qr, view->qr) != 0) {
            if (lv_qrcode_update(s_qr, view->qr, strlen(view->qr)) == LV_RESULT_OK) {
                snprintf(s_drawn_qr, sizeof(s_drawn_qr), "%s", view->qr);
            } else {
                // 编码失败就别留一块白方块骗人扫,下面的网址仍然可以手输。
                s_drawn_qr[0] = '\0';
                lv_obj_add_flag(s_qr_box, LV_OBJ_FLAG_HIDDEN);
                return;
            }
        }
        show(s_qr_box, true);
    } else {
        show(s_qr_box, false);
    }
}

// 页脚只有一行位置:刚进入一个页面时显示按键说明,几秒后让位给状态行。
// 按键说明常驻既占地方,又和正文最后一行贴在一起,尤其是小字号排得更满。
static void refresh_footer(void)
{
    if (lv_obj_has_flag(s_foot, LV_OBJ_FLAG_HIDDEN)) return;
    bool showing_hint = s_hint_text[0] && (int32_t)(s_hint_until - lv_tick_get()) > 0;
    const char *text = showing_hint ? s_hint_text : s_foot_text;
    if (strcmp(lv_label_get_text(s_foot), text) != 0) lv_label_set_text(s_foot, text);
}

static void on_tick(lv_timer_t *timer)
{
    (void)timer;
    static eb_view_t snapshot;
    xSemaphoreTake(s_view_lock, portMAX_DELAY);
    bool fresh = (s_view.revision != s_drawn_revision);
    if (fresh) {
        snapshot = s_view;
        s_drawn_revision = s_view.revision;
    }
    xSemaphoreGive(s_view_lock);

    if (fresh) {
        if (snapshot.screen != s_drawn_screen) {
            s_drawn_screen = snapshot.screen;
            s_hint_until = lv_tick_get() + EB_HINT_MS;
        }
        snprintf(s_foot_text, sizeof(s_foot_text), "%s", snapshot.foot);
        snprintf(s_hint_text, sizeof(s_hint_text), "%s", snapshot.hint);
        render(&snapshot);
    }
    refresh_footer();
}

static void on_battery(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_batt, "");
    else lv_label_set_text_fmt(s_batt, "%d%%", soc);
}

// ---------------------------------------------------------------- 生命周期

void ebook_prepare(void)
{
    s_storage_ok = eb_fs_mount();
    if (!s_storage_ok) ESP_LOGE(TAG, "books partition unavailable");
    if (!eb_net_prepare()) ESP_LOGE(TAG, "network stack unavailable");
    eb_audio_prepare();
    eb_state_init(&s_model, eb_store_load_font(), eb_store_load_theme());
}

void ebook_enter(bool buttons_ok)
{
    s_buttons_ok = buttons_ok;

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(UI_PAPER), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    s_bar = block(s_screen, 0, 0, 240, 22, UI_SKY_DARK);
    lv_obj_t *bar = s_bar;
    s_head = ui_pixel_label(bar, "", &ebook_zh_16, 0xFFFFFF);
    lv_obj_set_pos(s_head, 6, 2);
    lv_label_set_long_mode(s_head, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_head, 170);
    s_batt = ui_pixel_label(bar, "", &ebook_zh_12, 0xFFFFFF);
    lv_obj_set_pos(s_batt, 196, 4);

    // 正文:自己算好换行,交给 LVGL 时按 CLIP 显示,免得它再断一次行。
    s_page_label = ui_pixel_label(s_screen, "", &ebook_zh_16, UI_INK);
    lv_obj_set_pos(s_page_label, EB_TEXT_X, EB_TEXT_Y);
    lv_obj_set_size(s_page_label, EB_TEXT_WIDTH + 4, EB_TEXT_HEIGHT);
    lv_label_set_long_mode(s_page_label, LV_LABEL_LONG_CLIP);
    lv_obj_add_flag(s_page_label, LV_OBJ_FLAG_HIDDEN);

    // 先建木框,再建隔板,最后建书脊:LVGL 按创建顺序叠放,书必须压在隔板上面。
    s_shelf_frame = block(s_screen, 4, 26, 232, 272, 0x76502D);
    lv_obj_set_style_border_color(s_shelf_frame, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(s_shelf_frame, 4, 0);
    lv_obj_add_flag(s_shelf_frame, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < EB_ROWS_VISIBLE; i++) {
        int slot_y = EB_SLOT_TOP + i * EB_SLOT_PITCH;
        s_boards[i] = block(s_screen, 8, slot_y + EB_SLOT_HEIGHT, 224, 6, 0x5A3A24);
        lv_obj_add_flag(s_boards[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < EB_ROWS_VISIBLE; i++) {
        int slot_y = EB_SLOT_TOP + i * EB_SLOT_PITCH;
        s_rows[i] = block(s_screen, 14, slot_y, 212, EB_SLOT_HEIGHT, 0xFFFFFF);
        lv_obj_set_style_border_color(s_rows[i], lv_color_hex(UI_INK), 0);
        lv_obj_set_style_border_width(s_rows[i], 1, 0);
        s_row_text[i] = ui_pixel_label(s_rows[i], "", &ebook_zh_16, UI_INK);
        // 书名从 x=22 起,给书签带留出位置,否则选中那本的第一个字被压掉一角。
        lv_obj_set_pos(s_row_text[i], 22, 7);
        lv_label_set_long_mode(s_row_text[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_row_text[i], 122);
        s_row_sub[i] = ui_pixel_label(s_rows[i], "", &ebook_zh_12, 0x5A6A72);
        lv_obj_set_pos(s_row_sub[i], 148, 10);
        lv_obj_set_width(s_row_sub[i], 56);
        lv_obj_set_style_text_align(s_row_sub[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);

    }

    // 书签带最后建,才压得住书脊。它挂在屏幕上而不是书脊里:LVGL 默认把子对象
    // 裁到父对象范围内,挂在书脊里就露不出上下那两截。
    for (int i = 0; i < EB_ROWS_VISIBLE; i++) {
        int slot_y = EB_SLOT_TOP + i * EB_SLOT_PITCH;
        s_ribbons[i] = block(s_screen, 20, slot_y - 4, 10, EB_SLOT_HEIGHT + 8, UI_YELLOW);
        lv_obj_set_style_border_color(s_ribbons[i], lv_color_hex(UI_INK), 0);
        lv_obj_set_style_border_width(s_ribbons[i], 2, 0);
        lv_obj_add_flag(s_ribbons[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_qr_box = block(s_screen, 60, 118, 120, 120, 0xFFFFFF);
    s_qr = lv_qrcode_create(s_qr_box);
    lv_qrcode_set_size(s_qr, 112);
    lv_qrcode_set_dark_color(s_qr, lv_color_hex(UI_INK));
    lv_qrcode_set_light_color(s_qr, lv_color_hex(0xFFFFFF));
    lv_obj_center(s_qr);
    lv_obj_add_flag(s_qr_box, LV_OBJ_FLAG_HIDDEN);

    // 二维码下面的两行:容量,以及最近一本书的结果。
    s_xfer_space = ui_pixel_label(s_screen, "", &ebook_zh_16, UI_INK);
    lv_obj_set_pos(s_xfer_space, 8, 246);
    lv_obj_set_width(s_xfer_space, 224);
    lv_obj_set_style_text_align(s_xfer_space, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(s_xfer_space, LV_OBJ_FLAG_HIDDEN);

    s_xfer_note = ui_pixel_label(s_screen, "", &ebook_zh_12, UI_INK);
    lv_obj_set_pos(s_xfer_note, 8, 270);
    lv_obj_set_width(s_xfer_note, 224);
    lv_label_set_long_mode(s_xfer_note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_xfer_note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(s_xfer_note, LV_OBJ_FLAG_HIDDEN);

    s_foot = ui_pixel_label(s_screen, "", &ebook_zh_12, 0x5A6A72);
    lv_obj_set_pos(s_foot, 8, EB_FOOT_Y);
    lv_obj_set_width(s_foot, 224);
    lv_obj_set_style_text_align(s_foot, LV_TEXT_ALIGN_CENTER, 0);

    lv_screen_load(s_screen);
    eb_audio_active(true);

    s_view_lock = xSemaphoreCreateMutex();
    s_queue = xQueueCreate(EB_QUEUE_DEPTH, sizeof(eb_input_t));
    if (!s_view_lock || !s_queue) {
        lv_label_set_text(s_foot, "内存不足,阅读器无法启动");
        return;
    }
    if (!s_buttons_ok) lv_label_set_text(s_foot, "按键不可用");

    s_timer = lv_timer_create(on_tick, 120, NULL);
    s_batt_timer = lv_timer_create(on_battery, 20000, NULL);
    on_battery(NULL);

    // 读闪存与联网都在工作任务里做,LVGL 任务不会被文件 I/O 卡住。
    if (xTaskCreate(eb_worker, "eb_worker", 5120, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "worker task creation failed");
        lv_label_set_text(s_foot, "内存不足,阅读器无法启动");
    }
}
