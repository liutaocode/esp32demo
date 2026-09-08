// main/apps/ebook/ebook_text.c —— 见 ebook_text.h。纯 C,不碰硬件。
#include "ebook_text.h"

#include <stdio.h>

// 不能出现在行首的标点。命中时允许它挤在上一行末尾,即最基本的避头尾,
// 否则中文正文每隔几行就会出现孤零零的句号。每行只让一个字符越界。
static bool is_no_line_start(uint32_t cp)
{
    static const uint32_t table[] = {
        0x3002, 0xFF0C, 0x3001, 0xFF1B, 0xFF1A, 0xFF1F, 0xFF01,  // 。，、；：？！
        0x300B, 0x300D, 0x300F, 0xFF09, 0x3011, 0x3015, 0xFF5D,  // 》」』）】〕｝
        0x2026, 0x2014, 0xFF5E, 0x00B7, 0x2019, 0x201D,          // …—～·’”
        ',', '.', ';', ':', '?', '!', ')', ']', '}',
    };
    for (size_t k = 0; k < sizeof(table) / sizeof(table[0]); k++) {
        if (table[k] == cp) return true;
    }
    return false;
}

bool eb_utf8_next(const char *buf, size_t len, size_t *i, uint32_t *cp)
{
    if (!buf || !i || !cp || *i >= len) return false;
    const unsigned char *p = (const unsigned char *)buf;
    unsigned char lead = p[*i];
    size_t need;
    uint32_t value;
    if (lead < 0x80) {
        need = 1; value = lead;
    } else if ((lead & 0xE0) == 0xC0) {
        need = 2; value = lead & 0x1Fu;
    } else if ((lead & 0xF0) == 0xE0) {
        need = 3; value = lead & 0x0Fu;
    } else if ((lead & 0xF8) == 0xF0) {
        need = 4; value = lead & 0x07u;
    } else {
        // 续字节或非法前导字节:吞掉一个字节,避免整页排版卡死在这里。
        *i += 1; *cp = 0xFFFD; return true;
    }
    // 先看已有的续字节:缺续字节是"数据坏了",而不是"数据没读完",
    // 否则一段非法字节会让分页永远停在同一个偏移上等更多数据。
    for (size_t k = 1; k < need && *i + k < len; k++) {
        if ((p[*i + k] & 0xC0) != 0x80) { *i += 1; *cp = 0xFFFD; return true; }
    }
    if (*i + need > len) return false;   // 尾部截断,交给调用方补数据
    for (size_t k = 1; k < need; k++) {
        value = (value << 6) | (p[*i + k] & 0x3Fu);
    }
    *i += need;
    *cp = value;
    return true;
}

int eb_glyph_width(uint32_t cp, int glyph_px)
{
    if (glyph_px <= 0) return 0;
    // GB2312 字库里只有 ASCII 是窄的,其余(含全角标点)按方块字算。
    if (cp < 0x80) {
        int half = glyph_px / 2;
        return half > 0 ? half : 1;
    }
    return glyph_px;
}

// 行尾的 CR 不参与显示,否则 LVGL 会画出一个豆腐块。
static uint16_t trim_cr(const char *buf, size_t start, size_t end)
{
    while (end > start && buf[end - 1] == '\r') end--;
    return (uint16_t)(end - start);
}

