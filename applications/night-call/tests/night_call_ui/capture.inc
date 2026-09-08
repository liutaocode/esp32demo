/* Exercise the actual USB screenshot source against an actual LVGL partial
 * renderer. This does not open any serial device on the host. */
#include "fap_screenshot.c"
static bool capture_lock, deny_lock, usb_installed;
static int capture_fail_after=-1;
static unsigned serial_count;
static uint8_t serial_bytes[FAP_SNAP_BYTES+128], expected_pixels[FAP_SNAP_BYTES];
static uint8_t render_strip[240*20*2];
bool bsp_lvgl_lock(unsigned timeout) { (void)timeout; if(deny_lock) return false; assert(!capture_lock);capture_lock=true;return true; }
void bsp_lvgl_unlock(void) { assert(capture_lock);capture_lock=false; }
bool usb_serial_jtag_is_driver_installed(void) { return usb_installed; }
int usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *c) { assert(c->rx_buffer_size==256);usb_installed=true;return 0; }
void usb_serial_jtag_vfs_use_driver(void) {}
int usb_serial_jtag_write_bytes(const void *p,size_t n,int timeout) {
    assert(capture_lock && timeout==2000);
    if(capture_fail_after>=0 && serial_count>=(unsigned)capture_fail_after) return 0;
    if(n>37) n=37; /* Partial USB writes must not duplicate/skip bytes. */
    assert(serial_count+n<=sizeof(serial_bytes));memcpy(serial_bytes+serial_count,p,n);serial_count+=(unsigned)n;return (int)n;
}
int usb_serial_jtag_read_bytes(void *p,size_t n,int timeout) { (void)p;(void)n;(void)timeout;return 0; }
void vTaskDelay(unsigned n) { (void)n; }
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle) {
    (void)entry;(void)name;(void)arg;(void)handle;assert(stack==8192 && priority==3);return pdPASS;
}
int esp_log_level_get(const char *tag) { (void)tag;return 3; }
void esp_log_level_set(const char *tag,int level) { (void)tag;assert(level==0 || level==3); }
const char *esp_err_to_name(int e) { (void)e;return "test error"; }
static void fake_panel_flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) {
    unsigned stride=lv_display_get_buf_active(display)->header.stride;
    for(int y=area->y1;y<=area->y2;y++) {
        memcpy(expected_pixels+(y*240+area->x1)*2,pixels+(y-area->y1)*stride,(area->x2-area->x1+1)*2);
    }
    lv_display_flush_ready(display);
}
static void test_capture(void) {
    lv_display_t *display=lv_display_get_default();
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,render_strip,NULL,sizeof(render_strip),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,fake_panel_flush);fap_screenshot_start();
    const char *header="FAP_SCREENSHOT_V1 240 320 RGB565LE 153600\n";
    for(unsigned n=0;n<nc_node_count;n++) {
        s_state.node=n;s_state.chapter=nc_nodes[n].chapter;s_page=STORY;s_selected=0;render();
        serial_count=0;send_screen();assert(!capture_lock && !s_capturing);
        assert(s_capture_ok && s_capture_row==320);
        assert(serial_count==strlen(header)+FAP_SNAP_BYTES && !memcmp(serial_bytes,header,strlen(header)));
        assert(!memcmp(serial_bytes+strlen(header),expected_pixels,FAP_SNAP_BYTES));
    }
    serial_count=0;capture_fail_after=1200;send_screen();assert(!s_capture_ok && !capture_lock);
    capture_fail_after=-1;serial_count=0;send_screen();assert(s_capture_ok && serial_count==strlen(header)+FAP_SNAP_BYTES);
    deny_lock=true;serial_count=0;send_screen();assert(!serial_count);deny_lock=false;
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);serial_count=0;send_screen();assert(!serial_count && !capture_lock);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    home();
    puts("Night Call screenshot streaming: all 22 screens match panel pixels, short writes, failure recovery, lock and format guards PASS");
}
