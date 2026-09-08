#include "listening_runtime.h"
#include "listening_catalog.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>
#include <string.h>
extern const uint8_t li_embedded_start[] __asm__("_binary_listening_a_bin_start");
extern const uint8_t li_embedded_end[] __asm__("_binary_listening_a_bin_end");
typedef struct {int id;uint32_t serial;unsigned repeats,volume;} request_t;
static QueueHandle_t requests,saves;
static StaticQueue_t request_control,save_control;
static uint8_t request_buffer[sizeof(request_t)],save_buffer[sizeof(li_progress_t)];
static atomic_bool stopped=true,stop,audio_ok;
static atomic_uint generation,completed;
static atomic_int save_status;
static bool storage_ok;
static nvs_handle_t storage;
static const esp_partition_t *resource;
static bool cancelled(const request_t *r) {return atomic_load(&stop)||r->serial!=atomic_load(&generation);}
static void save_pending(void) {
 li_progress_t p;if(xQueueReceive(saves,&p,0)!=pdTRUE)return;
 if(!storage_ok){atomic_store(&save_status,0);return;}
 esp_err_t e=nvs_set_blob(storage,"progress_v1",&p,sizeof(p));if(e==ESP_OK)e=nvs_commit(storage);
 atomic_store(&save_status,e==ESP_OK?1:3);
}
static bool write_pcm(const int16_t *pcm,size_t n) {
 if(bsp_audio_write(pcm,n*sizeof(*pcm))==ESP_OK)return true;
 ESP_LOGE("listening_io","PCM write failed");atomic_store(&audio_ok,false);return false;
}
static bool play(const request_t *r) {
 const li_clip_t *c=&li_clips[r->id];
 if(!li_clip_valid(c,li_bank_sizes[c->bank])){ESP_LOGE("listening_io","Invalid clip metadata: %d",r->id);atomic_store(&audio_ok,false);return false;}
 minecraft_adpcm_state_t decoder;minecraft_adpcm_init(&decoder,c->predictor,c->step);
 uint32_t sample=0,nibble=0,cached_base=UINT32_MAX;size_t cached_size=0;
 uint8_t compressed[256];int16_t pcm[512];
 while(sample<c->samples) {
  if(cancelled(r))return false;
  unsigned n=0;
  while(n<512 && sample<c->samples) {
   if(!sample)pcm[n++]=(int16_t)decoder.predictor;
   else {
    uint32_t byte=nibble/2;
    if(cached_base==UINT32_MAX || byte>=cached_base+cached_size) {
     cached_base=byte;cached_size=c->bytes-byte;if(cached_size>sizeof(compressed))cached_size=sizeof(compressed);
     if(c->bank==0)memcpy(compressed,li_embedded_start+c->offset+byte,cached_size);
     else if(esp_partition_read(resource,c->offset+byte,compressed,cached_size)!=ESP_OK){atomic_store(&audio_ok,false);return false;}
    }
    uint8_t b=compressed[byte-cached_base];pcm[n++]=minecraft_adpcm_decode(&decoder,(nibble&1)?b>>4:b&15);nibble++;
   }
   sample++;
  }
  if(cancelled(r)||!write_pcm(pcm,n))return false;
 }
 return true;
}
static bool silence(const request_t *r,unsigned chunks) {
 int16_t pcm[512]={0};for(unsigned i=0;i<chunks;i++)if(cancelled(r)||!write_pcm(pcm,512))return false;return true;
}
static void worker(void *arg) {
 (void)arg;
 while(!atomic_load(&stop)) {
  save_pending();request_t r;
  if(xQueueReceive(requests,&r,pdMS_TO_TICKS(40))!=pdTRUE)continue;
  if(cancelled(&r))continue;
  if(r.id<0){bsp_audio_set_volume(0);continue;}
  if(!atomic_load(&audio_ok))continue;
  if(bsp_audio_set_format(16000,16,1)!=ESP_OK){ESP_LOGE("listening_io","Codec format failed");atomic_store(&audio_ok,false);continue;}
  bsp_audio_set_volume(r.volume);bool success=true;
  for(unsigned i=0;i<r.repeats && success;i++) {success=play(&r);if(success)success=silence(&r,i+1<r.repeats?25:4);}
  bsp_audio_set_volume(0);
  if(success && !cancelled(&r))atomic_store(&completed,r.serial);
 }
 save_pending();bsp_audio_set_volume(0);if(storage_ok)nvs_close(storage);storage_ok=false;
 atomic_store(&stopped,true);vTaskDelete(NULL);
}
li_progress_t li_runtime_start(bool audio) {
 li_progress_t p={0};if(!atomic_load(&stopped))return p;
 atomic_store(&stop,false);atomic_store(&completed,0);atomic_store(&generation,0);atomic_store(&save_status,0);
 requests=xQueueCreateStatic(1,sizeof(request_t),request_buffer,&request_control);
 saves=xQueueCreateStatic(1,sizeof(li_progress_t),save_buffer,&save_control);
 resource=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,0x40,"listening");
 ESP_LOGI("listening_io","Start: board=%d resource=%d embedded=%u expected=%u",audio,resource!=NULL,(unsigned)(li_embedded_end-li_embedded_start),(unsigned)li_bank_sizes[0]);
 audio=audio && resource && resource->size>=li_bank_sizes[1] && (size_t)(li_embedded_end-li_embedded_start)==li_bank_sizes[0];
 if(audio) {
  uint8_t chunk[512];uint32_t crc=0;
  for(size_t offset=0;offset<li_bank_sizes[1];offset+=sizeof(chunk)) {
   size_t n=li_bank_sizes[1]-offset;if(n>sizeof(chunk))n=sizeof(chunk);
   if(esp_partition_read(resource,offset,chunk,n)!=ESP_OK){audio=false;break;}
   crc=esp_rom_crc32_le(crc,chunk,n);
  }
  ESP_LOGI("listening_io","Resource CRC: %08x expected %08x",(unsigned)crc,(unsigned)li_resource_crc);
  if(crc!=li_resource_crc)audio=false;
 }
 atomic_store(&audio_ok,audio);
 storage_ok=nvs_flash_init()==ESP_OK && nvs_open("listen_island",NVS_READWRITE,&storage)==ESP_OK;
 if(storage_ok){size_t n=sizeof(p);esp_err_t e=nvs_get_blob(storage,"progress_v1",&p,&n);if(e!=ESP_OK||n!=sizeof(p))p=(li_progress_t){0};atomic_store(&save_status,e==ESP_OK||e==ESP_ERR_NVS_NOT_FOUND?1:3);}
 atomic_store(&stopped,false);
 if(xTaskCreate(worker,"listening_io",4608,NULL,3,NULL)!=pdPASS){ESP_LOGE("listening_io","Worker creation failed");if(storage_ok)nvs_close(storage);storage_ok=false;atomic_store(&audio_ok,false);atomic_store(&stopped,true);atomic_store(&save_status,0);}
 return p;
}
void li_runtime_speak(int id,uint32_t serial,unsigned repeats,unsigned volume) {
 if(atomic_load(&stopped)||atomic_load(&stop)||id>=LI_COUNT)return;
 request_t r={id,serial,repeats>2?2:repeats,volume>100?100:volume};atomic_store(&generation,serial);xQueueOverwrite(requests,&r);
}
void li_runtime_save(const li_progress_t *p) {if(!atomic_load(&stopped)&&!atomic_load(&stop)){atomic_store(&save_status,2);xQueueOverwrite(saves,p);}}
bool li_runtime_audio_ok(void){return atomic_load(&audio_ok);}
uint32_t li_runtime_completed(void){return atomic_load(&completed);}
int li_runtime_save_status(void){return atomic_load(&save_status);}
void li_runtime_shutdown(void){atomic_store(&stop,true);}
bool li_runtime_stopped(void){return atomic_load(&stopped);}
