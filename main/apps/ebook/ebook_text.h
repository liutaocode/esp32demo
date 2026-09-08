// main/apps/ebook/ebook_text.h —— 纯文本排版：UTF-8 解码、按屏分页、禁则处理。
// 不依赖 ESP-IDF 与 LVGL,由主机测试覆盖。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 一页的排版参数。glyph_px 是中文字宽(等于字号),ASCII 按半宽估算。
typedef struct {
    int width_px;   // 正文可用宽度
    int glyph_px;   // 中文字宽/字号
    int lines;      // 一页可容纳的行数
} eb_layout_t;

// 一行在输入缓冲区中的位置。bytes 不含行尾换行符。
typedef struct {
    uint32_t offset;
    uint16_t bytes;
} eb_line_t;

// 解码 buf[*i] 处的一个 UTF-8 码点。非法字节按单字节跳过并返回 0xFFFD。
// 缓冲区尾部出现不完整序列时返回 false 且不移动 *i。
bool eb_utf8_next(const char *buf, size_t len, size_t *i, uint32_t *cp);

// 码点在给定字号下的显示宽度(像素)。
int eb_glyph_width(uint32_t cp, int glyph_px);

// 把字节数写成给人看的大小,如 "3.2 MB" / "812 KB"。容量提示要出现在屏幕、
// 网页和错误信息三个地方,措辞必须一致,所以只在这里实现一次。
void eb_format_size(uint64_t bytes, char *out, size_t out_len);

// 把秒数写成给人看的时长,如 "1 小时 12 分" / "8 分钟" / "不到 1 分钟"。
void eb_format_duration(uint32_t seconds, char *out, size_t out_len);

// 反向找页:buf 是文件中紧挨着当前页之前的一段,长度 len 的末尾就是当前页起点。
// 返回上一页在 buf 中的起始位置。at_file_start 表示 buf 的第一个字节就是文件开头。
//
// 历史栈里有来路时不需要它;从书架续读、跳书签进来的那一页没有来路,只能这样
// 从前面某处重新排一遍版,取最后一个落在当前页之前的页起点。
//
// 这是近似的:一页从哪里断,取决于从哪一页开始数行,局部还原不出来。保证的是
// 返回的起点严格小于当前页,也就是最坏情况下重复几行,绝不会跳过没读的内容。
size_t eb_prev_page_start(const char *buf, size_t len, bool at_file_start,
                          const eb_layout_t *lo);

// 把 buf 排成最多 lo->lines 行,写入 out,返回本页消耗的字节数。
// at_eof 为 false 时,尾部不完整的 UTF-8 序列会留给下一次读取。
// out_lines 可为 NULL。返回 0 表示无内容可排(缓冲区为空)。
size_t eb_wrap(const char *buf, size_t len, bool at_eof,
               const eb_layout_t *lo, eb_line_t *out, int max_lines,
               int *out_lines);
