// main/apps/ebook/ebook_name.c —— 见 ebook_name.h。
#include "ebook_name.h"

#include <string.h>

#define EB_NAME_MAX_BYTES 47   // 加上 "/books/" 前缀后仍在 FATFS 长文件名限内

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// FAT 不接受这些字符,另外 / 和 \ 会让文件名跑出 /books 目录。
static bool is_forbidden(unsigned char c)
{
    if (c < 0x20 || c == 0x7F) return true;
    return strchr("/\\:*?\"<>|", (char)c) != NULL;
}

static bool has_txt_suffix(const char *s, size_t len)
{
    if (len < 4) return false;
    const char *tail = s + len - 4;
    return tail[0] == '.'
        && (tail[1] == 't' || tail[1] == 'T')
        && (tail[2] == 'x' || tail[2] == 'X')
        && (tail[3] == 't' || tail[3] == 'T');
}

// FATFS 以 UTF-8 收文件名,半个汉字会让它写出一个打不开的条目。
static bool is_valid_utf8(const char *s, size_t len)
{
    for (size_t i = 0; i < len; ) {
        unsigned char c = (unsigned char)s[i];
        size_t need;
        if (c < 0x80) need = 1;
        else if ((c & 0xE0) == 0xC0) need = 2;
        else if ((c & 0xF0) == 0xE0) need = 3;
        else if ((c & 0xF8) == 0xF0) need = 4;
        else return false;
        if (i + need > len) return false;
        for (size_t k = 1; k < need; k++) {
            if (((unsigned char)s[i + k] & 0xC0) != 0x80) return false;
        }
        i += need;
    }
    return true;
}

bool eb_name_sanitize(const char *raw, char *out, size_t out_len)
{
    if (!out || out_len == 0) return false;
    out[0] = '\0';
    if (!raw || out_len < EB_NAME_MAX_BYTES + 5) return false;

    char decoded[EB_NAME_MAX_BYTES + 1];
    size_t n = 0;
    for (size_t i = 0; raw[i]; i++) {
        char c = raw[i];
        if (c == '%') {
            int hi = hex_value(raw[i + 1]);
            int lo = hi < 0 ? -1 : hex_value(raw[i + 2]);
            if (lo < 0) return false;          // 残缺的转义:整个名字作废
            c = (char)((hi << 4) | lo);
            i += 2;
        }
        if (is_forbidden((unsigned char)c)) return false;
        if (n >= EB_NAME_MAX_BYTES) return false;   // 过长的名字宁可拒收
        decoded[n++] = c;
    }
    decoded[n] = '\0';

    size_t start = 0;
    while (start < n && (decoded[start] == ' ' || decoded[start] == '\t')) start++;
    while (n > start && (decoded[n - 1] == ' ' || decoded[n - 1] == '\t' || decoded[n - 1] == '.')) n--;
    size_t len = n - start;
    if (len == 0) return false;

    const char *body = decoded + start;
    if (!is_valid_utf8(body, len)) return false;
    // 去掉尾部的点顺带挡住了 "." 与 "..",这里再确认一次。
    if ((len == 1 && body[0] == '.') || (len == 2 && body[0] == '.' && body[1] == '.')) return false;

    memcpy(out, body, len);
    out[len] = '\0';
    if (!has_txt_suffix(out, len)) memcpy(out + len, ".txt", 5);
    return true;
}

void eb_name_title(const char *file_name, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!file_name) return;
    size_t len = strlen(file_name);
    if (has_txt_suffix(file_name, len)) len -= 4;
    if (len > out_len - 1) len = out_len - 1;
    memcpy(out, file_name, len);
    out[len] = '\0';
}
