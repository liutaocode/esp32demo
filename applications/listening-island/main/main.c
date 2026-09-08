#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "fap_screenshot.h"
#include "apps/listening/listening_state.h"
#include "apps/listening/listening_runtime.h"
#include "apps/listening/listening_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <assert.h>
static li_state_t state;
typedef struct {unsigned key;bool held;} key_t;
static StaticQueue_t keys_control;
static uint8_t keys_buffer[16*sizeof(key_t)];
static QueueHandle_t keys;
static atomic_int battery=-1;
static void on_key(bsp_btn_t b,bsp_btn_ev_t event,void *arg) {
 (void)arg;if(event!=BSP_BTN_CLICK && event!=BSP_BTN_LONG)return;
 key_t k={(unsigned)b,event==BSP_BTN_LONG};(void)xQueueSend(keys,&k,0);
}
static void frame(lv_timer_t *timer) {
 (void)timer;static uint32_t sent,done;static unsigned frames;static int last_battery=-2,last_saving=-1;static bool last_audio;
 uint32_t now=(uint32_t)(esp_timer_get_time()/1000);bool redraw=false;
 bool audio=li_runtime_audio_ok();state.voice_enabled=audio;
 uint32_t completed=li_runtime_completed();if(completed!=done){done=completed;li_audio_done(&state,completed,now);redraw=true;}
 key_t k;while(xQueueReceive(keys,&k,0)==pdTRUE){li_key(&state,k.key,k.held,now);redraw=true;}
 uint32_t before=state.serial;li_tick(&state,now);redraw|=before!=state.serial;
 if(sent!=state.serial){sent=state.serial;li_runtime_speak(state.voice,sent,state.page==LI_LISTEN?2:1,state.progress.volume);}
 if(state.dirty){li_runtime_save(&state.progress);state.dirty=false;}
 int b=atomic_load(&battery),saving=li_runtime_save_status();
 redraw|=last_battery!=b||last_audio!=audio||last_saving!=saving;
 if(redraw||++frames==100){li_ui_render(&state,b,audio,saving);frames=0;last_battery=b;last_audio=audio;last_saving=saving;}
}
void app_main(void) {
 ESP_ERROR_CHECK(bsp_i2c_init());ESP_ERROR_CHECK(bsp_display_init());assert(bsp_lvgl_init());
 bool audio=bsp_audio_init()==ESP_OK;bool battery_ok=bsp_battery_init()==ESP_OK;
 li_progress_t p=li_runtime_start(audio);li_init(&state,&p,esp_random());state.voice_enabled=li_runtime_audio_ok();
 keys=xQueueCreateStatic(16,sizeof(key_t),keys_buffer,&keys_control);
 if(bsp_lvgl_lock(2000)){li_ui_create(&state);lv_timer_create(frame,30,NULL);bsp_lvgl_unlock();}
 ESP_ERROR_CHECK(bsp_button_init(on_key,NULL));bsp_display_backlight(80);fap_screenshot_start();
 for(;;){atomic_store(&battery,battery_ok?bsp_battery_soc():-1);vTaskDelay(pdMS_TO_TICKS(10000));}
}
