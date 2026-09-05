#include "idiom_pet_storage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>

typedef struct { uint64_t data; unsigned version; } save_t;
static QueueHandle_t s_saves;
static nvs_handle_t s_nvs;
static unsigned s_requested;
static atomic_uint s_saved, s_failed;
static bool s_ready, s_initialized;
static ip_progress_t s_initial;

static void worker(void *arg)
{
    (void)arg;
    save_t item;
    for (;;) {
        if (xQueueReceive(s_saves, &item, portMAX_DELAY) != pdTRUE) continue;
        esp_err_t err = nvs_set_u64(s_nvs, "progress_v1", item.data);
        if (err == ESP_OK) err = nvs_commit(s_nvs);
        if (err == ESP_OK) atomic_store(&s_saved, item.version);
        else atomic_store(&s_failed, item.version);
    }
}

ip_progress_t ip_storage_init(void)
{
    if (s_initialized) return s_initial;
    s_initialized = true;
    /* Never erase NVS on initialization failure: other apps own data here. */
    if (nvs_flash_init() != ESP_OK) return s_initial;
    if (nvs_open("idiom_pet", NVS_READWRITE, &s_nvs) != ESP_OK) return s_initial;
    uint64_t packed = 0;
    esp_err_t err = nvs_get_u64(s_nvs, "progress_v1", &packed);
    if (err == ESP_OK && (packed >> 56) == 1 && !(packed & (3ULL << 54))) {
        s_initial.learned = packed & IP_PROGRESS_MASK;
        s_initial.pending = (packed >> 24) & IP_PROGRESS_MASK;
        s_initial.pets = (packed >> 48) & 63U;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(s_nvs);
        return s_initial;
    }
    s_saves = xQueueCreate(1, sizeof(save_t));
    if (!s_saves || xTaskCreate(worker, "idiom_save", 3072, NULL, 2, NULL) != pdPASS) {
        if (s_saves) vQueueDelete(s_saves);
        s_saves = NULL;
        nvs_close(s_nvs);
        return s_initial;
    }
    s_ready = true;
    return s_initial;
}

void ip_storage_save(ip_progress_t progress)
{
    if (!s_ready) return;
    save_t item = {
        .data = (1ULL << 56) | ((uint64_t)progress.pets << 48) |
                ((uint64_t)progress.pending << 24) | progress.learned,
        .version = ++s_requested,
    };
    (void)xQueueOverwrite(s_saves, &item);
}

unsigned ip_storage_status(void)
{
    if (!s_ready) return 2;
    if (atomic_load(&s_saved) == s_requested) return 0;
    if (atomic_load(&s_failed) == s_requested) return 2;
    return 1;
}
