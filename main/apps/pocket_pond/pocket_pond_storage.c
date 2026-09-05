#include "pocket_pond_storage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>

typedef struct { uint8_t data[PP_SAVE_SIZE]; unsigned version; } save_t;
static QueueHandle_t s_saves;
static nvs_handle_t s_nvs;
static unsigned s_requested;
static atomic_uint s_saved, s_failed;
static bool s_ready, s_initialized;
static pp_progress_t s_initial;

static void worker(void *arg)
{
    (void)arg;
    save_t item;
    for (;;) {
        if (xQueueReceive(s_saves, &item, portMAX_DELAY) != pdTRUE) continue;
        esp_err_t err = nvs_set_blob(s_nvs, "progress_v1", item.data, sizeof(item.data));
        if (err == ESP_OK) err = nvs_commit(s_nvs);
        if (err == ESP_OK) atomic_store(&s_saved, item.version);
        else atomic_store(&s_failed, item.version);
    }
}

pp_progress_t pp_storage_init(void)
{
    if (s_initialized) return s_initial;
    s_initialized = true;
    /* Never erase NVS on initialization failure: other apps own data here. */
    if (nvs_flash_init() != ESP_OK) return s_initial;
    if (nvs_open("pocket_pond", NVS_READWRITE, &s_nvs) != ESP_OK) return s_initial;
    uint8_t bytes[PP_SAVE_SIZE];
    size_t length = sizeof(bytes);
    esp_err_t err = nvs_get_blob(s_nvs, "progress_v1", bytes, &length);
    if (err == ESP_OK && length == sizeof(bytes) && pp_decode(&s_initial, bytes)) {
        /* Valid versioned collection. */
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(s_nvs); return s_initial;
    }
    s_saves = xQueueCreate(1, sizeof(save_t));
    if (!s_saves || xTaskCreate(worker, "pond_save", 3072, NULL, 2, NULL) != pdPASS) {
        if (s_saves) vQueueDelete(s_saves);
        s_saves = NULL;
        nvs_close(s_nvs);
        return s_initial;
    }
    s_ready = true;
    return s_initial;
}

void pp_storage_save(pp_progress_t progress)
{
    if (!s_ready) return;
    save_t item = {.version = ++s_requested};
    pp_encode(&progress, item.data);
    (void)xQueueOverwrite(s_saves, &item);
}

unsigned pp_storage_status(void)
{
    if (!s_ready) return 2;
    if (atomic_load(&s_saved) == s_requested) return 0;
    if (atomic_load(&s_failed) == s_requested) return 2;
    return 1;
}
