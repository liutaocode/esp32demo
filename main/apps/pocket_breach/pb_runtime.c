#include "pocket_breach.h"
#include "pb_game.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>
static QueueHandle_t queue;
static atomic_bool active, audio_ok;
static atomic_int battery=-1;
static void worker(void *arg)
{
    (void)arg;int16_t pcm[160];unsigned sound;TickType_t last=0;
    for(;;){
        TickType_t now=xTaskGetTickCount();
        if(!last||now-last>=pdMS_TO_TICKS(10000)){atomic_store(&battery,bsp_battery_soc());last=now;}
        if(xQueueReceive(queue,&sound,pdMS_TO_TICKS(30))!=pdTRUE)continue;
        if(!atomic_load(&active)||!atomic_load(&audio_ok))continue;
        for(unsigned offset=0;offset<pb_sound_samples(sound)&&atomic_load(&active);offset+=160){
            pb_pcm(sound,offset,pcm,160);
            if(bsp_audio_write(pcm,sizeof(pcm))!=ESP_OK){atomic_store(&audio_ok,false);break;}
        }
    }
}
void pb_runtime_start(bool available)
{
    if(queue)return;
    atomic_store(&audio_ok,available&&bsp_audio_set_format(16000,16,1)==ESP_OK);
    if(atomic_load(&audio_ok))bsp_audio_set_volume(55);
    queue=xQueueCreate(3,sizeof(unsigned));
    if(!queue){atomic_store(&audio_ok,false);return;}
    if(xTaskCreate(worker,"breach_audio",3072,NULL,3,NULL)!=pdPASS){vQueueDelete(queue);queue=NULL;atomic_store(&audio_ok,false);}
}
void pb_runtime_active(bool enabled){atomic_store(&active,enabled);if(queue&&!enabled)xQueueReset(queue);}
void pb_runtime_sound(unsigned sound){if(queue&&sound)(void)xQueueSend(queue,&sound,0);}
int pb_runtime_battery(void){return atomic_load(&battery);}
bool pb_runtime_audio_ok(void){return atomic_load(&audio_ok);}
