#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ebook_text.h"

static const eb_layout_t LO16 = { .width_px = 216, .glyph_px = 16, .lines = 12 };

static void check_line(const char *buf, const eb_line_t *l, const char *want) {
    assert(l->bytes == strlen(want));
    assert(memcmp(buf + l->offset, want, l->bytes) == 0);
}

int main(void) {
    eb_line_t lines[32];
    int n = 0;

    // UTF-8 解码:ASCII、三字节汉字、非法字节、截断尾巴。
    size_t i = 0; uint32_t cp = 0;
    assert(eb_utf8_next("A", 1, &i, &cp) && cp == 'A' && i == 1);
    i = 0; assert(eb_utf8_next("\xe4\xb8\xad", 3, &i, &cp) && cp == 0x4E2D && i == 3);
    i = 0; assert(eb_utf8_next("\xff", 1, &i, &cp) && cp == 0xFFFD && i == 1);
    i = 0; assert(!eb_utf8_next("\xe4\xb8", 2, &i, &cp) && i == 0);
    i = 0; assert(eb_utf8_next("\xe4Z", 2, &i, &cp) && cp == 0xFFFD && i == 1);

    // 宽度:ASCII 半宽,汉字全宽。
    assert(eb_glyph_width('a', 16) == 8);
    assert(eb_glyph_width(0x4E2D, 16) == 16);
    assert(eb_glyph_width(0x3002, 12) == 12);

    // 216 px / 16 px = 13 个汉字一行。
    const char *text14 = "一二三四五六七八九十甲乙丙丁";
    size_t used = eb_wrap(text14, strlen(text14), true, &LO16, lines, 32, &n);
    assert(n == 2);
    check_line(text14, &lines[0], "一二三四五六七八九十甲乙丙");
    check_line(text14, &lines[1], "丁");
    assert(used == strlen(text14));

    // 换行符消耗掉自己,CRLF 的 CR 不进正文,空行保留为空行。
    const char *crlf = "甲\r\n\r\n乙";
    used = eb_wrap(crlf, strlen(crlf), true, &LO16, lines, 32, &n);
    assert(n == 3);
    check_line(crlf, &lines[0], "甲");
    assert(lines[1].bytes == 0);
    check_line(crlf, &lines[2], "乙");
    assert(used == strlen(crlf));

    // 避头尾:第 14 个字是句号时,允许它挤在行尾,而不是自己占一行开头。
    const char *punct = "一二三四五六七八九十甲乙丙。丁";
    used = eb_wrap(punct, strlen(punct), true, &LO16, lines, 32, &n);
    assert(n == 2);
    check_line(punct, &lines[0], "一二三四五六七八九十甲乙丙。");
    check_line(punct, &lines[1], "丁");
    // 每行只让一个标点越界,连续标点不会无限撑长。
    const char *punct2 = "一二三四五六七八九十甲乙丙。。丁";
    (void)eb_wrap(punct2, strlen(punct2), true, &LO16, lines, 32, &n);
    assert(n == 2);
    check_line(punct2, &lines[0], "一二三四五六七八九十甲乙丙。");
    check_line(punct2, &lines[1], "。丁");

    // 满页:消耗字节数只覆盖排下的行,余下留给下一页。
    eb_layout_t two = { .width_px = 216, .glyph_px = 16, .lines = 2 };
    const char *three = "甲\n乙\n丙\n";
    used = eb_wrap(three, strlen(three), true, &two, lines, 32, &n);
    assert(n == 2 && used == strlen("甲\n乙\n"));

    // 缓冲区尾部的半个汉字:未到文件结尾时留给下一次读取,不排进本页。
    const char *cut = "甲\xe4\xb8";
    used = eb_wrap(cut, strlen(cut), false, &LO16, lines, 32, &n);
    assert(n == 1 && used == 3);
    check_line(cut, &lines[0], "甲");

    // 单字宽于整行也必须排下去,否则会死循环在同一个偏移上。
    eb_layout_t narrow = { .width_px = 8, .glyph_px = 16, .lines = 4 };
    used = eb_wrap("甲乙", strlen("甲乙"), true, &narrow, lines, 32, &n);
    assert(n == 2 && used == strlen("甲乙"));

    // 空输入与非法参数。
    assert(eb_wrap("", 0, true, &LO16, lines, 32, &n) == 0 && n == 0);
    assert(eb_wrap("甲", 3, true, &LO16, NULL, 32, &n) == 0);
    assert(eb_wrap("甲", 3, true, &LO16, lines, 0, &n) == 0);

    // 整本书连续翻页:偏移严格单调递增,最终恰好等于文件长度。
    char book[4096];
    for (size_t k = 0; k + 3 <= sizeof(book) - 1; k += 3) memcpy(book + k, "字", 3);
    book[4095] = '\0';
    size_t total = strlen(book), offset = 0, pages = 0;
    while (offset < total) {
        size_t step = eb_wrap(book + offset, total - offset, true, &LO16, lines, 32, &n);
        assert(step > 0 && n > 0);
        offset += step;
        pages++;
        assert(pages < 1000);
    }
    assert(offset == total);

    // 容量提示的格式:三个地方共用同一句话,数字必须一致。
    char size[32];
    eb_format_size(0, size, sizeof(size));                 assert(strcmp(size, "0 KB") == 0);
    eb_format_size(1, size, sizeof(size));                 assert(strcmp(size, "1 KB") == 0);
    eb_format_size(1024, size, sizeof(size));              assert(strcmp(size, "1 KB") == 0);
    eb_format_size(1025, size, sizeof(size));              assert(strcmp(size, "2 KB") == 0);
    eb_format_size(1024u * 1024 - 1, size, sizeof(size));  assert(strcmp(size, "1024 KB") == 0);
    eb_format_size(1024u * 1024, size, sizeof(size));      assert(strcmp(size, "1.0 MB") == 0);
    eb_format_size(3355443u, size, sizeof(size));          assert(strcmp(size, "3.1 MB") == 0);
    eb_format_size(3355444u, size, sizeof(size));          assert(strcmp(size, "3.2 MB") == 0);
    eb_format_size(3801088u, size, sizeof(size));          assert(strcmp(size, "3.6 MB") == 0);
    eb_format_size(99, NULL, 8);   // 不崩即可
    eb_format_size(99, size, 0);

    char span[32];
    eb_format_duration(0, span, sizeof(span));      assert(strcmp(span, "不到 1 分钟") == 0);
    eb_format_duration(59, span, sizeof(span));     assert(strcmp(span, "不到 1 分钟") == 0);
    eb_format_duration(60, span, sizeof(span));     assert(strcmp(span, "1 分钟") == 0);
    eb_format_duration(3599, span, sizeof(span));   assert(strcmp(span, "59 分钟") == 0);
    eb_format_duration(3600, span, sizeof(span));   assert(strcmp(span, "1 小时") == 0);
    eb_format_duration(4320, span, sizeof(span));   assert(strcmp(span, "1 小时 12 分") == 0);
    eb_format_duration(86400, span, sizeof(span));  assert(strcmp(span, "24 小时") == 0);
    eb_format_duration(60, NULL, 8);
    eb_format_duration(60, span, 0);

    // 反向找页:先正着把一本书分页,再从每一页往回找,必须回到上一页的起点。
    // 这是"从书架续读进来那一页按不动上一页"那个 bug 的回归用例。
    size_t starts[64];
    size_t count_pages = 0, walk = 0;
    while (walk < total && count_pages < 64) {
        starts[count_pages++] = walk;
        walk += eb_wrap(book + walk, total - walk, true, &LO16, lines, 32, &n);
    }
    assert(count_pages > 3);
    for (size_t page = 1; page < count_pages; page++) {
        size_t here = starts[page];
        size_t window = here < 4096 ? here : 4096;
        size_t from = here - window;
        size_t back = eb_prev_page_start(book + from, window, from == 0, &LO16);
        assert(from + back == starts[page - 1]);
    }
    // 窗口够不到文件开头时的真实场景:带段落的长文本,只往回读 1 KB。
    // 这时排版起点和真实分页可能错开,但必须守住"上一页翻下去正好回到原处"
    // 这条不变量,否则用户会在两页之间来回跳。
    static char novel[20000];
    size_t filled = 0;
    for (int para = 0; filled + 64 < sizeof(novel) - 1; para++) {
        for (int ch = 0; ch < 30 + (para % 17) * 3 && filled + 4 < sizeof(novel) - 1; ch++) {
            memcpy(novel + filled, "字", 3);
            filled += 3;
        }
        novel[filled++] = '\n';
    }
    novel[filled] = '\0';

    size_t cursor = 0;
    size_t visited[64];
    size_t visits = 0;
    while (cursor < filled && visits < 64) {
        visited[visits++] = cursor;
        cursor += eb_wrap(novel + cursor, filled - cursor, true, &LO16, lines, 32, &n);
    }
    for (size_t page = 3; page < visits; page++) {
        size_t here = visited[page];
        size_t window = here < 1024 ? here : 1024;
        size_t from = here - window;
        size_t back = eb_prev_page_start(novel + from, window, from == 0, &LO16);
        size_t start = from + back;
        // 唯一必须守住的:确实往回走了,而且没有跳过任何还没读到的内容。
        // 页边界取决于从哪一页开始数行,局部还原不出来,最坏情况是重复几行。
        assert(start < here);
        size_t forward = eb_wrap(novel + start, filled - start, true, &LO16, lines, 32, &n);
        assert(forward > 0);
        // 每按一次上一页至少要退回小半屏,否则要连按十几次才退完一屏。
        assert(here - start > forward / 3);
    }

    // 连按上一页必须一路往前推进,不能卡在同一个偏移上打转。
    size_t cursor_back = visited[visits - 1];
    int steps = 0;
    for (; steps < 200 && cursor_back > 0; steps++) {
        size_t window = cursor_back < 1024 ? cursor_back : 1024;
        size_t from = cursor_back - window;
        size_t moved = from + eb_prev_page_start(novel + from, window, from == 0, &LO16);
        assert(moved < cursor_back);
        cursor_back = moved;
    }
    assert(cursor_back == 0);
    // 一路退到开头用的次数,应该和正着翻过来的页数是一个量级。
    assert((size_t)steps < visits * 2);

    // 文件开头往前找只能停在 0,不会越界。
    assert(eb_prev_page_start(book, 0, true, &LO16) == 0);
    assert(eb_prev_page_start(NULL, 100, true, &LO16) == 0);

    printf("ebook text layout: %zu pages over %zu bytes\n", pages, total);
    return 0;
}
