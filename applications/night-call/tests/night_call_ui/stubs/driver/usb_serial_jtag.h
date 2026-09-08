#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef struct { unsigned rx_buffer_size, tx_buffer_size; } usb_serial_jtag_driver_config_t;
bool usb_serial_jtag_is_driver_installed(void);
esp_err_t usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *);
int usb_serial_jtag_write_bytes(const void *,size_t,int);
int usb_serial_jtag_read_bytes(void *,size_t,int);
