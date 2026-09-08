#pragma once
#include "pa_hub.h"

/* 最好成绩存在 NVS 的 pocket_arcade 命名空间里,写盘在后台任务上做,
   按键回调和渲染都不会因为它阻塞。 */
pa_record_t pa_storage_init(void);
void pa_storage_save(const pa_record_t *record);
/* 0 已存好,1 正在存,2 存不了(没有 NVS 或写失败)。 */
unsigned pa_storage_status(void);
