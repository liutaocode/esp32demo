#include "rhythm_service.h"
#include "bsp_battery.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdatomic.h>
static atomic_int soc = -1;
static atomic_uint best, pending_best;
static atomic_bool saved;
static QueueHandle_t queue;
static StaticQueue_t control;
static uint8_t storage[sizeof(unsigned)];
static void worker(void *unused) {
    (void)unused;
    /* Lifetime service; never accesses UI objects, never erases a shared NVS. */
    nvs_handle_t handle=0;
    bool nvs_ok=nvs_flash_init()==ESP_OK && nvs_open("rhythm_agent",NVS_READWRITE,&handle)==ESP_OK;
    uint32_t value=0;
    if (nvs_ok && nvs_get_u32(handle,"best_v1",&value)==ESP_OK && value<=800) atomic_store(&best,value);
    atomic_store(&saved,nvs_ok);
    for (;;) {
        atomic_store(&soc,bsp_battery_soc());
        unsigned signal;
        if (xQueueReceive(queue,&signal,pdMS_TO_TICKS(10000))!=pdTRUE) continue;
        unsigned score=atomic_load(&pending_best);
        if (score>atomic_load(&best) && score<=800) {
            atomic_store(&best,score);
            bool ok=nvs_ok && nvs_set_u32(handle,"best_v1",score)==ESP_OK && nvs_commit(handle)==ESP_OK;
            atomic_store(&saved,ok);
        }
    }
}
void ra_service_start(void) {
    if (queue) return;
    queue=xQueueCreateStatic(1,sizeof(unsigned),storage,&control);
    if (xTaskCreate(worker,"rhythm_store",3072,NULL,1,NULL)!=pdPASS) queue=NULL;
}
int ra_service_soc(void) { return atomic_load(&soc); }
unsigned ra_service_best(void) { return atomic_load(&best); }
bool ra_service_saved(void) { return atomic_load(&saved); }
void ra_service_save(unsigned score) {
    if(!queue || score>800) return;
    unsigned old=atomic_load(&pending_best);
    while(score>old && !atomic_compare_exchange_weak(&pending_best,&old,score)) {}
    unsigned signal=1; (void)xQueueOverwrite(queue,&signal);
}
