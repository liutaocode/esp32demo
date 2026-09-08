/* Run the production worker against a deterministic queue and durable NVS fake. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../main/tally/tally_runtime.c"
static int scenario,stage,commits,writes,fail_writes,fail_commits;
static bool button_context,locked,has_disk;
static int64_t fake_time;
static tc_data disk,pending;
static tc_notice displayed;
static tc_feedback last_sound,last_fx;
bool tc_audio_quiet(void) { return scenario!=7 || fake_time>=4000; }
void tc_audio_request(tc_feedback f) { assert(!button_context && !locked); last_sound=f; }
void tc_ui_feedback(tc_feedback f) { assert(locked && !button_context); if(f!=TC_FX_NONE)last_fx=f; }
static tc_state visible;
static jmp_buf stopped;
static void (*task_entry)(void *);
int64_t esp_timer_get_time(void) { return fake_time*1000; }
bool bsp_lvgl_lock(unsigned timeout) { assert(!button_context && timeout==1000 && !locked); locked=true; return true; }
void bsp_lvgl_unlock(void) { assert(locked); locked=false; }
int bsp_battery_soc(void) { assert(!button_context && !locked); return -1; }
void tc_ui_render(const tc_state *s,tc_notice n,int battery,int64_t now)
{ assert(locked && !button_context && battery==-1 && now==fake_time); visible=*s; displayed=n; }
esp_err_t nvs_flash_init(void) { return scenario==5 ? ESP_FAIL : ESP_OK; }
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h)
{ assert(!button_context && !strcmp(name,"tally_click") && mode==NVS_READWRITE); *h=1; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *data,size_t *size)
{
    assert(h==1 && !strcmp(key,"snapshot") && *size==sizeof(disk));
    if (!has_disk) return ESP_ERR_NVS_NOT_FOUND;
    memcpy(data,&disk,sizeof(disk)); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size)
{
    assert(!button_context && !locked && h==1 && !strcmp(key,"snapshot") && size==sizeof(disk));
    ++writes; if (fail_writes) return ESP_FAIL;
    memcpy(&pending,data,size); assert(tc_valid(&pending)); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h)
{ assert(!button_context && !locked && h==1); ++commits; if(fail_commits)return ESP_FAIL; disk=pending; has_disk=true; return ESP_OK; }
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle)
{ (void)name;(void)stack;(void)arg;(void)priority;(void)handle; task_entry=entry; return pdPASS; }
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,void *storage,StaticQueue_t *q)
{ (void)storage; assert(n<=64 && size<=32); q->capacity=n; q->size=size; q->count=0; return q; }
int xQueueSend(QueueHandle_t q,const void *p,unsigned timeout)
{ assert(timeout==0); if(q->count==q->capacity)return 0; memcpy(q->data[q->count++],p,q->size); return pdTRUE; }
void xQueueReset(QueueHandle_t q) { q->count=0; }
static void press(bsp_btn_t b,bsp_btn_ev_t e)
{ button_context=true; tally_key(b,e); button_context=false; }
static void script(void)
{
    if (scenario>=4 && scenario<=6) {
        assert(!atomic_load(&accepting));
        assert(displayed==(scenario==6 ? TC_BUTTON_ERROR : TC_RECOVER_ERROR));
        assert(writes==0); press(BSP_BTN_DOWN,BSP_BTN_PRESS); assert(queue->count==0); longjmp(stopped,1);
    }
    if(stage==0) { assert(state.page==TC_COUNT);stage=6; }
    if (scenario==0) {
        if(stage==6) {
            for(int i=0;i<50;i++) { press(BSP_BTN_DOWN,BSP_BTN_PRESS); press(BSP_BTN_DOWN,BSP_BTN_DOUBLE); }
            ++stage; return;
        }
        if(stage==7 && queue->count==0) { assert(state.data.count==50 && last_sound==TC_FX_ADD && last_fx==TC_FX_ADD); press(BSP_BTN_OK,BSP_BTN_LONG); ++stage; return; }
        if(stage==8) {
            assert(state.data.count==0 && state.data.length==1 && disk.records[0].count==50 && displayed==TC_ARCHIVED);
            press(BSP_BTN_DOWN,BSP_BTN_PRESS); ++stage; return;
        }
        if(stage==9 && fake_time>5000) {
            assert(disk.count==1 && disk.records[0].count==50 && tc_valid(&disk));
            tc_state reboot; tc_init(&reboot,&disk); assert(reboot.data.count==1 && reboot.data.length==1 && reboot.page==TC_COUNT);
            longjmp(stopped,1);
        }
    } else if(scenario==1 || scenario==2) {
        if(stage==6) { press(BSP_BTN_DOWN,BSP_BTN_PRESS); ++stage; return; }
        if(stage==7) { if(scenario==1)fail_writes=1;else fail_commits=1; press(BSP_BTN_OK,BSP_BTN_LONG); ++stage; return; }
        if(stage==8 && fake_time>3500) {
            assert(state.data.count==1 && state.data.length==0 && state.page==TC_PAUSE);
            assert(displayed==TC_SAVE_ERROR && disk.length==0);
            assert(writes<8); /* No 25 Hz flash retries while paused. */
            fail_writes=fail_commits=0; press(BSP_BTN_OK,BSP_BTN_LONG); ++stage; return;
        }
        if(stage==9) {
            assert(last_sound==TC_FX_ARCHIVE && last_fx==TC_FX_ARCHIVE);
            assert(disk.count==0 && disk.length==1 && disk.records[0].count==1 && displayed==TC_ARCHIVED);
            longjmp(stopped,1);
        }
    } else if(scenario==7) {
        if(stage==6) { press(BSP_BTN_DOWN,BSP_BTN_PRESS);++stage;return; }
        if(fake_time<4000)assert(writes==0);
        if(fake_time>=4200) { assert(disk.count==1 && writes==1);longjmp(stopped,1); }
    } else if(scenario==3) {
        if(stage==6) { for(int i=0;i<70;i++)press(BSP_BTN_DOWN,BSP_BTN_PRESS); ++stage; return; }
        if(stage==7) {
            assert(displayed==TC_INPUT_ERROR && state.page==TC_PAUSE && queue->count==0);
            press(BSP_BTN_OK,BSP_BTN_CLICK); ++stage; return;
        }
        if(stage==8) { assert(state.page==TC_COUNT && displayed!=TC_INPUT_ERROR); longjmp(stopped,1); }
    }
}
int xQueueReceive(QueueHandle_t q,void *p,unsigned timeout)
{
    assert(!button_context && !locked && timeout==40);
    fake_time+=40; assert(fake_time<15000);
    if(fake_time%200==0)script();
    if(!q->count)return 0;
    memcpy(p,q->data[0],q->size); memmove(q->data,q->data+1,(--q->count)*32); return pdTRUE;
}
int main(int argc,char **argv)
{
    assert(argc==2); scenario=atoi(argv[1]);
    if(scenario==4) { memset(&disk,0x5a,sizeof(disk)); has_disk=true; }
    tally_start(scenario!=6);
    if(!setjmp(stopped)) task_entry(NULL);
    printf("Tally production worker scenario %d: PASS\n",scenario);
}
