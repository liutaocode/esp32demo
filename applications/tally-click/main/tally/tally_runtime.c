#include "tally_runtime.h"
#include "tally_state.h"
#include "tally_ui.h"
#include "tally_sound.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>
typedef struct { tc_button button; tc_event event; int64_t at; } input;
static StaticQueue_t queue_control;
static uint8_t queue_storage[64*sizeof(input)];
static QueueHandle_t queue;
static atomic_bool accepting, overflow;
static bool buttons_available;
static tc_state state;
static tc_feedback pending_fx;
static void feedback(tc_feedback f)
{
    pending_fx=f;
    tc_audio_request(f);
}
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static void draw(tc_notice notice,int battery)
{
    if (bsp_lvgl_lock(1000)) {
        tc_ui_render(&state,notice,battery,now_ms());
        tc_ui_feedback(pending_fx);
        pending_fx=TC_FX_NONE;
        bsp_lvgl_unlock();
    }
}
static bool save(nvs_handle_t h,tc_data *data)
{
    tc_seal(data);
    return nvs_set_blob(h,"snapshot",data,sizeof(*data))==ESP_OK && nvs_commit(h)==ESP_OK;
}
static void worker(void *arg)
{
    (void)arg;
    nvs_handle_t h=0;
    tc_data saved;
    bool loaded=false, storage_ok=false;
    esp_err_t e=nvs_flash_init();
    if (e==ESP_OK && nvs_open("tally_click",NVS_READWRITE,&h)==ESP_OK) {
        size_t size=sizeof(saved);
        e=nvs_get_blob(h,"snapshot",&saved,&size);
        loaded=e==ESP_OK && size==sizeof(saved) && tc_valid(&saved);
        storage_ok=loaded || e==ESP_ERR_NVS_NOT_FOUND;
    }
    tc_init(&state,loaded ? &saved : NULL);
    tc_notice notice=storage_ok ? TC_STORED : TC_RECOVER_ERROR;
    if (!buttons_available) notice=TC_BUTTON_ERROR;
    int battery=bsp_battery_soc();
    draw(notice,battery);
    atomic_store(&accepting,buttons_available && storage_ok);
    bool dirty=loaded && saved.version==1;
    int64_t dirty_since=0,last_change=0,last_battery=now_ms(),guard=0;
    for (;;) {
        input in;
        bool redraw=false;
        if (xQueueReceive(queue,&in,pdMS_TO_TICKS(40))==pdTRUE && in.at>guard) {
            tc_page previous_page=state.page;
            tc_effect effect=tc_input(&state,in.button,in.event);
            redraw=effect!=TC_NONE;
            if (effect!=TC_NONE && notice==TC_INPUT_ERROR) notice=dirty ? TC_PENDING : TC_STORED;
            if (effect!=TC_NONE && state.page==TC_PAUSE && dirty) last_change-=1000;
            if (effect==TC_ARCHIVE) {
                draw(TC_SAVING,battery);
                tc_data candidate;
                tc_candidate(&state,&candidate);
                if (save(h,&candidate)) {
                    state.data=candidate; state.page=TC_COUNT;
                    dirty=false; notice=TC_ARCHIVED; feedback(TC_FX_ARCHIVE);
                } else { state.page=TC_PAUSE; notice=TC_SAVE_ERROR; feedback(TC_FX_ERROR); }
                /* Discard inputs made during the visible archive operation. */
                guard=now_ms();
            } else if (effect==TC_DIRTY) {
                if (!dirty) dirty_since=now_ms();
                dirty=true; last_change=now_ms(); notice=TC_PENDING;
            } else if (effect!=TC_NONE && notice==TC_ARCHIVED) notice=TC_STORED;
            if(effect!=TC_NONE && effect!=TC_ARCHIVE) {
                tc_feedback f=TC_FX_CONFIRM;
                if(effect==TC_DIRTY) f=in.button==TC_DOWN ? TC_FX_ADD:TC_FX_SUBTRACT;
                else if(previous_page==TC_COUNT && in.button!=TC_OK) f=TC_FX_LIMIT;
                else if(previous_page==TC_COUNT && state.page==TC_PAUSE) f=TC_FX_PAUSE;
                else if(previous_page==TC_PAUSE && state.page==TC_COUNT) f=TC_FX_RESUME;
                else if(in.button==TC_UP) f=TC_FX_UP;
                else if(in.button==TC_DOWN) f=TC_FX_DOWN;
                feedback(f);
            }
        }
        int64_t now=now_ms();
        if (atomic_exchange(&overflow,false)) {
            if (state.page==TC_COUNT) state.page=TC_PAUSE;
            notice=TC_INPUT_ERROR; redraw=true; feedback(TC_FX_ERROR);
            /* Keep the warning latched until a deliberate next input. */
            xQueueReset(queue); guard=now;
        }
        if (dirty && storage_ok && tc_audio_quiet() && (now-last_change>=800 || now-dirty_since>=2000)) {
            tc_data snapshot=state.data;
            if (save(h,&snapshot)) {
                state.data=snapshot; dirty=false;
                if (notice!=TC_INPUT_ERROR) notice=TC_STORED;
            } else { notice=TC_SAVE_ERROR; last_change=now; dirty_since=now; }
            redraw=true;
        }
        if (now-last_battery>=30000) { battery=bsp_battery_soc(); last_battery=now; redraw=true; }
        if (redraw) draw(notice,battery);
    }
}
void tally_key(bsp_btn_t b,bsp_btn_ev_t ev)
{
    if (!atomic_load(&accepting)) return;
    if (b!=BSP_BTN_OK && ev!=BSP_BTN_PRESS) return;
    if (b==BSP_BTN_OK && ev==BSP_BTN_PRESS) return;
    input in={(tc_button)b,(tc_event)ev,now_ms()};
    if (xQueueSend(queue,&in,0)!=pdTRUE) atomic_store(&overflow,true);
}
void tally_start(bool buttons_ok)
{
    buttons_available=buttons_ok;
    queue=xQueueCreateStatic(64,sizeof(input),queue_storage,&queue_control);
    if (xTaskCreate(worker,"tally",6144,NULL,4,NULL)!=pdPASS) {
        tc_init(&state,NULL);
        draw(TC_RECOVER_ERROR,-1);
    }
}