size_t eb_wrap(const char *buf, size_t len, bool at_eof,
               const eb_layout_t *lo, eb_line_t *out, int max_lines,
               int *out_lines)
{
    if (out_lines) *out_lines = 0;
    if (!buf || !lo || !out || max_lines <= 0 || lo->width_px <= 0) return 0;

    int limit = lo->lines < max_lines ? lo->lines : max_lines;
    int line = 0;
    size_t i = 0, line_start = 0, consumed = 0;
    int line_w = 0;
    bool overflowed = false;   // 本行已经让一个禁则标点越界

    while (line < limit && i < len) {
        size_t next = i;
        uint32_t cp;
        if (!eb_utf8_next(buf, len, &next, &cp)) {
            if (at_eof) break;      // 数据到此为止
            break;                  // 不完整序列,留给下一次读取
        }
        if (cp == '\n') {
            out[line].offset = (uint32_t)line_start;
            out[line].bytes = trim_cr(buf, line_start, i);
            line++;
            i = next;
            consumed = i;
            line_start = i;
            line_w = 0;
            overflowed = false;
            continue;
        }
        int w = eb_glyph_width(cp, lo->glyph_px);
        if (line_w + w > lo->width_px && i > line_start) {
            if (is_no_line_start(cp) && !overflowed) {
                overflowed = true;   // 让标点跟着上一行走
            } else {
                out[line].offset = (uint32_t)line_start;
                out[line].bytes = trim_cr(buf, line_start, i);
                line++;
                consumed = i;
                line_start = i;
                line_w = 0;
                overflowed = false;
                continue;
            }
        }
        line_w += w;
        i = next;
    }

    // 最后一行没有换行符结尾时,只有确实读到了内容才算作一行。
    if (line < limit && i > line_start) {
        out[line].offset = (uint32_t)line_start;
        out[line].bytes = trim_cr(buf, line_start, i);
        line++;
        consumed = i;
    }

    if (out_lines) *out_lines = line;
    return consumed;
}

void eb_format_size(uint64_t bytes, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    // 1 MB 以上给一位小数,以下用整数 KB:再细的数字对"还能不能放下这本书"
    // 没有帮助,反而让人去数位数。不足 1 KB 的也报 1 KB,避免显示成 0 让人以为是空的。
    if (bytes >= 1024u * 1024u) {
        unsigned long long tenths = (unsigned long long)(bytes * 10 / (1024u * 1024u));
        snprintf(out, out_len, "%llu.%llu MB", tenths / 10, tenths % 10);
    } else if (bytes == 0) {
        snprintf(out, out_len, "0 KB");
    } else {
        snprintf(out, out_len, "%llu KB", (unsigned long long)((bytes + 1023) / 1024));
    }
}

void eb_format_duration(uint32_t seconds, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    // 读书时长看的是"读了多久"这个量级,秒数没有意义;不足一分钟也要说清楚
    // 是刚开始,而不是显示 0 让人以为没记上。
    if (seconds < 60) {
        snprintf(out, out_len, "不到 1 分钟");
    } else if (seconds < 3600) {
        snprintf(out, out_len, "%u 分钟", (unsigned)(seconds / 60));
    } else {
        unsigned hours = (unsigned)(seconds / 3600);
        unsigned minutes = (unsigned)((seconds % 3600) / 60);
        if (minutes == 0) snprintf(out, out_len, "%u 小时", hours);
        else snprintf(out, out_len, "%u 小时 %u 分", hours, minutes);
    }
}

size_t eb_prev_page_start(const char *buf, size_t len, bool at_file_start,
                          const eb_layout_t *lo)
{
    if (!buf || !lo || len == 0) return 0;

    size_t start = 0;
    if (!at_file_start) {
        // 从半路开始排版会和真实分页错开。找一个换行符当起点能把误差限制在
        // 一段之内;找不到就退而求其次,对齐到一个合法的 UTF-8 首字节。
        size_t scan = len < 512 ? len : 512;
        size_t line = (size_t)-1;
        for (size_t i = 0; i < scan; i++) {
            if (buf[i] == '\n') { line = i + 1; break; }
        }
        if (line != (size_t)-1) {
            start = line;
        } else {
            while (start < len && ((unsigned char)buf[start] & 0xC0) == 0x80) start++;
        }
    }

    eb_line_t lines[64];
    size_t page = start, previous = start, full_bytes = 0;
    while (page < len) {
        int count = 0;
        size_t used = eb_wrap(buf + page, len - page, true, lo, lines,
                              (int)(sizeof(lines) / sizeof(lines[0])), &count);
        if (used == 0) break;              // 排不动了,别在同一个偏移上打转
        if (page + used >= len) break;     // 末尾这段被窗口截断,不是整页
        previous = page;
        page += used;
        full_bytes = used;                 // 只记整页的长度,截断的那段不算
    }

    // 当前页起点通常不落在重排后的页边界上,于是"上一页"可能只退回几十个字节,
    // 连按十几次才退完一屏。退得太少就再往前一格,宁可多重复几行。
    if (page > start && full_bytes > 0 && len - page < full_bytes / 2) return previous;
    return page;
}
