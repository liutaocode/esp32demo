// main/apps/ebook/ebook_fs.h —— books 分区(FAT)与 NVS 存档的窄接口。
// 所有调用都可能阻塞,只允许在阅读器的工作任务里使用,不得在按键回调或
// LVGL 定时器中调用。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ebook_state.h"

#define EB_BOOKS_ROOT "/books"

typedef struct {
    char     name[EB_NAME_LEN];   // 含 .txt 后缀的文件名
    uint32_t size;                // 字节
} eb_book_t;

// 挂载 books 分区。首次使用时格式化。失败返回 false,界面会提示存储不可用。
bool eb_fs_mount(void);

// 列出 *.txt,按名字排序,返回条目数。out 至少能放 max 条。
int eb_fs_list(eb_book_t *out, int max);

// 打开/关闭当前正文文件。同一时刻只保持一个句柄。
bool     eb_fs_open(const char *name);
void     eb_fs_close(void);
uint32_t eb_fs_size(void);

// 从当前文件的 offset 处读最多 len 字节,返回实际读到的字节数。
size_t eb_fs_read(uint32_t offset, char *buf, size_t len);

// 删除一本书。name 必须已经过 eb_name_sanitize。
bool eb_fs_delete(const char *name);

// 分区容量与剩余空间,用于传书页显示。失败时两个值都置 0。
void eb_fs_usage(uint64_t *total, uint64_t *freespace);

// 书签:按书名各存一份,最多 EB_MAX_MARKS 条。
int  eb_store_load_marks(const char *name, uint32_t *out, int max);
void eb_store_save_marks(const char *name, const uint32_t *marks, int count);

// 每本书的阅读进度与已读时长。读不到时 offset 与 seconds 置 0。
void eb_store_load_progress(const char *name, uint32_t *offset, uint32_t *seconds);
void eb_store_save_progress(const char *name, uint32_t offset, uint32_t seconds);

// 全部书的累计阅读时长。
uint32_t eb_store_load_total_seconds(void);
void     eb_store_save_total_seconds(uint32_t seconds);

// 字号与主题,全局各一份。读不到时回落到中号 / 纯白。
eb_font_t     eb_store_load_font(void);
void          eb_store_save_font(eb_font_t font);
eb_theme_id_t eb_store_load_theme(void);
void          eb_store_save_theme(eb_theme_id_t theme);
