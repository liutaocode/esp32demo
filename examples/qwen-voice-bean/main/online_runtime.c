#include "online_runtime.h"
#include "online_setup.h"
#include "bsp_audio.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCM_BYTES 640
#define FRAME_MAX 16384
/* Fixed queue budget ~30 KB, one bounded JSON frame <16 KB. */
typedef struct { unsigned epoch,rate; size_t len; bool done; char id[129]; uint8_t pcm[PCM_BYTES]; } playback_t;
typedef struct { size_t len; unsigned epoch; uint8_t pcm[PCM_BYTES]; } capture_t;
typedef struct { char type[32],id[129]; } receipt_t;
static QueueHandle_t playback,capture,receipts;
static portMUX_TYPE state_lock=portMUX_INITIALIZER_UNLOCKED;
static online_status_t status={.phase=ONLINE_CONNECTING,.volume=75};
static atomic_bool mic,ready,connected,got_ip,hello_pending,cancel_pending,setup_pending,volume_pending,fault;
static atomic_uint epoch,level,volume=75,capture_drops;
static atomic_bool suppress_audio;
static esp_websocket_client_handle_t gateway_socket;
static char *frame;
static size_t frame_used,frame_base,chunk_used,chunk_total;
static bool message_active;
static online_config_t config;
static uint32_t serial;
static bool audio_available;
static void state(online_phase_t p,const char *message) {
    portENTER_CRITICAL(&state_lock);status.phase=p;
    if(message) snprintf(status.message,sizeof(status.message),"%s",message);
    portEXIT_CRITICAL(&state_lock);
}
void online_snapshot(online_status_t *out) {
    portENTER_CRITICAL(&state_lock);*out=status;portEXIT_CRITICAL(&state_lock);
    out->mic=atomic_load(&mic);out->volume=atomic_load(&volume);out->level=atomic_load(&level);
}
void online_toggle_mic(void) { if(audio_available) {atomic_store(&mic,!atomic_load(&mic));atomic_fetch_add(&epoch,1);if(!atomic_load(&mic))online_cancel();} }
void online_cancel(void) { atomic_store(&suppress_audio,true); atomic_fetch_add(&epoch,1); atomic_store(&cancel_pending,true); }
void online_volume(void) { atomic_store(&volume_pending,true); }
void online_setup(void) { atomic_store(&setup_pending,true); }
static void fail(const char *why) {
    ESP_LOGE("bean_network","%s",why);
    atomic_store(&ready,false);atomic_store(&mic,false);atomic_store(&fault,true);
    atomic_fetch_add(&epoch,1);state(ONLINE_ERROR,why);
}
/* A transient transport failure must not change the user's microphone choice.
 * The websocket client owns reconnects after a disconnect event; only request
 * an explicit restart when a send fails while it still reports connected. */
