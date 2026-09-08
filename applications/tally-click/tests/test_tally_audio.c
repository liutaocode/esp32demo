#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../main/tally/tally_audio.c"
static jmp_buf stopped;
static int mode,writes,volume,overwrites;
static void (*entry_point)(void *);
static bool in_request;
static tc_mixer expected;
esp_err_t bsp_audio_init(void) { assert(!in_request);return mode==1 ? ESP_FAIL:ESP_OK; }
esp_err_t bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t ch)
{ assert(!in_request && hz==16000 && bits==16 && ch==1);return ESP_OK; }
void bsp_audio_set_volume(uint8_t v) { assert(!in_request);volume=v; }
void vTaskDelete(void *task) { assert(task==NULL && !atomic_load(&ready));longjmp(stopped,1); }
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle)
{ (void)name;(void)stack;(void)arg;(void)handle;assert(priority==6);entry_point=entry;return pdPASS; }
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,void *storage,StaticQueue_t *q)
{ (void)storage;assert(n==1 && size==sizeof(tc_feedback));q->count=0;q->size=size;return q; }
int xQueueOverwrite(QueueHandle_t q,const void *p)
{ assert(in_request);memcpy(q->data[0],p,q->size);q->count=1;++overwrites;return pdTRUE; }
static void request(tc_feedback sound)
{ in_request=true;tc_audio_request(sound);in_request=false; }
int xQueueReceive(QueueHandle_t q,void *p,unsigned timeout)
{
    assert(!in_request && timeout==0); /* No stopped/empty DMA between cues. */
    if(writes==0)request(TC_FX_ADD);
    if(writes==25) { assert(tc_audio_quiet());longjmp(stopped,1); }
    if(!q->count)return 0;
    memcpy(p,q->data[0],q->size);q->count=0;tc_mixer_request(&expected,*(tc_feedback *)p);return pdTRUE;
}
esp_err_t bsp_audio_write(const void *data,size_t bytes)
{
    assert(!in_request && bytes==TC_AUDIO_FRAMES*2 && volume==66);
    int16_t reference[TC_AUDIO_FRAMES];tc_mixer_render(&expected,reference,TC_AUDIO_FRAMES);
    assert(!memcmp(data,reference,bytes));++writes;
    if(mode==2)return ESP_FAIL;
    if(writes==2) {
        request(TC_FX_ADD);request(TC_FX_PAUSE);request(TC_FX_SUBTRACT);
        assert(!tc_audio_quiet());
    }
    if(writes>16)for(unsigned i=0;i<TC_AUDIO_FRAMES;i++)assert(((const int16_t *)data)[i]==0);
    return ESP_OK;
}
int main(int argc,char **argv)
{
    assert(argc==2);mode=atoi(argv[1]);tc_audio_start();request(TC_FX_ADD);assert(!sounds->count);
    if(!setjmp(stopped))entry_point(NULL);
    if(mode==0)assert(overwrites==4 && atomic_load(&ready));
    else { assert(!atomic_load(&ready) && tc_audio_quiet());int before=overwrites;request(TC_FX_ADD);assert(overwrites==before); }
    printf("Tally audio worker %d: continuous PCM stream, smooth latest-cue switching and failure handling PASS\n",mode);
}
