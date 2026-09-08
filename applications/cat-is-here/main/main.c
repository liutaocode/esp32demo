#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "cat/cat_audio_runtime.h"
#include "cat/cat_control.h"
#include "cat/cat_ui.h"
#include "fap_screenshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG="cat_here";
typedef struct { bsp_btn_t key; bsp_btn_ev_t event; } input_t;
static QueueHandle_t inputs;
static cat_control_t control;
static cat_ui_t ui;
static void on_key(bsp_btn_t key,bsp_btn_ev_t event,void *user) {
    (void)user; input_t item={key,event};
    if(inputs) (void)xQueueSend(inputs,&item,0);
}
void app_main(void) {
    bsp_i2c_init();
    if(bsp_display_init()!=ESP_OK || !bsp_lvgl_init()) { ESP_LOGE(TAG,"display unavailable"); return; }
    bsp_display_backlight(75);
    inputs=xQueueCreate(24,sizeof(input_t));
    bool buttons=inputs && bsp_button_init(on_key,NULL)==ESP_OK;
    cat_audio_prepare();
    bool audio=cat_audio_ready();
    bool battery_ready=bsp_battery_init()==ESP_OK;
    int battery=battery_ready ? bsp_battery_soc() : -1;
    nvs_handle_t nvs=0;
    bool storage=nvs_flash_init()==ESP_OK && nvs_open("cat_here",NVS_READWRITE,&nvs)==ESP_OK;
    cat_profile_t saved; bool have_saved=false;
    if(storage) {
        uint8_t bytes[CAT_SAVE_BYTES]; size_t n=sizeof bytes;
        if(nvs_get_blob(nvs,"profile",bytes,&n)==ESP_OK) have_saved=cat_decode(&saved,bytes,n);
    }
    cat_control_init(&control,esp_random(),have_saved ? &saved : NULL);
    if(!bsp_lvgl_lock(2000)) { if(storage) nvs_close(nvs); return; }
    cat_ui_create(&ui,&control.cat);
    cat_ui_render(&ui,0,0,0,battery,audio,storage,buttons);
    bsp_lvgl_unlock();
    fap_screenshot_start();
    bool was_sound_enabled=control.cat.profile.sound;
    bool was_dancing=false;
    int64_t last=esp_timer_get_time(), next_frame=last, next_battery=last+30000000;
    int64_t dirty_since=0, last_change=0;
    unsigned brightness=75;
    ESP_LOGI(TAG,"ready; buttons=%d audio=%d storage=%d sound_enabled=%d",buttons,audio,storage,control.cat.profile.sound);
    /* Permanent single-screen app. This owner task handles UI, storage and battery;
       a separate worker streams audio without UI/storage gaps;
       the button task only queues events. No screen is destroyed at runtime. */
    for(;;) {
        int64_t now=esp_timer_get_time();
        if(now>=next_frame) {
            audio=cat_audio_ready();
            cat_sound_t sound=CAT_SILENT;
            if(bsp_lvgl_lock(100)) {
                uint32_t dt=(uint32_t)((now-last)/1000); last=now;
                cat_control_tick(&control,dt);
                input_t in; unsigned drained=0;
                while(inputs && drained++<24 && xQueueReceive(inputs,&in,0)==pdTRUE) {
                    cat_sound_t next=cat_control_key(&control,(unsigned)in.key,(unsigned)in.event);
                    if(next!=CAT_SILENT) sound=next;
                }
                cat_ui_render(&ui,(uint32_t)(now/1000),control.page,control.selection,battery,audio,storage,buttons);
                bsp_lvgl_unlock();
            }
            next_frame=now+80000;
            bool dancing=control.cat.pose==CAT_DANCING;
            if(was_dancing && !dancing) cat_audio_request(CAT_SILENT);
            if(sound==CAT_DANCE && !dancing) sound=CAT_SILENT;
            if(audio && control.cat.profile.sound && sound!=CAT_SILENT) cat_audio_request(sound);
            was_dancing=dancing;
            if(was_sound_enabled && !control.cat.profile.sound) cat_audio_request(CAT_SILENT);
            was_sound_enabled=control.cat.profile.sound;
            unsigned desired=cat_control_brightness(&control);
            if(!buttons && desired==0) desired=32;
            if(desired!=brightness) { bsp_display_backlight((uint8_t)desired); brightness=desired; }
            if(control.cat.dirty) {
                if(!dirty_since) dirty_since=now;
                last_change=now; control.cat.dirty=false;
            }
        }
        if(storage && dirty_since && (now-last_change>=1500000 || now-dirty_since>=5000000)) {
            uint8_t bytes[CAT_SAVE_BYTES]; cat_encode(&control.cat.profile,bytes);
            if(nvs_set_blob(nvs,"profile",bytes,sizeof bytes)!=ESP_OK || nvs_commit(nvs)!=ESP_OK) {
                storage=false; nvs_close(nvs); ESP_LOGW(TAG,"storage unavailable; session continues");
            }
            dirty_since=0;
        }
        if(now>=next_battery) { battery=battery_ready ? bsp_battery_soc() : -1; next_battery=now+30000000; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
