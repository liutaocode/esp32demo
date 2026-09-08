#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef struct {unsigned rx_buffer_size,tx_buffer_size;} usb_serial_jtag_driver_config_t;
static inline bool usb_serial_jtag_is_driver_installed(void){return true;}
static inline int usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *c){(void)c;return 0;}
static inline const char *esp_err_to_name(int e){(void)e;return "error";}
static inline int usb_serial_jtag_read_bytes(void *out,size_t size,unsigned timeout){(void)out;(void)size;(void)timeout;return 0;}
int usb_serial_jtag_write_bytes(const void *data,size_t size,unsigned timeout);
