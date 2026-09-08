// main/apps/ebook/ebook_net.h —— Wi-Fi 传书:联网 + 一个极小的 HTTP 文件服务。
// 只在传书页停留期间运行,离开页面立刻停掉,把射频与 HTTP 的内存还给系统。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EB_NET_IDLE = 0,
    EB_NET_CONNECTING,   // 用已保存的凭据连路由器
    EB_NET_STA_READY,    // 已连上路由器
    EB_NET_AP_READY,     // 没有凭据或连不上,自己开热点
    EB_NET_FAILED,       // 射频起不来
} eb_net_state_t;

typedef struct {
    eb_net_state_t state;
    char ssid[33];       // STA 模式是路由器名,AP 模式是本机热点名
    char url[32];        // http://<ip>
    uint32_t uploads;    // 书架内容变化的次数,界面据此重新列书
    uint64_t total;      // books 分区容量与剩余空间,传书页直接上屏
    uint64_t freespace;
    char     note[80];   // 最近一次传书的结果,已经是能直接显示的整句
    bool     note_ok;
} eb_net_status_t;

// 在 LVGL 建屏之前调用一次:NVS、netif、默认事件循环。
bool eb_net_prepare(void);

// 进入传书页时调用。非阻塞:连接过程通过 eb_net_status() 轮询。
bool eb_net_start(void);

// 离开传书页时调用。停 HTTP 服务并关掉射频。重复调用安全。
void eb_net_stop(void);

void eb_net_status(eb_net_status_t *out);
