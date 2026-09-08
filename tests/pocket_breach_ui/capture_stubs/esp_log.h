#pragma once
#define ESP_LOG_NONE 0
#define ESP_LOG_INFO 1
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
static inline void esp_log_level_set(const char *s,int level){(void)s;(void)level;}
