#include "online_setup.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static char nonce[33];
static bool saved;
static esp_timer_handle_t reboot_timer;
bool online_load_config(online_config_t *c) {
    nvs_handle_t n; size_t size=sizeof(*c); memset(c,0,size);
    if(nvs_open("bean_online",NVS_READWRITE,&n)!=ESP_OK) return false;
    uint8_t setup=0; nvs_get_u8(n,"setup",&setup);
    if(setup) { nvs_erase_key(n,"setup"); nvs_commit(n); }
    esp_err_t e=nvs_get_blob(n,"config",c,&size); nvs_close(n);
    return !setup && e==ESP_OK && size==sizeof(*c) && online_config_valid(c);
}
static void reboot(void *arg) { (void)arg;esp_restart(); }
#define NETWORK_LIMIT 32
static wifi_ap_record_t nearby[NETWORK_LIMIT];
static uint16_t nearby_count;
static bool scan_ok;
static esp_err_t networks(httpd_req_t *r) {
    cJSON *j=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(j,"networks");
    cJSON_AddBoolToObject(j,"scanned",scan_ok);
    for(unsigned i=0;i<nearby_count;i++) {
        const char *ssid=(const char *)nearby[i].ssid;if(!*ssid)continue;
        bool duplicate=false;
        for(unsigned k=0;k<i;k++)if(!strcmp((const char *)nearby[k].ssid,ssid))duplicate=true;
        if(duplicate)continue;
        cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"ssid",ssid);
        cJSON_AddNumberToObject(item,"rssi",nearby[i].rssi);
        cJSON_AddBoolToObject(item,"secure",nearby[i].authmode!=WIFI_AUTH_OPEN);cJSON_AddItemToArray(list,item);
    }
    char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!raw)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"读取列表失败");
    httpd_resp_set_type(r,"application/json; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t e=httpd_resp_sendstr(r,raw);free(raw);return e;
}
static esp_err_t home(httpd_req_t *r) {
    char *html=malloc(6144);if(!html)return ESP_ERR_NO_MEM;
    int length=snprintf(html,6144,
        "<!doctype html><html lang=zh-CN><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Qwen 语音豆</title><style>body{font:17px system-ui;max-width:520px;margin:24px auto;padding:20px;background:#fff6d6;color:#553b23}input,select,button{box-sizing:border-box;width:100%%;padding:12px;margin:8px 0 20px;font:inherit}button{background:#ffd43b;border:0;border-radius:12px}small{display:block;color:#76624a}input[type=checkbox]{width:auto;margin-right:8px}</style>"
        "<h1>Qwen 语音豆</h1><p>基于 Qwen Audio Agent 的语音伙伴</p><h2>1. 先搭建后端</h2><p>请先在电脑上按仓库说明安装并启动 Qwen Audio Agent，配置语音服务及后台 Agent。</p><p><a href=https://github.com/QwenAudio/qwen-audio-agent target=_blank rel=noopener style=overflow-wrap:anywhere>https://github.com/QwenAudio/qwen-audio-agent</a></p><small>热点不能上网？请先用电脑或手机移动网络打开仓库。完成部署后，再回到这里配网。设备入口需使用随附后端工具。</small><h2>2. 配置 Wi-Fi 与后端</h2>"
        "<form id=f><label>选择附近的 2.4 GHz Wi-Fi<select id=wifi required><option value=''>正在读取附近网络…</option></select></label>"
        "<label id=manualRow hidden>Wi-Fi 名称<input id=manual maxlength=32 autocomplete=off disabled></label>"
        "<small id=scanHint>设备已在开启热点前扫描附近网络。</small>"
        "<label>Wi-Fi 密码<input name=password type=password maxlength=63 autocomplete=new-password placeholder='输入所选 Wi-Fi 的密码'></label>"
        "<label>后端电脑的 IP 地址<input name=ip inputmode=decimal placeholder='例如 192.0.2.10' maxlength=15 required></label>"
        "<small>自动使用端口 3101，连接地址无需手动拼写。电脑需要保持开机。</small>"
        "<label><input id=auth type=checkbox>后端需要访问令牌</label>"
        "<label id=tokenRow hidden>访问令牌<input name=token type=password minlength=24 maxlength=192 autocomplete=new-password disabled></label>"
        "<small>免令牌需启用随附后端的局域网入口；请只在可信局域网使用。模型 API Key 仅在电脑后端配置。</small>"
        "<p>开启麦克风后，语音会发送至配置的后端及其语音服务。历史记录由后端设置决定。</p>"
        "<button>保存并开始连接</button></form><p id=s></p><script>"
        "const f=document.getElementById('f'),wifi=document.getElementById('wifi'),manual=document.getElementById('manual'),auth=document.getElementById('auth'),token=f.elements.token,s=document.getElementById('s');let networks=[];"
        "wifi.onchange=()=>{const yes=wifi.value==='manual';document.getElementById('manualRow').hidden=!yes;manual.disabled=!yes;manual.required=yes;};"
        "auth.onchange=()=>{document.getElementById('tokenRow').hidden=!auth.checked;token.disabled=!auth.checked;token.required=auth.checked;};"
        "fetch('/networks').then(r=>r.json()).then(data=>{networks=data.networks||[];wifi.replaceChildren(new Option('请选择你的 Wi-Fi',''));networks.forEach((n,i)=>wifi.add(new Option(n.ssid+' · '+(n.rssi>=-60?'信号强':n.rssi>=-75?'信号中':'信号弱'),String(i))));wifi.add(new Option('手动输入 / 隐藏网络','manual'));if(!networks.length){wifi.value='manual';wifi.onchange();document.getElementById('scanHint').textContent='暂未发现网络，可手动输入，或重启后重新扫描。';}}).catch(()=>{wifi.replaceChildren(new Option('手动输入 Wi-Fi','manual'));wifi.onchange();});"
        "f.onsubmit=async e=>{e.preventDefault();const b=f.querySelector('button');b.disabled=true;const data=Object.fromEntries(new FormData(f));data.ssid=wifi.value==='manual'?manual.value:networks[Number(wifi.value)].ssid;data.token=auth.checked?token.value:'';data.ip=data.ip.trim();try{const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Nonce':'%s'},body:JSON.stringify(data)});s.textContent=await r.text();if(!r.ok)b.disabled=false}catch(e){s.textContent='连接中断，请查看设备；如未重启请重新连接热点。';b.disabled=false}};</script></html>",nonce);
    if(length<0 || length>=6144){free(html);return ESP_FAIL;}
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Frame-Options","DENY");
    esp_err_t e=httpd_resp_send(r,html,length);free(html);return e;
}
static bool field(cJSON *j,const char *key,char *out,size_t cap) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    if(!cJSON_IsString(v) || strlen(v->valuestring)>=cap) return false;
    strcpy(out,v->valuestring); return true;
}
static esp_err_t save(httpd_req_t *r) {
    char provided[40];
    if(httpd_req_get_hdr_value_str(r,"X-Setup-Nonce",provided,sizeof(provided))!=ESP_OK || strcmp(provided,nonce))
        return httpd_resp_send_err(r,HTTPD_403_FORBIDDEN,"请重新打开配置页");
    if(saved || r->content_len<=0 || r->content_len>1200) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"配置无效");
    char body[1201];size_t used=0;
    while(used<(size_t)r->content_len) {
        int n=httpd_req_recv(r,body+used,r->content_len-used);
        if(n<=0) return ESP_FAIL;
        used+=n;
    }
    body[used]=0;cJSON *j=cJSON_Parse(body);online_config_t c={.version=1};
    char ip[16]={0};
    bool valid=j && field(j,"ssid",c.ssid,sizeof(c.ssid)) && field(j,"password",c.password,sizeof(c.password)) &&
        field(j,"ip",ip,sizeof(ip)) && online_endpoint_from_ip(ip,c.url,sizeof(c.url)) &&
        field(j,"token",c.token,sizeof(c.token)) && online_config_valid(&c);
    cJSON_Delete(j);memset(body,0,sizeof(body));
    if(!valid) {memset(&c,0,sizeof(c));return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"检查 Wi-Fi、密码、IP 和可选令牌");}
    nvs_handle_t n;esp_err_t e=nvs_open("bean_online",NVS_READWRITE,&n);
    if(e==ESP_OK) {e=nvs_set_blob(n,"config",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
    memset(&c,0,sizeof(c));
    if(e!=ESP_OK) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"保存失败，请重试");
    saved=true;httpd_resp_set_type(r,"text/plain; charset=utf-8");
    httpd_resp_sendstr(r,"已保存，设备即将重启。请让手机重新连接原来的 Wi-Fi。");
    esp_timer_start_once(reboot_timer,1500000);return ESP_OK;
}
esp_err_t online_start_setup(char *description,size_t size) {
    wifi_config_t ap={0}; char password[9];
    online_setup_password(esp_random(),password);
    snprintf(nonce,sizeof(nonce),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
    snprintf((char *)ap.ap.ssid,sizeof(ap.ap.ssid),"Qwen-Bean-%04lx",(unsigned long)(esp_random()&65535));
    strcpy((char *)ap.ap.password,password);ap.ap.ssid_len=strlen((char *)ap.ap.ssid);
    ap.ap.authmode=WIFI_AUTH_WPA2_PSK;ap.ap.max_connection=1;ap.ap.channel=1;
    if(!esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap()) return ESP_FAIL;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t e=esp_wifi_init(&cfg); if(e!=ESP_OK)return e;
    if((e=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK || (e=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK ||
       (e=esp_wifi_start())!=ESP_OK)return e;
    /* Blocking scan belongs to this boot worker and happens before AP startup. */
    scan_ok=esp_wifi_scan_start(NULL,true)==ESP_OK;
    if(scan_ok) {
        nearby_count=NETWORK_LIMIT;
        if(esp_wifi_scan_get_ap_records(&nearby_count,nearby)!=ESP_OK){nearby_count=0;scan_ok=false;}
    } else esp_wifi_clear_ap_list();
    ESP_LOGI("bean_setup","pre-hotspot scan: %u networks, success=%d",nearby_count,scan_ok);
    if((e=esp_wifi_stop())!=ESP_OK || (e=esp_wifi_set_mode(WIFI_MODE_AP))!=ESP_OK ||
       (e=esp_wifi_set_config(WIFI_IF_AP,&ap))!=ESP_OK || (e=esp_wifi_start())!=ESP_OK)return e;
    esp_timer_create_args_t ta={.callback=reboot,.name="setup_reboot"};
    if((e=esp_timer_create(&ta,&reboot_timer))!=ESP_OK)return e;
    httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.stack_size=6144;hc.max_open_sockets=2;hc.lru_purge_enable=true;
    httpd_handle_t server=NULL;if((e=httpd_start(&server,&hc))!=ESP_OK)return e;
    httpd_uri_t get={.uri="/",.method=HTTP_GET,.handler=home};
    httpd_uri_t post={.uri="/save",.method=HTTP_POST,.handler=save};
    httpd_uri_t list={.uri="/networks",.method=HTTP_GET,.handler=networks};
    if((e=httpd_register_uri_handler(server,&get))!=ESP_OK || (e=httpd_register_uri_handler(server,&post))!=ESP_OK || (e=httpd_register_uri_handler(server,&list))!=ESP_OK)return e;
    snprintf(description,size,"%s\n密码 %s",ap.ap.ssid,password);
    return ESP_OK;
}
