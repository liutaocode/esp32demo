// main/apps/ebook/ebook_net.c —— 见 ebook_net.h。
//
// 传书页面按用户的选择走明文 HTTP:同一个局域网里的任何人都能上传和删除。
// 这是有意的取舍(省掉输密码的交互),因此这里绝不碰任何凭据:上传的内容只
// 落在 books 分区,页面不读取也不回显 Wi-Fi 密码。
#include "ebook_net.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "ebook_fs.h"
#include "ebook_name.h"
#include "ebook_text.h"

static const char *TAG = "ebook_net";

#define EB_STA_MAX_RETRY   3
#define EB_UPLOAD_CHUNK    2048
#define EB_AP_URL          "http://192.168.4.1"

static SemaphoreHandle_t s_lock;
static eb_net_status_t   s_status;
static esp_netif_t      *s_sta_netif;
static esp_netif_t      *s_ap_netif;
static httpd_handle_t    s_httpd;
static int               s_retry;
static bool              s_prepared;
static bool              s_running;

static void set_state(eb_net_state_t state, const char *ssid, const char *url)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    if (ssid) snprintf(s_status.ssid, sizeof(s_status.ssid), "%s", ssid);
    if (url) snprintf(s_status.url, sizeof(s_status.url), "%s", url);
    xSemaphoreGive(s_lock);
}

// 记下一句给设备屏幕看的结果,并顺手刷新容量。传书页每 400 毫秒读一次状态,
// 所以这里不需要额外的通知机制。
static void set_note(bool ok, bool shelf_changed, const char *fmt, ...)
{
    char note[sizeof(((eb_net_status_t *)0)->note)];
    va_list args;
    va_start(args, fmt);
    vsnprintf(note, sizeof(note), fmt, args);
    va_end(args);

    uint64_t total = 0, freespace = 0;
    eb_fs_usage(&total, &freespace);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    snprintf(s_status.note, sizeof(s_status.note), "%s", note);
    s_status.note_ok = ok;
    s_status.total = total;
    s_status.freespace = freespace;
    if (shelf_changed) s_status.uploads++;
    xSemaphoreGive(s_lock);
}

void eb_net_status(eb_net_status_t *out)
{
    if (!out) return;
    if (!s_lock) { memset(out, 0, sizeof(*out)); return; }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_status;
    xSemaphoreGive(s_lock);
}

// ---------------------------------------------------------------- HTTP 页面

