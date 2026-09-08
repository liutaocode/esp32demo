#pragma once
/* Evaluate diagnostic arguments without printing timing-dependent host output. */
static inline void ec_test_log(const char *tag,const char *format,...)
{ (void)tag; (void)format; }
#define ESP_LOGI(...) ec_test_log(__VA_ARGS__)
