// main/apps/ebook/ebook_name.h —— 上传文件名的解码与清洗。
// 传书页面完全开放,文件名来自局域网里的任意一台设备,必须当成不可信输入。
// 纯 C,由主机测试覆盖。
#pragma once

#include <stdbool.h>
#include <stddef.h>

// 把查询串里的 name 参数解码成可以直接拼进 /books/ 的文件名。
// 依次做:百分号解码 → 去掉路径成分 → 拒绝控制字符与 FAT 非法字符 →
// 去首尾空白 → 限长 → 补齐 .txt 后缀。
// 任何一步不合法都返回 false,并保证 out 是空串。
bool eb_name_sanitize(const char *raw, char *out, size_t out_len);

// 书名(去掉 .txt 后缀)拷贝到 out,用于界面显示。
void eb_name_title(const char *file_name, char *out, size_t out_len);
