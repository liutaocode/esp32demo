#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fap_screenshot.c"
static uint8_t buffer[240*20*2],captured[240*320*2+128],expected[240*320*2];
static size_t received;
static lv_display_t *display;
static lv_obj_t *patch;
static bool locked;
bool bsp_lvgl_lock(int timeout){(void)timeout;assert(!locked);locked=true;return true;}
void bsp_lvgl_unlock(void){assert(locked);locked=false;}
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *data)
{
    /* Match the real display port: swap the same DMA buffer in place. */
    for(unsigned i=0;i<lv_area_get_size(area)*2;i+=2){uint8_t b=data[i];data[i]=data[i+1];data[i+1]=b;}
    lv_display_flush_ready(d);
}
int usb_serial_jtag_write_bytes(const void *data,size_t size,unsigned timeout)
{
    (void)timeout;assert(locked);
    if(size>31)size=31;assert(received+size<=sizeof(captured));
    memcpy(captured+received,data,size);received+=size;
    return (int)size;
}
int main(void)
{
    lv_init();display=lv_display_create(240,320);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lv_obj_t *screen=lv_obj_create(NULL);lv_obj_remove_style_all(screen);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x183850),0);lv_screen_load(screen);
    patch=lv_obj_create(screen);lv_obj_remove_style_all(patch);lv_obj_set_pos(patch,17,63);lv_obj_set_size(patch,46,37);
    lv_obj_set_style_bg_opa(patch,LV_OPA_COVER,0);lv_obj_set_style_bg_color(patch,lv_color_hex(0xff0000),0);
    fap_screenshot_start();lv_refr_now(display);
    lv_obj_set_style_bg_color(patch,lv_color_hex(0x00ff00),0);lv_refr_now(display);
    send_screen();
    assert(!locked && s_mode==FAP_MODE_IDLE);
    uint8_t *newline=memchr(captured,'\n',received);assert(newline);
    uint8_t *payload=newline+1;
    assert(received-(size_t)(payload-captured)==sizeof(expected));
    const char *header="FAP_SCREENSHOT_V1 240 320 RGB565LE 153600\n";
    assert((size_t)(payload-captured)==strlen(header));
    assert(memcmp(captured,header,strlen(header))==0);
    /* The real port swaps DMA bytes after the capture event. Wire data must
       remain little-endian even with short serial writes. */
    for(int y=64;y<99;y++)for(int x=18;x<62;x++){
        unsigned offset=(y*240+x)*2;
        assert(payload[offset]==0xe0&&payload[offset+1]==0x07);
    }
    memcpy(expected,payload,sizeof(expected));received=0;send_screen();
    newline=memchr(captured,'\n',received);assert(newline);payload=newline+1;
    assert(received-(size_t)(payload-captured)==sizeof(expected));
    assert(memcmp(payload,expected,sizeof(expected))==0);
    assert(!locked && s_mode==FAP_MODE_IDLE);
    puts("Screen capture: streamed RGB565, byte-swap, short writes and UI lock PASS");
}