// 单页:上传按钮 + 书目列表。没有外链,断网也能用。
static const char PAGE_HTML[] =
    "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>FoloToy 传书</title><style>"
    "body{font-family:system-ui,sans-serif;margin:0;padding:24px;background:#f4f4ea;color:#17202a}"
    "h1{font-size:20px;margin:0 0 4px}p{color:#5a6a72;font-size:14px;margin:4px 0 20px}"
    "label{display:block;padding:14px;background:#1689e8;color:#fff;border-radius:8px;text-align:center;font-weight:600}"
    "input[type=file]{display:none}ul{list-style:none;padding:0;margin:20px 0 0}"
    "li{display:flex;align-items:center;justify-content:space-between;padding:12px;background:#fff;border-radius:8px;margin-bottom:8px}"
    "button{border:0;background:#e43b2f;color:#fff;border-radius:6px;padding:6px 12px}"
    "small{color:#8a9aa2}#log{margin-top:12px;font-size:14px;color:#5a6a72}"
    "</style></head><body>"
    "<h1>传书到 FoloToy</h1><p>只支持 UTF-8 编码的 .txt 文件。</p>"
    "<label>选择文件<input type=\"file\" id=\"f\" accept=\".txt,text/plain\"></label>"
    "<div id=\"space\"></div><div id=\"log\"></div><ul id=\"list\"></ul>"
    "<script>"
    "const log=document.getElementById('log');let free=0;"
    "function human(b){return b>=1048576?(b/1048576).toFixed(1)+' MB':Math.ceil(b/1024)+' KB';}"
    "async function refresh(){"
    "const s=await(await fetch('/api/space')).json();free=s.free;"
    "document.getElementById('space').textContent='剩余 '+human(s.free)+' / '+human(s.total);"
    "const books=await(await fetch('/api/list')).json();"
    "document.getElementById('list').innerHTML=books.map(b=>"
    "`<li><span>${b.name}<br><small>${human(b.size)}</small></span>"
    "<button onclick=\"del('${encodeURIComponent(b.name)}')\">删除</button></li>`).join('')"
    "||'<li><small>书架还是空的</small></li>';}"
    "async function del(n){if(!confirm('删除这本书？'))return;"
    "await fetch('/delete?name='+n,{method:'POST'});refresh();}"
    "document.getElementById('f').onchange=async e=>{const file=e.target.files[0];if(!file)return;"
    // 空间不够就在手机上直接拦下,一个字节都不发。设备端同样会拦,这里只是
    // 为了立刻给出答复:等传完再报错，用户已经白等了一分多钟。
    "e.target.value='';"
    "if(Math.ceil(file.size/4096)*4096+8192>free){"
    "log.textContent='放不下：'+file.name+' 要 '+human(file.size)+'，设备只剩 '+human(free);return;}"
    "log.textContent='上传中…';"
    "try{const r=await fetch('/upload?name='+encodeURIComponent(file.name),{method:'POST',body:file});"
    "log.textContent=r.ok?'已保存：'+file.name:'上传失败：'+await r.text();}"
    "catch(err){log.textContent='上传中断，请重试';}"
    "refresh();};"
    "refresh();"
    "</script></body></html>";

static esp_err_t page_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGE_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t list_get(httpd_req_t *req)
{
    static eb_book_t books[EB_MAX_BOOKS];
    int count = eb_fs_list(books, EB_MAX_BOOKS);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");
    for (int i = 0; i < count; i++) {
        char item[EB_NAME_LEN + 48];
        // 书名已由 eb_name_sanitize 过滤掉引号、反斜杠和控制字符,可直接拼进 JSON。
        snprintf(item, sizeof(item), "%s{\"name\":\"%s\",\"size\":%lu}",
                 i ? "," : "", books[i].name, (unsigned long)books[i].size);
        httpd_resp_sendstr_chunk(req, item);
    }
    httpd_resp_sendstr_chunk(req, "]");
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t space_get(httpd_req_t *req)
{
    uint64_t total = 0, freespace = 0;
    eb_fs_usage(&total, &freespace);
    char body[64];
    snprintf(body, sizeof(body), "{\"free\":%llu,\"total\":%llu}",
             (unsigned long long)freespace, (unsigned long long)total);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, body);
}

// 从查询串取出 name 并清洗。失败时已经回过 400。
static bool query_name(httpd_req_t *req, char *out, size_t out_len)
{
    char query[256];
    char raw[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "name", raw, sizeof(raw)) != ESP_OK ||
        !eb_name_sanitize(raw, out, out_len)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "文件名不合法");
        return false;
    }
    return true;
}