static void recover_transport(void) {
    bool was_connected=atomic_exchange(&connected,false);
    atomic_store(&ready,false);
    atomic_fetch_add(&epoch,1);
    if(was_connected)atomic_store(&fault,true);
    ESP_LOGW("bean_network","recovering transport; mic=%d restart=%d",atomic_load(&mic),was_connected);
    state(ONLINE_CONNECTING,atomic_load(&got_ip)?"后端断开，正在重连":"Wi-Fi 断开，正在重连");
}
static void receipt(const char *type,const char *id) {
    receipt_t r={0};snprintf(r.type,sizeof(r.type),"%s",type);snprintf(r.id,sizeof(r.id),"%s",id);
    if(xQueueSend(receipts,&r,0)!=pdTRUE)fail("回复确认失败，请重新连接");
}
static const char *string(cJSON *j,const char *key) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:"";
}
static int number(cJSON *j,const char *key) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsNumber(v)?v->valueint:0;
}
static void receive_message(void) {
    const char *parse_end=NULL;
    cJSON *j=cJSON_ParseWithOpts(frame,&parse_end,true);
    if(!j){ESP_LOGE("bean_network","JSON parse failed bytes=%u offset=%d heap=%lu largest=%u",(unsigned)frame_used,parse_end?(int)(parse_end-frame):-1,(unsigned long)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));fail("后端消息格式错误");return;}
    const char *type=string(j,"type");
    if(!strcmp(type,"session.ready")) {
        cJSON *caps=cJSON_GetObjectItemCaseSensitive(j,"capabilities"),*cap;
        bool input=false,ack=false;
        cJSON_ArrayForEach(cap,caps)if(cJSON_IsString(cap)) {
            if(!strcmp(cap->valuestring,"input.audio"))input=true;
            if(!strcmp(cap->valuestring,"playback.receipts"))ack=true;
        }
        if(strcmp(string(j,"protocol_version"),"7.0.0") || !input || !ack)fail("后端协议版本不兼容");
    } else if(!strcmp(type,"voice.ready")) {
        if(number(j,"inputSampleRate")!=16000)fail("请将后端输入配置为 16000 Hz");
        else {atomic_store(&ready,true);state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,audio_available?(atomic_load(&mic)?"我在认真听":"确定：开始对话"):"音频不可用，请检查设备");}
    } else if(!strcmp(type,"voice.connection") && !strcmp(string(j,"state"),"unavailable")) {
        /* Gateway owns provider reconnection. Keep this client/session and the
         * user's mic setting when an idle provider connection is recycled. */
        atomic_store(&ready,false);atomic_fetch_add(&epoch,1);
        if(*string(j,"message")) {
            atomic_store(&mic,false);state(ONLINE_ERROR,"语音服务不可用，请检查后端");
        } else {
            ESP_LOGI("bean_network","voice provider recovering; retaining gateway session");
            state(atomic_load(&mic)?ONLINE_CONNECTING:ONLINE_READY,
                atomic_load(&mic)?"语音服务恢复中":"按确定开麦");
        }
    }
    else if(!strcmp(type,"voice.ownership") && !strcmp(string(j,"state"),"busy"))fail("后端正被其他客户端使用");
    else if(!strcmp(type,"voice.deactivated"))fail("对话已被其他客户端接管");
    else if(!strcmp(type,"voice.state") && atomic_load(&ready)) {
        const char *v=string(j,"state");
        if(!strcmp(v,"listening") && atomic_load(&mic))state(ONLINE_LISTENING,"我在认真听");
        if(!strcmp(v,"processing"))state(ONLINE_THINKING,"正在请教 Agent");
    } else if(!strcmp(type,"response.started")) {
        atomic_store(&suppress_audio,!atomic_load(&mic));
    } else if(!strcmp(type,"audio.delta")) {
        const char *id=string(j,"responseId"),*audio=string(j,"audio");int rate=number(j,"sampleRate");
        if(!atomic_load(&mic) || !atomic_load(&ready) || !audio_available || atomic_load(&suppress_audio)) {cJSON_Delete(j);return;}
        if((rate!=16000 && rate!=24000) || !*id || strlen(id)>128) {cJSON_Delete(j);fail("后端音频格式不支持");return;}
        /* Decode complete base64 quartets into even PCM chunks. The bounded
         * receive frame and JSON tree already hold the encoded audio; allocating
         * another whole decoded frame can exhaust this no-PSRAM board. */
        size_t encoded_len=strlen(audio);
        playback_t p={.rate=(unsigned)rate,.epoch=atomic_load(&epoch)};
        snprintf(p.id,sizeof(p.id),"%s",id);
        for(size_t off=0;off<encoded_len;) {
            size_t chunk=encoded_len-off;
            if(chunk>848)chunk=848; /* 848 base64 chars -> 636 PCM bytes. */
            if(mbedtls_base64_decode(p.pcm,sizeof(p.pcm),&p.len,
                    (const unsigned char *)audio+off,chunk) || (p.len&1)) {
                fail("后端音频解码失败");break;
            }
            if(p.len && xQueueSend(playback,&p,pdMS_TO_TICKS(100))!=pdTRUE) {
                fail("网络音频拥塞，请重试");break;
            }
            off+=chunk;
        }
    } else if(!strcmp(type,"audio.done")) {
        playback_t p={.done=true,.epoch=atomic_load(&epoch)};
        snprintf(p.id,sizeof(p.id),"%s",string(j,"responseId"));
        if(xQueueSend(playback,&p,pdMS_TO_TICKS(100))!=pdTRUE)fail("网络音频拥塞，请重试");
    } else if(!strcmp(type,"playback.clear") || !strcmp(type,"response.interrupted")) {
        atomic_fetch_add(&epoch,1);
    } else if(!strcmp(type,"error")) {
        /* Never display arbitrary server messages: they can include private data. */
        fail("后端错误，请检查服务配置");
    } else if(!strcmp(type,"task.permission.requested")) {
        state(ONLINE_READY,"Agent 等待授权，请用语音确认");
    } else if(!strcmp(type,"task.input.requested"))state(ONLINE_READY,"Agent 等待补充，请用语音回答");
    cJSON_Delete(j);
}
static void websocket_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;(void)base;
    if(id==WEBSOCKET_EVENT_CONNECTED) {
        atomic_store(&connected,true);atomic_store(&hello_pending,true);frame_used=0;
        state(ONLINE_CONNECTING,"正在启动语音服务");
    } else if(id==WEBSOCKET_EVENT_DISCONNECTED || id==WEBSOCKET_EVENT_ERROR || id==WEBSOCKET_EVENT_CLOSED) {
        atomic_store(&connected,false);atomic_store(&ready,false);atomic_store(&hello_pending,false);atomic_fetch_add(&epoch,1);
        frame_used=0;message_active=false;
        ESP_LOGW("bean_network","websocket event=%ld",(long)id);
        state(ONLINE_CONNECTING,atomic_load(&got_ip)?"后端断开，正在重连":"Wi-Fi 断开，正在重连");
    } else if(id==WEBSOCKET_EVENT_DATA) {
        esp_websocket_event_data_t *d=data;
        if(d->op_code==8 && d->data_len>=2) {
            const unsigned char *close_data=(const unsigned char *)d->data_ptr;
            ESP_LOGW("bean_network","server close code=%u",((unsigned)close_data[0]<<8)|close_data[1]);
        }
        if(d->op_code!=1 && d->op_code!=0)return;
        if(!frame)frame=malloc(FRAME_MAX);
        if(!frame)ESP_LOGE("bean_network","receive buffer allocation failed");
        if(!frame || d->payload_offset<0 || d->data_len<0 || d->payload_len<0) {
            fail("后端消息过大或分片无效");return;
        }
        if(d->payload_offset==0) {
            if(d->op_code==1) {
                if(message_active){fail("后端消息分片顺序错误");return;}
                frame_used=0;message_active=true;
            } else if(!message_active || chunk_used!=chunk_total) {
                fail("后端消息分片顺序错误");return;
            }
            frame_base=frame_used;chunk_used=0;chunk_total=d->payload_len;
        }
        if(!message_active || chunk_total!=(size_t)d->payload_len ||
           !online_fragment(&chunk_used,FRAME_MAX-frame_base,d->payload_offset,d->data_len,d->payload_len)) {
            fail("后端消息过大或分片无效");return;
        }
        memcpy(frame+frame_base+d->payload_offset,d->data_ptr,d->data_len);
        frame_used=frame_base+chunk_used;
        if(chunk_used==chunk_total && d->fin) {
            frame[frame_used]=0;receive_message();frame_used=0;message_active=false;
        }
    }
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP){atomic_store(&got_ip,true);state(ONLINE_CONNECTING,"正在连接后端");}
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event=data;
        ESP_LOGW("bean_network","wifi disconnected reason=%u",event?event->reason:0);
        atomic_store(&got_ip,false);atomic_store(&ready,false);atomic_fetch_add(&epoch,1);
        state(ONLINE_CONNECTING,"Wi-Fi 断开，正在重试");
    }
}
static bool send_json(cJSON *j) {
    if(!j)return false;
    char event_id[48];snprintf(event_id,sizeof(event_id),"bean_%08lx_%lu",(unsigned long)esp_random(),(unsigned long)++serial);
    cJSON_AddStringToObject(j,"event_id",event_id);char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!raw)return false;
    if(!atomic_load(&connected) || !esp_websocket_client_is_connected(gateway_socket)){free(raw);return false;}
    int length=strlen(raw);int sent=esp_websocket_client_send_text(gateway_socket,raw,length,pdMS_TO_TICKS(3000));free(raw);
    if(sent!=length)ESP_LOGW("bean_network","send failed bytes=%d sent=%d connected=%d heap=%lu",length,sent,atomic_load(&connected),(unsigned long)esp_get_free_heap_size());
    return sent==length;
}
static cJSON *event(const char *type) {cJSON *j=cJSON_CreateObject();if(j)cJSON_AddStringToObject(j,"type",type);return j;}
static void send_hello(void) {
    cJSON *j=event("session.hello"),*protocol=cJSON_AddObjectToObject(j,"protocol");
    cJSON_AddStringToObject(protocol,"min","7.0.0");cJSON_AddStringToObject(protocol,"max","7.0.0");
    cJSON *client=cJSON_AddObjectToObject(j,"client");uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);
    char instance[40];snprintf(instance,sizeof(instance),"bean_%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    cJSON_AddStringToObject(client,"type","mouthy-bean");cJSON_AddStringToObject(client,"version","0.3.0");cJSON_AddStringToObject(client,"instance_id",instance);
    cJSON *caps=cJSON_AddArrayToObject(j,"capabilities");
    cJSON_AddItemToArray(caps,cJSON_CreateString("input.audio"));
    cJSON_AddItemToArray(caps,cJSON_CreateString("playback.receipts"));
    cJSON_AddStringToObject(j,"locale","zh-CN");cJSON_AddStringToObject(j,"time_zone","Asia/Shanghai");
    cJSON *connection=cJSON_AddObjectToObject(j,"connection");
    cJSON_AddBoolToObject(connection,"voice_enabled",true);cJSON_AddBoolToObject(connection,"input_enabled",atomic_load(&mic));
    cJSON_AddBoolToObject(connection,"output_enabled",true);cJSON_AddBoolToObject(connection,"text_only",false);
    if(!send_json(j))fail("握手失败，请检查后端");
}
static void audio_task(void *arg) {
    (void)arg;
    /* One lifetime worker owns these buffers. Keep codec call-stack headroom. */
    static playback_t p;
    static capture_t c={.len=PCM_BYTES};
    static char active[129];
    static uint8_t silence[PCM_BYTES];
    int applied_volume=-1;
    int64_t last_metrics=0,starved_since=0,prefill_since=0;
    unsigned playback_starves=0,max_starve_ms=0,max_write_ms=0;
    unsigned active_epoch=0,format=0;int64_t quiet_until=0;
    for(;;) {
        if(esp_timer_get_time()-last_metrics>10000000) {
            last_metrics=esp_timer_get_time();
            ESP_LOGI("bean_audio","stack_free_min=%u heap_free=%lu ready=%d capture_drops=%u largest=%u",
                (unsigned)uxTaskGetStackHighWaterMark(NULL),(unsigned long)esp_get_free_heap_size(),atomic_load(&ready),atomic_load(&capture_drops),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        }
        if(active[0] && (active_epoch!=atomic_load(&epoch) || !atomic_load(&ready))) {
            receipt("playback.cancelled",active);active[0]=0;quiet_until=esp_timer_get_time()+600000;applied_volume=-1;
        }
        /* Reuse the existing queue for about 300 ms of starting audio. This
         * absorbs provider/network jitter without allocating another buffer. */
        if(!active[0] && atomic_load(&ready)) {
            static playback_t first;
            if(xQueuePeek(playback,&first,0)==pdTRUE && first.epoch==atomic_load(&epoch) && !first.done) {
                if(!prefill_since)prefill_since=esp_timer_get_time();
                if(online_playback_prefill_wait((unsigned)uxQueueMessagesWaiting(playback),
                        (unsigned)((esp_timer_get_time()-prefill_since)/1000))) {
                    vTaskDelay(pdMS_TO_TICKS(5));continue;
                }
            } else prefill_since=0;
        }
        if(xQueueReceive(playback,&p,pdMS_TO_TICKS(5))==pdTRUE) {
            prefill_since=0;
            if(p.epoch!=atomic_load(&epoch) || !atomic_load(&ready))continue;
            if(starved_since) {
                unsigned waited=(unsigned)((esp_timer_get_time()-starved_since)/1000);
                if(waited>=60 && !p.done){playback_starves++;if(waited>max_starve_ms)max_starve_ms=waited;ESP_LOGW("bean_audio","playback queue empty ms=%u buffered=%u",waited,(unsigned)uxQueueMessagesWaiting(playback));}
                starved_since=0;
            }
            if(p.done) {
                ESP_LOGI("bean_audio","playback stats gaps=%u max_gap_ms=%u max_write_ms=%u",playback_starves,max_starve_ms,max_write_ms);
                playback_starves=0;max_starve_ms=0;max_write_ms=0;
                if(active[0] && !strcmp(active,p.id)) {
                    for(unsigned i=0;i<6;i++) {
                        if(active_epoch!=atomic_load(&epoch) || bsp_audio_write(silence,sizeof(silence))!=ESP_OK)break;
                    }
                    receipt(active_epoch==atomic_load(&epoch)?"playback.ended":"playback.cancelled",active);active[0]=0;quiet_until=esp_timer_get_time()+600000;
                    state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,atomic_load(&mic)?"你说，我在听":"麦克风已关闭");
                }
                continue;
            }
            if(format!=p.rate) {
                if(bsp_audio_set_format(p.rate,16,1)!=ESP_OK){fail("扬声器不可用");continue;}format=p.rate;applied_volume=-1;
            }
            if(strcmp(active,p.id)) {
                if(active[0])receipt("playback.cancelled",active);
                snprintf(active,sizeof(active),"%s",p.id);active_epoch=p.epoch;
                receipt("playback.started",active);state(ONLINE_SPEAKING,"Qwen Audio Agent");
            }
            unsigned requested_volume=atomic_load(&volume);
            if(applied_volume!=(int)requested_volume) {
                bsp_audio_set_volume(requested_volume);applied_volume=(int)requested_volume;

            }
            unsigned peak=0;
            for(size_t i=0;i<p.len;i+=2){int16_t s=(int16_t)((unsigned)p.pcm[i]|((unsigned)p.pcm[i+1]<<8));unsigned a=s<0?-(int)s:s;if(a>peak)peak=a;}
            atomic_store(&level,peak);
            int64_t write_start=esp_timer_get_time();
            if(bsp_audio_write(p.pcm,p.len)!=ESP_OK)fail("扬声器播放失败");
            unsigned write_ms=(unsigned)((esp_timer_get_time()-write_start)/1000);if(write_ms>max_write_ms)max_write_ms=write_ms;
            if(write_ms>=80)ESP_LOGW("bean_audio","slow audio write ms=%u buffered=%u",write_ms,(unsigned)uxQueueMessagesWaiting(playback));
            continue;
        }
        if(active[0]){if(!starved_since)starved_since=esp_timer_get_time();continue;}
        starved_since=0;
        atomic_store(&level,0);
        if(!audio_available || !atomic_load(&ready) || !atomic_load(&mic)){vTaskDelay(pdMS_TO_TICKS(20));continue;}
        if(format!=16000){if(bsp_audio_set_format(16000,16,1)!=ESP_OK){fail("麦克风不可用");continue;}format=16000;}
        if(bsp_audio_read(c.pcm,PCM_BYTES)!=ESP_OK){fail("麦克风读取失败");continue;}
        if(atomic_load(&mic) && esp_timer_get_time()>quiet_until) {
            c.epoch=atomic_load(&epoch);
            if(xQueueSend(capture,&c,0)!=pdTRUE) {
                /* Prefer current speech to stale queued audio after a network stall.
                 * A transient full queue must not tear down the voice session. */
                static capture_t discarded;
                if(xQueueReceive(capture,&discarded,0)==pdTRUE)atomic_fetch_add(&capture_drops,1);
                if(xQueueSend(capture,&c,0)!=pdTRUE)atomic_fetch_add(&capture_drops,1);
            }
        }
    }
}
static void network_task(void *arg) {
    (void)arg;
    if(nvs_flash_init()!=ESP_OK || esp_netif_init()!=ESP_OK || esp_event_loop_create_default()!=ESP_OK) {
        state(ONLINE_ERROR,"系统初始化失败");vTaskDelete(NULL);return;
    }
    if(!online_load_config(&config)) {
        state(ONLINE_CONNECTING,"正在扫描附近 Wi-Fi");
        char info[128];esp_err_t e=online_start_setup(info,sizeof(info));state(e==ESP_OK?ONLINE_SETUP:ONLINE_ERROR,e==ESP_OK?info:"配网启动失败");
        vTaskDelete(NULL);return;
    }
    /* Reserve the largest application allocation before Wi-Fi/TLS fragments
     * the heap. TLS itself uses fixed lifetime buffers for this application. */
    frame=malloc(FRAME_MAX);
    if(!frame){state(ONLINE_ERROR,"内存不足，无法启动");vTaskDelete(NULL);return;}
    state(ONLINE_CONNECTING,"正在连接 Wi-Fi");
    portENTER_CRITICAL(&state_lock);
    status.configured=true;status.password_set=config.password[0]!=0;status.token_set=config.token[0]!=0;
    snprintf(status.ssid,sizeof(status.ssid),"%s",config.ssid);
    const char *host=strstr(config.url,"://");host=host?host+3:config.url;
    size_t host_len=strcspn(host,":/");if(host_len>=sizeof(status.host))host_len=sizeof(status.host)-1;
    memcpy(status.host,host,host_len);status.host[host_len]=0;
    portEXIT_CRITICAL(&state_lock);
    esp_netif_create_default_wifi_sta();wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi={0};memcpy(wifi.sta.ssid,config.ssid,strlen(config.ssid));memcpy(wifi.sta.password,config.password,strlen(config.password));
    if(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL)!=ESP_OK ||
       esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL)!=ESP_OK || esp_wifi_init(&init)!=ESP_OK ||
       esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK ||
       esp_wifi_set_config(WIFI_IF_STA,&wifi)!=ESP_OK || esp_wifi_start()!=ESP_OK) {
        state(ONLINE_ERROR,"Wi-Fi 初始化失败");vTaskDelete(NULL);return;
    }
    /* Voice streaming needs prompt uplink delivery after opening the microphone. */
    if(esp_wifi_set_ps(WIFI_PS_NONE)!=ESP_OK)ESP_LOGW("bean_network","Wi-Fi power-save disable failed");
    memset(wifi.sta.password,0,sizeof(wifi.sta.password));memset(config.password,0,sizeof(config.password));
    /* Start only after DHCP, so offline time does not exhaust SNTP retries. */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    static const char *const time_sources[]={"ntp.aliyun.com","ntp1.aliyun.com","pool.ntp.org"};
    char headers[240]={0};
    if(config.token[0])snprintf(headers,sizeof(headers),"Authorization: Bearer %s\r\n",config.token);
    esp_websocket_client_config_t ws={.uri=config.url,.headers=config.token[0]?headers:NULL,.buffer_size=2048,.task_stack=6144,
        .keep_alive_enable=true,.keep_alive_idle=10,.keep_alive_interval=5,.keep_alive_count=3,
        .ping_interval_sec=10,.pingpong_timeout_sec=120,.network_timeout_ms=8000,.reconnect_timeout_ms=5000,.enable_close_reconnect=true,.crt_bundle_attach=esp_crt_bundle_attach};
    gateway_socket=esp_websocket_client_init(&ws);memset(headers,0,sizeof(headers));memset(config.token,0,sizeof(config.token));
    if(!gateway_socket || esp_websocket_register_events(gateway_socket,WEBSOCKET_EVENT_ANY,websocket_event,NULL)!=ESP_OK) {
        state(ONLINE_ERROR,"后端连接初始化失败");vTaskDelete(NULL);return;
    }
    online_time_wait_t time_wait={0};
    bool started=false,previous_mic=false;int64_t last_connect=0;capture_t c;receipt_t r;
    for(;;) {
        if(atomic_exchange(&setup_pending,false)) {
            nvs_handle_t n;esp_err_t e=nvs_open("bean_online",NVS_READWRITE,&n);
            if(e==ESP_OK){e=nvs_set_u8(n,"setup",1);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
            if(e==ESP_OK)esp_restart();else state(ONLINE_ERROR,"无法进入配置，请重试");
        }
        if(atomic_exchange(&volume_pending,false))atomic_store(&volume,atomic_load(&volume)>=90?0:atomic_load(&volume)==0?55:atomic_load(&volume)==55?75:90);
        bool has_ip=atomic_load(&got_ip);
        if(!started) {
            bool was_waiting=time_wait.active;
            online_time_action_t action=online_time_step(&time_wait,has_ip,
                !strncmp(config.url,"wss://",6) && time(NULL)<1704067200,
                (uint64_t)(esp_timer_get_time()/1000));
            if(was_waiting && !has_ip)esp_sntp_stop();
            if(action==ONLINE_TIME_START || action==ONLINE_TIME_RETRY) {
                esp_sntp_stop();
                esp_sntp_setservername(0,time_sources[time_wait.source_index]);
                esp_sntp_init();
                ESP_LOGI("bean_network","time sync %s source=%u",action==ONLINE_TIME_START?"started":"retry",time_wait.source_index);
            }
            if(was_waiting && !time_wait.active && has_ip)ESP_LOGI("bean_network","time sync complete");
        }
        if(!has_ip) {
            if(esp_timer_get_time()-last_connect>5000000 || !last_connect){esp_wifi_connect();last_connect=esp_timer_get_time();}
            vTaskDelay(pdMS_TO_TICKS(50));continue;
        }
        if(time_wait.active) {
            state(ONLINE_CONNECTING,time_wait.timed_out?"校时超时，正在重试":"正在同步时间");
            vTaskDelay(pdMS_TO_TICKS(100));continue;
        }
        if(!started){state(ONLINE_CONNECTING,"正在连接后端");esp_websocket_client_start(gateway_socket);started=true;}
        if(!atomic_load(&fault) && atomic_load(&connected) && atomic_exchange(&hello_pending,false)) {send_hello();previous_mic=!atomic_load(&mic);}
        if(atomic_exchange(&fault,false)) {
            esp_websocket_client_stop(gateway_socket);atomic_store(&connected,false);vTaskDelay(pdMS_TO_TICKS(1000));
            esp_websocket_client_start(gateway_socket);continue;
        }
        if(atomic_exchange(&cancel_pending,false)) {
            atomic_fetch_add(&epoch,1);
            if(atomic_load(&connected))send_json(event("response.cancel"));
            if(atomic_load(&ready))state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,atomic_load(&mic)?"你说，我在听":"麦克风已关闭");
        }
        if(atomic_load(&connected) && previous_mic!=atomic_load(&mic)) {
            previous_mic=atomic_load(&mic);
            /* Clear upstream buffered input on pause; wake the retained session
             * before resuming. These controls must precede fresh audio frames. */
            if(previous_mic && !send_json(event("wake"))) {recover_transport();continue;}
            if(!send_json(event(previous_mic?"input.unmute":"input.mute"))) {recover_transport();continue;}
            ESP_LOGI("bean_network","microphone %s",previous_mic?"resumed":"paused");
            if(atomic_load(&ready))state(previous_mic?ONLINE_LISTENING:ONLINE_READY,previous_mic?"你说，我在听":"麦克风已关闭");
            else if(previous_mic && atomic_load(&connected))state(ONLINE_CONNECTING,"语音服务恢复中");
        }
        while(xQueueReceive(receipts,&r,0)==pdTRUE)if(atomic_load(&connected)) {
            cJSON *j=event(r.type);cJSON_AddStringToObject(j,"responseId",r.id);if(!send_json(j)){recover_transport();break;}
        }
        if(xQueueReceive(capture,&c,pdMS_TO_TICKS(10))==pdTRUE && atomic_load(&connected) && atomic_load(&ready) && atomic_load(&mic) && c.epoch==atomic_load(&epoch)) {
            unsigned char encoded[PCM_BYTES*4/3+8];size_t len=0;
            if(!mbedtls_base64_encode(encoded,sizeof(encoded),&len,c.pcm,c.len)) {
                encoded[len]=0;cJSON *j=event("input_audio_buffer.append");cJSON_AddStringToObject(j,"audio",(char *)encoded);
                if(!send_json(j))recover_transport();
            }
        }
    }
}
void online_start(bool audio_ok) {
    audio_available=audio_ok;
    playback=xQueueCreate(32,sizeof(playback_t));capture=xQueueCreate(8,sizeof(capture_t));receipts=xQueueCreate(8,sizeof(receipt_t));
    if(!playback || !capture || !receipts){state(ONLINE_ERROR,"内存不足，无法启动");return;}
    if(xTaskCreate(network_task,"bean_network",8192,NULL,4,NULL)!=pdPASS ||
       xTaskCreate(audio_task,"bean_audio",8192,NULL,5,NULL)!=pdPASS)state(ONLINE_ERROR,"任务启动失败");
}
