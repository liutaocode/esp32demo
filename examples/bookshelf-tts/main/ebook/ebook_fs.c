// main/apps/ebook/ebook_fs.c —— 见 ebook_fs.h。
#include "ebook_fs.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "ebook_fs";
static const char *NVS_NAMESPACE = "ebook";

static wl_handle_t s_wl = WL_INVALID_HANDLE;
static FILE       *s_file;
static uint32_t    s_file_size;

// 传书传到一半断电或断线,会在分区里留下一个 .part。它不出现在书架上,
// 也没有任何入口能删掉它,却一直占着空间。开机扫一遍是唯一的回收时机。
static void sweep_partials(void)
{
    DIR *dir = opendir(EB_BOOKS_ROOT);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || len >= 64 || strcmp(entry->d_name + len - 5, ".part") != 0) continue;
        char path[EB_NAME_LEN + sizeof(EB_BOOKS_ROOT) + 8];
        snprintf(path, sizeof(path), EB_BOOKS_ROOT "/%.63s", entry->d_name);
        if (remove(path) == 0) ESP_LOGW(TAG, "removed leftover %s", entry->d_name);
    }
    closedir(dir);
}

bool eb_fs_mount(void)
{
    if (s_wl != WL_INVALID_HANDLE) return true;
    const esp_vfs_fat_mount_config_t cfg = {
        // 第一次刷机时分区是空白的,必须允许格式化,否则永远挂不上。
        .format_if_mount_failed = true,
        .max_files = 2,               // 正文一个 + 上传一个
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(EB_BOOKS_ROOT, "books", &cfg, &s_wl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount %s failed: %s", EB_BOOKS_ROOT, esp_err_to_name(err));
        s_wl = WL_INVALID_HANDLE;
        return false;
    }
    sweep_partials();
    return true;
}

static bool is_txt(const char *name)
{
    size_t len = strlen(name);
    if (len < 5) return false;   // 至少 "x.txt"
    const char *tail = name + len - 4;
    return tail[0] == '.'
        && (tail[1] == 't' || tail[1] == 'T')
        && (tail[2] == 'x' || tail[2] == 'X')
        && (tail[3] == 't' || tail[3] == 'T');
}

static int book_rank(const char *name)
{
    if (strcmp(name, "朗读体验.txt") == 0) return 0;
    if (strcmp(name, "唐诗选读.txt") == 0) return 1;
    if (strcmp(name, "宋词选读.txt") == 0) return 2;
    return 3;
}

static int book_compare(const char *a, const char *b)
{
    int order = book_rank(a) - book_rank(b);
    return order ? order : strcmp(a, b);
}

int eb_fs_list(eb_book_t *out, int max)
{
    if (!out || max <= 0 || s_wl == WL_INVALID_HANDLE) return 0;
    DIR *dir = opendir(EB_BOOKS_ROOT);
    if (!dir) return 0;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max) {
        if (entry->d_type == DT_DIR || !is_txt(entry->d_name)) continue;
        if (strlen(entry->d_name) >= EB_NAME_LEN) continue;

        // 精度写死成 EB_NAME_LEN-1,上面的长度检查编译器看不到。
        char path[EB_NAME_LEN + sizeof(EB_BOOKS_ROOT) + 2];
        snprintf(path, sizeof(path), EB_BOOKS_ROOT "/%.63s", entry->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || st.st_size <= 0) continue;

        snprintf(out[count].name, sizeof(out[count].name), "%.63s", entry->d_name);
        out[count].size = (uint32_t)st.st_size;
        count++;
    }
    closedir(dir);

    // 条目数最多 32,插入排序足够,也不需要额外内存。
    for (int i = 1; i < count; i++) {
        eb_book_t key = out[i];
        int j = i - 1;
        while (j >= 0 && book_compare(out[j].name, key.name) > 0) { out[j + 1] = out[j]; j--; }
        out[j + 1] = key;
    }
    return count;
}

bool eb_fs_open(const char *name)
{
    eb_fs_close();
    if (!name || s_wl == WL_INVALID_HANDLE) return false;
    char path[EB_NAME_LEN + sizeof(EB_BOOKS_ROOT) + 2];
    snprintf(path, sizeof(path), EB_BOOKS_ROOT "/%.63s", name);
    s_file = fopen(path, "rb");
    if (!s_file) {
        ESP_LOGW(TAG, "open %s failed", path);
        return false;
    }
    struct stat st;
    s_file_size = (stat(path, &st) == 0 && st.st_size > 0) ? (uint32_t)st.st_size : 0;
    return true;
}

