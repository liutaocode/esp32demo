#include "jelly_squeeze_storage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

typedef struct { uint8_t data[JS_SAVE_SIZE]; } save_t;
static QueueHandle_t s_saves;
static nvs_handle_t s_nvs;
static bool s_ready, s_initialized;
static js_progress_t s_initial;

static void worker(void *arg)
{
    (void)arg;
    save_t item;
    for (;;) {
        if (xQueueReceive(s_saves, &item, portMAX_DELAY) != pdTRUE) continue;
        if (nvs_set_blob(s_nvs, "progress_v1", item.data, sizeof(item.data)) == ESP_OK) {
            (void)nvs_commit(s_nvs);
        }
    }
}

js_progress_t js_storage_init(void)
{
    if (s_initialized) return s_initial;
    s_initialized = true;
    /* 初始化失败也绝不擦除 NVS：这块分区还存着设备身份和别的应用的数据。 */
    if (nvs_flash_init() != ESP_OK) return s_initial;
    if (nvs_open("jelly_squeeze", NVS_READWRITE, &s_nvs) != ESP_OK) return s_initial;
    uint8_t bytes[JS_SAVE_SIZE];
    size_t length = sizeof(bytes);
    esp_err_t err = nvs_get_blob(s_nvs, "progress_v1", bytes, &length);
    if (err == ESP_OK && length == sizeof(bytes)) {
        (void)js_decode(&s_initial, bytes);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(s_nvs);
        return s_initial;
    }
    s_saves = xQueueCreate(1, sizeof(save_t));
    if (!s_saves || xTaskCreate(worker, "jelly_save", 3072, NULL, 2, NULL) != pdPASS) {
        if (s_saves) vQueueDelete(s_saves);
        s_saves = NULL;
        nvs_close(s_nvs);
        return s_initial;
    }
    s_ready = true;
    return s_initial;
}

void js_storage_save(js_progress_t progress)
{
    if (!s_ready) return;
    save_t item;
    js_encode(progress, item.data);
    (void)xQueueOverwrite(s_saves, &item);
}
