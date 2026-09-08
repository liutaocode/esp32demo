#include "night_call_storage.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
typedef struct { uint8_t data[NC_SAVE_SIZE]; unsigned generation; } save_t;
static StaticQueue_t s_control;
static uint8_t s_buffer[sizeof(save_t)];
static QueueHandle_t s_queue;
static nvs_handle_t s_handle;
static atomic_uint s_requested, s_completed;
static atomic_bool s_failed;
static void worker(void *arg) {
    (void)arg; save_t r;
    for (;;) {
        if (xQueueReceive(s_queue,&r,portMAX_DELAY)!=pdTRUE) continue;
        esp_err_t result=nvs_set_blob(s_handle,"story",r.data,sizeof(r.data));
        if(result==ESP_OK) result=nvs_commit(s_handle);
        atomic_store(&s_failed,result!=ESP_OK);
        atomic_store(&s_completed,r.generation);
    }
}
void nc_storage_start(nc_state_t *s) {
    nc_init(s);
    if(nvs_flash_init()!=ESP_OK || nvs_open("nightcall",NVS_READWRITE,&s_handle)!=ESP_OK) {
        atomic_store(&s_failed,true); return;
    }
    uint8_t data[NC_SAVE_SIZE]; size_t size=sizeof(data);
    esp_err_t result=nvs_get_blob(s_handle,"story",data,&size);
    if(result==ESP_OK) { if(!nc_decode(s,data,size)) atomic_store(&s_failed,true); }
    else if(result!=ESP_ERR_NVS_NOT_FOUND) atomic_store(&s_failed,true);
    s_queue=xQueueCreateStatic(1,sizeof(save_t),s_buffer,&s_control);
    if(xTaskCreate(worker,"night_save",3072,NULL,2,NULL)!=pdPASS) {
        s_queue=NULL; atomic_store(&s_failed,true); nvs_close(s_handle);
    }
}
void nc_storage_save(const nc_state_t *s) {
    if(!s_queue) return;
    save_t r; nc_encode(s,r.data); r.generation=atomic_fetch_add(&s_requested,1)+1;
    (void)xQueueOverwrite(s_queue,&r);
}
int nc_storage_status(void) {
    if(atomic_load(&s_failed)) return -1;
    return atomic_load(&s_requested)!=atomic_load(&s_completed) ? 1 : 0;
}
