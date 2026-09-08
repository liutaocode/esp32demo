#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ebook_name.h"

static void ok(const char *raw, const char *want) {
    char out[64] = "sentinel";
    assert(eb_name_sanitize(raw, out, sizeof(out)));
    assert(strcmp(out, want) == 0);
}
static void bad(const char *raw) {
    char out[64] = "sentinel";
    assert(!eb_name_sanitize(raw, out, sizeof(out)));
    assert(out[0] == '\0');   // 失败时不能留下半个名字
}

int main(void) {
    ok("book.txt", "book.txt");
    ok("book", "book.txt");                 // 补后缀
    ok("BOOK.TXT", "BOOK.TXT");             // 已有后缀不重复补
    ok("%E4%B8%89%E4%BD%93", "三体.txt");    // 百分号解码
    ok("  spaced  ", "spaced.txt");         // 去首尾空白
    ok("a.b.c", "a.b.c.txt");

    bad("../../secret.txt");                // 目录穿越
    bad("dir/book.txt");
    bad("dir\\book.txt");
    bad("bad:name.txt");
    bad("q?.txt");
    bad("bell\x07.txt");
    bad("%E4%B8");                          // 残缺转义
    bad("%zz.txt");
    bad("");
    bad("   ");
    bad(".");
    bad("..");
    bad("...");
    bad(NULL);
    // 超长名字拒收,而不是截断成另一本书。
    char longname[80];
    memset(longname, 'a', sizeof(longname) - 1);
    longname[sizeof(longname) - 1] = '\0';
    bad(longname);

    // 输出缓冲太小时安全失败。
    char small[8];
    assert(!eb_name_sanitize("book.txt", small, sizeof(small)) && small[0] == '\0');
    assert(!eb_name_sanitize("book.txt", NULL, 0));

    char title[64];
    eb_name_title("三体.txt", title, sizeof(title));
    assert(strcmp(title, "三体") == 0);
    eb_name_title("noext", title, sizeof(title));
    assert(strcmp(title, "noext") == 0);
    eb_name_title(NULL, title, sizeof(title));
    assert(title[0] == '\0');
    char tiny[4];
    eb_name_title("abcdefg.txt", tiny, sizeof(tiny));
    assert(strcmp(tiny, "abc") == 0);

    printf("ebook name sanitizer: ok\n");
    return 0;
}
