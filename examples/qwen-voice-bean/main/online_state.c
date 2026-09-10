#include "online_state.h"
#include <string.h>
#include <stdio.h>
static bool bounded(const char *s,size_t n) { return memchr(s,0,n)!=NULL; }
bool online_config_valid(const online_config_t *c) {
    if(!c || c->version!=1 || !bounded(c->ssid,sizeof(c->ssid)) ||
       !bounded(c->password,sizeof(c->password)) || !bounded(c->url,sizeof(c->url)) ||
       !bounded(c->token,sizeof(c->token)) || !c->ssid[0]) return false;
    size_t pass=strlen(c->password);
    if(pass && (pass<8 || pass>63)) return false;
    const char *host=NULL;
    if(!strncmp(c->url,"ws://",5)) host=c->url+5;
    if(!strncmp(c->url,"wss://",6)) host=c->url+6;
    if(!host || !*host || *host=='/' || (c->token[0] && strlen(c->token)<24)) return false;
    if(strpbrk(c->url,"@?#\r\n\t ") || strpbrk(c->token,"\r\n\t ")) return false;
    const char *path=strchr(host,'/');
    if(!path || strcmp(path,"/api/realtime")) return false;
    return true;
}
bool online_fragment(size_t *used,size_t cap,size_t off,size_t len,size_t total) {
    if(!off) *used=0;
    if(total>=cap || off!=*used || off>total || len>total-off) { *used=0;return false; }
    *used+=len; return true;
}
const char *online_phase_text(online_phase_t p) {
    static const char *const names[]={"配置后端","正在连接","准备好了","你说，我在听","思考一下","正在回答","连接失败"};
    return p<=ONLINE_ERROR?names[p]:names[ONLINE_ERROR];
}

bool online_endpoint_from_ip(const char *ip,char *out,size_t size) {
    if(!ip || !*ip || strlen(ip)>15 || !out || !size)return false;
    unsigned parts=0,value=0,digits=0,octets[4]={0};
    for(const char *p=ip;;p++) {
        if(*p>='0' && *p<='9') {
            if(digits==1 && value==0)return false;
            if(++digits>3)return false;
            value=value*10+(unsigned)(*p-'0');if(value>255)return false;
        } else if(*p=='.' || !*p) {
            if(!digits || ++parts>4)return false;
            octets[parts-1]=value;
            if(!*p)break;
            value=digits=0;
        } else return false;
    }
    if(parts!=4)return false;
    bool lan=octets[0]==10 || (octets[0]==172 && octets[1]>=16 && octets[1]<=31) ||
        (octets[0]==192 && octets[1]==168);
    int n=snprintf(out,size,"%s://%s:3101/api/realtime",lan?"ws":"wss",ip);
    return n>0 && (size_t)n<size;
}
void online_setup_password(uint32_t random_value,char out[9]) {
    snprintf(out,9,"%08lu",(unsigned long)(random_value%100000000u));
}

online_action_t online_menu_key(online_menu_t *m,online_key_t key) {
    if(key==ONLINE_KEY_LONG) {
        m->page=m->page==ONLINE_HOME?ONLINE_SETTINGS:ONLINE_HOME;m->selected=0;
        return ONLINE_ACTION_NONE;
    }
    if(m->page==ONLINE_HOME) {
        if(key==ONLINE_KEY_OK)return ONLINE_ACTION_MIC;
        if(key==ONLINE_KEY_UP)return ONLINE_ACTION_VOLUME;
        if(key==ONLINE_KEY_DOWN)return ONLINE_ACTION_CANCEL;
    } else if(key==ONLINE_KEY_UP || key==ONLINE_KEY_DOWN)m->selected^=1;
    else if(key==ONLINE_KEY_OK) {
        if(m->page==ONLINE_SETTINGS) {
            m->page=m->selected?ONLINE_CONFIRM:ONLINE_HOME;m->selected=0;
        } else if(m->selected) {
            m->page=ONLINE_HOME;m->selected=0;return ONLINE_ACTION_SETUP;
        } else {m->page=ONLINE_SETTINGS;m->selected=1;}
    }
    return ONLINE_ACTION_NONE;
}

bool online_playback_prefill_wait(unsigned queued,unsigned elapsed_ms) {
    return queued>0 && queued<24 && elapsed_ms<350;
}

online_time_action_t online_time_step(online_time_wait_t *w, bool has_ip,
                                     bool needs_time, uint64_t now_ms) {
    if (!has_ip || !needs_time) { memset(w,0,sizeof(*w)); return ONLINE_TIME_IDLE; }
    if (!w->active) {
        w->active=true; w->started_ms=w->retry_ms=now_ms;
        return ONLINE_TIME_START;
    }
    w->timed_out=now_ms-w->started_ms>=10000;
    if (now_ms-w->retry_ms>=10000) {
        w->retry_ms=now_ms;
        w->source_index=(w->source_index+1)%3;
        return ONLINE_TIME_RETRY;
    }
    return ONLINE_TIME_WAIT;
}