static esp_err_t upload_post(httpd_req_t *req)
{
    char name[EB_NAME_LEN];
    if (!query_name(req, name, sizeof(name))) return ESP_FAIL;

    // 先比空间再收正文。收完再报错等于让用户白等一分多钟,而且失败原因只能靠猜。
    // FAT 按簇分配,再给目录项和 FAT 表本身留一点余量。同名重传不需要额外算两倍:
    // 旧书此刻仍占着空间,esp_vfs_fat_info 报的剩余量已经把它算进去了。
    uint64_t total = 0, freespace = 0;
    eb_fs_usage(&total, &freespace);
    uint64_t need = ((uint64_t)req->content_len + 4095) / 4096 * 4096 + 8192;
    if (req->content_len == 0 || need > freespace) {
        char want[24], have[24], message[160];
        eb_format_size(req->content_len, want, sizeof(want));
        eb_format_size(freespace, have, sizeof(have));
        if (req->content_len == 0) {
            snprintf(message, sizeof(message), "%s 是空文件", name);
        } else {
            snprintf(message, sizeof(message), "%s 要 %s,设备只剩 %s", name, want, have);
        }
        set_note(false, false, "%s", message);
        ESP_LOGW(TAG, "rejected upload: %s", message);
        httpd_resp_set_status(req, "507 Insufficient Storage");
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        httpd_resp_sendstr(req, message);
        return ESP_FAIL;   // 关掉连接,一个字节的正文都不收
    }

    char path[EB_NAME_LEN + sizeof(EB_BOOKS_ROOT) + 8];
    snprintf(path, sizeof(path), EB_BOOKS_ROOT "/%s.part", name);
    FILE *out = fopen(path, "wb");
    if (!out) {
        set_note(false, false, "存储不可用,%s 没能保存", name);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "存储不可用");
        return ESP_FAIL;
    }

    // 整本书不进内存:每次收 2 KB 立刻落盘。
    static char chunk[EB_UPLOAD_CHUNK];
    int remaining = req->content_len;
    bool ok = true;
    while (remaining > 0) {
        int want = remaining < EB_UPLOAD_CHUNK ? remaining : EB_UPLOAD_CHUNK;
        int got = httpd_req_recv(req, chunk, want);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0) { ok = false; break; }
        if (fwrite(chunk, 1, (size_t)got, out) != (size_t)got) { ok = false; break; }
        remaining -= got;
    }
    fclose(out);

    // 半截的文件不该出现在书架上,所以先写 .part,收完整了再改名。
    char final_path[EB_NAME_LEN + sizeof(EB_BOOKS_ROOT) + 8];
    snprintf(final_path, sizeof(final_path), EB_BOOKS_ROOT "/%s", name);
    if (!ok || remaining != 0) {
        remove(path);
        // 空间在开头已经查过,走到这里多半是连接断了或闪存写失败,不再猜原因。
        set_note(false, false, "%s 传到一半中断了", name);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "传输中断,请重试");
        return ESP_FAIL;
    }
    remove(final_path);   // 覆盖同名旧书
    if (rename(path, final_path) != 0) {
        remove(path);
        set_note(false, false, "%s 保存失败", name);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "写入失败");
        return ESP_FAIL;
    }

    char size[24];
    eb_format_size(req->content_len, size, sizeof(size));
    ESP_LOGI(TAG, "received %s (%s)", name, size);
    set_note(true, true, "已收到 %s · %s", name, size);
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t delete_post(httpd_req_t *req)
{
    char name[EB_NAME_LEN];
    if (!query_name(req, name, sizeof(name))) return ESP_FAIL;
    if (!eb_fs_delete(name)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "没有这本书");
        return ESP_FAIL;
    }
    set_note(true, true, "已删除 %s", name);
    return httpd_resp_sendstr(req, "ok");
}

static void httpd_start_once(void)
{
    if (s_httpd) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 5;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 10;
    cfg.send_wait_timeout = 10;
    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        s_httpd = NULL;
        return;
    }
    static const httpd_uri_t routes[] = {
        { .uri = "/",         .method = HTTP_GET,  .handler = page_get },
        { .uri = "/api/list", .method = HTTP_GET,  .handler = list_get },
        { .uri = "/api/space",.method = HTTP_GET,  .handler = space_get },
        { .uri = "/upload",   .method = HTTP_POST, .handler = upload_post },
        { .uri = "/delete",   .method = HTTP_POST, .handler = delete_post },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(s_httpd, &routes[i]);
    }
}

// ---------------------------------------------------------------- 联网