void eb_fs_close(void)
{
    if (s_file) { fclose(s_file); s_file = NULL; }
    s_file_size = 0;
}

uint32_t eb_fs_size(void) { return s_file_size; }

size_t eb_fs_read(uint32_t offset, char *buf, size_t len)
{
    if (!s_file || !buf || len == 0 || offset >= s_file_size) return 0;
    if (fseek(s_file, (long)offset, SEEK_SET) != 0) return 0;
    return fread(buf, 1, len, s_file);
}

bool eb_fs_delete(const char *name)
{
    if (!name || s_wl == WL_INVALID_HANDLE) return false;
    char path[EB_NAME_LEN + sizeof(EB_BOOKS_ROOT) + 2];
    snprintf(path, sizeof(path), EB_BOOKS_ROOT "/%.63s", name);
    return remove(path) == 0;
}

void eb_fs_usage(uint64_t *total, uint64_t *freespace)
{
    if (total) *total = 0;
    if (freespace) *freespace = 0;
    if (s_wl == WL_INVALID_HANDLE) return;
    uint64_t bytes_total = 0, bytes_free = 0;
    if (esp_vfs_fat_info(EB_BOOKS_ROOT, &bytes_total, &bytes_free) != ESP_OK) return;
    if (total) *total = bytes_total;
    if (freespace) *freespace = bytes_free;
}

// ---------------------------------------------------------------- NVS 存档

// NVS 键最长 15 字节,中文书名放不下,用书名的 CRC32 当键。
static void book_key(const char *name, char prefix, char *out, size_t out_len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        crc ^= *p;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    snprintf(out, out_len, "%c%08lx", prefix, (unsigned long)(crc ^ 0xFFFFFFFFu));
}

int eb_store_load_marks(const char *name, uint32_t *out, int max)
{
    if (!name || !out || max <= 0) return 0;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return 0;
    char key[16];
    book_key(name, 'm', key, sizeof(key));
    size_t size = sizeof(uint32_t) * (size_t)max;
    esp_err_t err = nvs_get_blob(handle, key, out, &size);
    nvs_close(handle);
    if (err != ESP_OK) return 0;
    return (int)(size / sizeof(uint32_t));
}

