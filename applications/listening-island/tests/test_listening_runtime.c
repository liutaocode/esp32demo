#define _POSIX_C_SOURCE 200809L
#include "listening_runtime.h"
#include "listening_catalog.h"
#include "freertos/queue.h"
#include "esp_partition.h"
#include "nvs.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Mac host labels match the explicit asm names in production runtime code. */
__asm__(".section __TEXT,__const\n.globl _binary_listening_a_bin_start\n_binary_listening_a_bin_start:\n.incbin \"assets/music/listening/listening_a.bin\"\n.globl _binary_listening_a_bin_end\n_binary_listening_a_bin_end:\n");
static int failure;static pthread_t owner,thread;static void (*entry_fn)(void *);
static atomic_uint writes,volume,commits;static uint8_t *resource_bytes;static esp_partition_t resource;
static void delay(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
static void worker_only(void){assert(!pthread_equal(owner,pthread_self()));}
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,void *buf,StaticQueue_t *q){(void)buf;assert(n==1&&size<=sizeof(q->data));pthread_mutex_init(&q->lock,NULL);q->size=size;q->pending=false;return q;}
int xQueueReceive(QueueHandle_t q,void *out,int ticks){
 for(int i=0;;i++){pthread_mutex_lock(&q->lock);bool yes=q->pending;if(yes){memcpy(out,q->data,q->size);q->pending=false;}pthread_mutex_unlock(&q->lock);if(yes)return 1;if(i>=ticks)return 0;delay();}
}
void xQueueOverwrite(QueueHandle_t q,const void *in){pthread_mutex_lock(&q->lock);memcpy(q->data,in,q->size);q->pending=true;pthread_mutex_unlock(&q->lock);}
static void *run(void *arg){entry_fn(arg);return NULL;}
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned pri,void *handle){(void)name;(void)handle;assert(stack==4608&&pri==3);if(failure==4)return 0;entry_fn=fn;assert(!pthread_create(&thread,NULL,run,arg));return 1;}
void vTaskDelete(void *task){assert(!task);pthread_exit(NULL);}
const esp_partition_t *esp_partition_find_first(unsigned type,unsigned subtype,const char *name){assert(type==1&&subtype==0x40&&!strcmp(name,"listening"));return failure==3?NULL:&resource;}
int esp_partition_read(const esp_partition_t *p,size_t offset,void *out,size_t size){assert(p==&resource&&offset+size<=p->size&&size<=512);memcpy(out,resource_bytes+offset,size);return 0;}
uint32_t esp_rom_crc32_le(uint32_t crc,const uint8_t *p,uint32_t n){crc=~crc;for(unsigned i=0;i<n;i++){crc^=p[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&-(crc&1));}return ~crc;}
int bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t ch){worker_only();assert(hz==16000&&bits==16&&ch==1);return failure==1?-1:0;}
void bsp_audio_set_volume(uint8_t v){worker_only();assert(v<=100);atomic_store(&volume,v);}
int bsp_audio_write(const void *pcm,size_t n){worker_only();assert(pcm&&n>0&&n<=1024);atomic_fetch_add(&writes,1);delay();return failure==2?-1:0;}
int nvs_flash_init(void){return 0;}
int nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"listen_island")&&mode==NVS_READWRITE);*h=1;return 0;}
int nvs_get_blob(nvs_handle_t h,const char *name,void *data,size_t *size){(void)h;(void)name;(void)data;(void)size;return ESP_ERR_NVS_NOT_FOUND;}
int nvs_set_blob(nvs_handle_t h,const char *name,const void *data,size_t size){worker_only();assert(h==1&&!strcmp(name,"progress_v1")&&size==sizeof(li_progress_t)&&data);return failure==5?-1:0;}
int nvs_commit(nvs_handle_t h){worker_only();assert(h==1);atomic_fetch_add(&commits,1);return 0;}
void nvs_close(nvs_handle_t h){assert(h==1);}
static void wait_completed(unsigned id){for(unsigned i=0;i<3000&&li_runtime_completed()!=id;i++)delay();assert(li_runtime_completed()==id);}
int main(int argc,char **argv){
 assert(argc==2);failure=atoi(argv[1]);owner=pthread_self();resource.size=li_bank_sizes[1];resource_bytes=malloc(resource.size);assert(resource_bytes);
 FILE *f=fopen("assets/music/listening/listening_b.bin","rb");assert(f);assert(fread(resource_bytes,1,resource.size,f)==resource.size);fclose(f);if(failure==6)resource_bytes[0]^=1;
 (void)li_runtime_start(true);
 if(failure==4){assert(li_runtime_stopped()&&!li_runtime_audio_ok());free(resource_bytes);puts("Worker allocation failure: PASS");return 0;}
 li_runtime_speak(0,1,2,45);
 if(!failure||failure==5){
  for(unsigned i=0;i<3000&&!atomic_load(&writes);i++)delay();assert(atomic_load(&writes));
  li_runtime_speak(-1,2,1,45);for(unsigned i=0;i<3000&&atomic_load(&volume);i++)delay();assert(!atomic_load(&volume)&&li_runtime_completed()!=1);
  li_runtime_speak(LI_COUNT-1,3,1,100);wait_completed(3);assert(li_runtime_audio_ok());
  li_progress_t p={.version=LI_SAVE_VERSION};li_runtime_save(&p);for(unsigned i=0;i<3000&&li_runtime_save_status()==2;i++)delay();
  assert(li_runtime_save_status()==(failure==5?3:1));assert(atomic_load(&commits)==(failure==5?0:1));
 }else{for(unsigned i=0;i<3000&&li_runtime_audio_ok();i++)delay();assert(!li_runtime_audio_ok());assert(li_runtime_completed()==0);}
 li_runtime_shutdown();for(unsigned i=0;i<3000&&!li_runtime_stopped();i++)delay();assert(li_runtime_stopped());pthread_join(thread,NULL);assert(atomic_load(&volume)==0);free(resource_bytes);
 printf("Audio/storage worker scenario %d: PASS\n",failure);
}