static void start_softap(void);

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        if (++s_retry <= EB_STA_MAX_RETRY) {
            esp_wifi_connect();
        } else {
            // 路由器连不上就自己开热点,用户手边不一定有另一台能配网的设备。
            ESP_LOGW(TAG, "sta failed %d times, falling back to soft-AP", s_retry - 1);
            start_softap();
        }
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id != IP_EVENT_STA_GOT_IP) return;
    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)data;
    char url[32];
    snprintf(url, sizeof(url), "http://" IPSTR, IP2STR(&event->ip_info.ip));
    httpd_start_once();
    set_note(true, false, "还没有收到书");
    s_retry = 0;
    set_state(EB_NET_STA_READY, NULL, url);
    ESP_LOGI(TAG, "transfer page at %s", url);
}

// 热点名后缀每次开机重新随机,不再从 MAC 派生:MAC 后缀是设备的固定标识,
// 会让这台机器在别人的 Wi-Fi 列表里长期可辨认。字表去掉了 0/O、1/I 这类
// 看混的字符,用户要照着屏幕在手机上找这个名字。
static void random_suffix(char *out, size_t len)
{
    static const char ALPHABET[] = "ACDEFGHJKLMNPQRTUVWXY34679";
    for (size_t i = 0; i + 1 < len; i++) {
        out[i] = ALPHABET[esp_random() % (sizeof(ALPHABET) - 1)];
    }
    out[len - 1] = '\0';
}

static void start_softap(void)
{
    if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();

    char suffix[5];
    random_suffix(suffix, sizeof(suffix));

    wifi_config_t cfg = { 0 };
    // 开放热点:用户明确选择了明文传输,这里不设密码,也不显示任何凭据。
    snprintf((char *)cfg.ap.ssid, sizeof(cfg.ap.ssid), "BookTransfer-%s", suffix);
    cfg.ap.ssid_len = (uint8_t)strlen((char *)cfg.ap.ssid);
    cfg.ap.max_connection = 2;
    cfg.ap.authmode = WIFI_AUTH_OPEN;
    cfg.ap.channel = 1;

    esp_wifi_stop();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    if (esp_wifi_start() != ESP_OK) {
        set_state(EB_NET_FAILED, NULL, NULL);
        return;
    }
    httpd_start_once();
    set_note(true, false, "还没有收到书");
    set_state(EB_NET_AP_READY, (const char *)cfg.ap.ssid, EB_AP_URL);
    ESP_LOGI(TAG, "soft-AP \"%s\", transfer page at %s", cfg.ap.ssid, EB_AP_URL);
}

bool eb_net_prepare(void)
{
    if (s_prepared) return true;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return false;
    if (esp_netif_init() != ESP_OK) return false;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;

    if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL) != ESP_OK) return false;
    if (esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip_event, NULL) != ESP_OK) return false;

    s_prepared = true;
    return true;
}

bool eb_net_start(void)
{
    if (!s_prepared || s_running) return s_running;
    s_retry = 0;

    if (!s_sta_netif) s_sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) {
        set_state(EB_NET_FAILED, NULL, NULL);
        return false;
    }
    s_running = true;

    // 凭据由扫码配网页(wifi_setup)写进 NVS,这里只读出来用,不显示也不导出。
    wifi_config_t saved = { 0 };
    bool have_creds = (esp_wifi_get_config(WIFI_IF_STA, &saved) == ESP_OK) && saved.sta.ssid[0];
    if (!have_creds) {
        start_softap();
        return true;
    }

    char ssid[33];
    snprintf(ssid, sizeof(ssid), "%.*s", (int)sizeof(saved.sta.ssid), (const char *)saved.sta.ssid);
    set_state(EB_NET_CONNECTING, ssid, NULL);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &saved));
    if (esp_wifi_start() != ESP_OK) {
        start_softap();
    }
    return true;
}

void eb_net_stop(void)
{
    if (s_httpd) { httpd_stop(s_httpd); s_httpd = NULL; }
    if (s_running) {
        esp_wifi_stop();
        esp_wifi_deinit();
        s_running = false;
    }
    set_state(EB_NET_IDLE, "", "");
}