void eb_store_save_marks(const char *name, const uint32_t *marks, int count)
{
    if (!name || count < 0 || count > EB_MAX_MARKS) return;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    char key[16];
    book_key(name, 'm', key, sizeof(key));
    if (count == 0) {
        nvs_erase_key(handle, key);
    } else {
        nvs_set_blob(handle, key, marks, sizeof(uint32_t) * (size_t)count);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

eb_font_t eb_store_load_font(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return EB_FONT_M;
    uint8_t value = EB_FONT_M;
    esp_err_t err = nvs_get_u8(handle, "font", &value);
    nvs_close(handle);
    if (err != ESP_OK || value >= EB_FONT_COUNT) return EB_FONT_M;
    return (eb_font_t)value;
}

void eb_store_save_font(eb_font_t font)
{
    if (font < 0 || font >= EB_FONT_COUNT) return;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u8(handle, "font", (uint8_t)font);
    nvs_commit(handle);
    nvs_close(handle);
}

eb_theme_id_t eb_store_load_theme(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return EB_THEME_WHITE;
    uint8_t value = 0;
    esp_err_t err = nvs_get_u8(handle, "theme", &value);
    if (err != ESP_OK) {
        // 旧固件只存过护眼开关。开着的话继承成护眼主题,别让人重新挑一遍。
        uint8_t legacy = 0;
        if (nvs_get_u8(handle, "eye", &legacy) == ESP_OK && legacy != 0) {
            nvs_close(handle);
            return EB_THEME_EYE;
        }
    }
    nvs_close(handle);
    if (err != ESP_OK || value >= EB_THEME_COUNT) return EB_THEME_WHITE;
    return (eb_theme_id_t)value;
}

void eb_store_save_theme(eb_theme_id_t theme)
{
    if (theme < 0 || theme >= EB_THEME_COUNT) return;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u8(handle, "theme", (uint8_t)theme);
    nvs_commit(handle);
    nvs_close(handle);
}

// 阅读位置与本书时长写在一条记录里:两者总是一起变,分开存会在断电时对不上。
typedef struct { uint32_t offset; uint32_t seconds; } eb_progress_t;

void eb_store_load_progress(const char *name, uint32_t *offset, uint32_t *seconds)
{
    if (offset) *offset = 0;
    if (seconds) *seconds = 0;
    if (!name) return;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    char key[16];
    book_key(name, 'p', key, sizeof(key));
    eb_progress_t saved = { 0 };
    size_t size = sizeof(saved);
    esp_err_t err = nvs_get_blob(handle, key, &saved, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(saved)) return;
    if (offset) *offset = saved.offset;
    if (seconds) *seconds = saved.seconds;
}

void eb_store_save_progress(const char *name, uint32_t offset, uint32_t seconds)
{
    if (!name) return;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    char key[16];
    book_key(name, 'p', key, sizeof(key));
    eb_progress_t saved = { .offset = offset, .seconds = seconds };
    nvs_set_blob(handle, key, &saved, sizeof(saved));
    nvs_commit(handle);
    nvs_close(handle);
}

uint32_t eb_store_load_total_seconds(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return 0;
    uint32_t value = 0;
    if (nvs_get_u32(handle, "total", &value) != ESP_OK) value = 0;
    nvs_close(handle);
    return value;
}

void eb_store_save_total_seconds(uint32_t seconds)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u32(handle, "total", seconds);
    nvs_commit(handle);
    nvs_close(handle);
}

static void seed_intro(void) {
    eb_book_t first;
    if (s_wl == WL_INVALID_HANDLE || eb_fs_list(&first, 1) != 0) return;
    const char *path = EB_BOOKS_ROOT "/朗读体验.txt";
    struct stat st;
    if (stat(path, &st) == 0) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    const char *text =
        "口袋书架，朗读体验。\n\n"
        "欢迎使用口袋书架。现在，书里的文字也可以读给你听。按确定打开阅读菜单，再按确定开始朗读。\n\n"
        "清晨，阳光穿过窗帘，落在桌上的一本书旁边。窗外传来鸟叫声，街上的人们开始了新的一天。\n\n"
        "你可以安静地看书，也可以让口袋书架继续往下读。读完当前页面，文字会自动翻到下一页。\n\n"
        "如果想休息一下，打开菜单选择暂停朗读。再次选择继续朗读，会从刚才那一句重新开始，不会漏掉半句话。\n\n"
        "手动翻页或返回书架，会停止朗读。你也可以在阅读菜单里切换语速，找到自己听起来舒服的节奏。\n\n"
        "今天我们走到河边，看见微风吹过水面。树下有人慢慢散步，有人坐着看书，也有人闭上眼睛听故事。\n\n"
        "书里有山川，有城市，也有许多平凡而温暖的小事。每天读上一小段，时间会把这些句子变成自己的记忆。\n\n"
        "这是一篇随应用提供的测试文字。你可以长按确定回到书架，再长按确定进入传书页面，上传自己的纯文本书籍。\n\n"
        "朗读体验结束。愿你在每一本书里，都能遇见新的风景。\n";
    size_t len = strlen(text);
    bool ok = fwrite(text, 1, len, f) == len;
    if (fclose(f) != 0) ok = false;
    if (!ok) remove(path);
}

extern const unsigned char tang_start[] asm("_binary_tang_txt_start");
extern const unsigned char tang_end[] asm("_binary_tang_txt_end");
extern const unsigned char song_start[] asm("_binary_song_txt_start");
extern const unsigned char song_end[] asm("_binary_song_txt_end");

/* Commit each new book by rename; never replace an existing user file. */
static bool seed_book(const char *name, const unsigned char *data, size_t size)
{
    char path[128], partial[136];
    snprintf(path, sizeof(path), EB_BOOKS_ROOT "/%s", name);
    struct stat st;
    if (stat(path, &st) == 0) return true;
    snprintf(partial, sizeof(partial), "%s.part", path);
    FILE *f = fopen(partial, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    if (fclose(f) != 0) ok = false;
    if (ok) ok = rename(partial, path) == 0;
    if (!ok) remove(partial);
    return ok;
}

void eb_fs_seed_sample(void)
{
    if (s_wl == WL_INVALID_HANDLE) return;
    const char *marker = EB_BOOKS_ROOT "/.tts-samples-v2";
    struct stat st;
    if (stat(marker, &st) == 0) return;
    seed_intro();
    if (!seed_book("唐诗选读.txt", tang_start, tang_end - tang_start) ||
        !seed_book("宋词选读.txt", song_start, song_end - song_start)) return;
    FILE *f = fopen(marker, "wb");
    if (f) fclose(f);
}
