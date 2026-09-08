#pragma once
typedef int esp_log_level_t;
#define ESP_LOG_NONE 0
#define ESP_LOG_INFO 3
#define ESP_LOGI(tag,...) ((void)(tag))
#define ESP_LOGE(tag,...) ((void)(tag))
esp_log_level_t esp_log_level_get(const char *);
void esp_log_level_set(const char *,esp_log_level_t);
const char *esp_err_to_name(int);
