/* One caller: cast_sync worker. Only view/command flags cross task boundaries. */
#include "cast_remote.h"
#include "sdkconfig.h"
#include "cast_network.h"
#include "cast_protocol.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
typedef struct {uint32_t version;char origin[161],id[33],secret[65],ack[33];uint8_t result;} identity_t;
static identity_t identity;
static SemaphoreHandle_t lock;
static cast_remote_view_t view;
static bool request_pair;
static int64_t next_poll,pair_until;
static char last_origin[161];
static int response_status;
static void show(cast_remote_state_t state) {xSemaphoreTake(lock,portMAX_DELAY);view.state=state;xSemaphoreGive(lock);}
bool cast_remote_init(void) {if(!lock)lock=xSemaphoreCreateMutex();return lock!=NULL;}
bool cast_remote_pair(void) {if(!lock)return false;xSemaphoreTake(lock,portMAX_DELAY);request_pair=true;xSemaphoreGive(lock);return true;}
void cast_remote_view(cast_remote_view_t *out) {memset(out,0,sizeof(*out));if(!lock)return;xSemaphoreTake(lock,portMAX_DELAY);*out=view;xSemaphoreGive(lock);}
static bool save_identity(void) {
    nvs_handle_t h;if(nvs_open("cast_device",NVS_READWRITE,&h)!=ESP_OK)return false;
    bool ok=nvs_set_blob(h,"identity",&identity,sizeof(identity))==ESP_OK && nvs_commit(h)==ESP_OK;nvs_close(h);return ok;
}
static bool hex_value(const char *s,size_t n) {
    for(size_t i=0;i<n;i++)if(!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f')))return false;
    return s[n]==0;
}
static void random_hex(char *out,unsigned words) {
    for(unsigned i=0;i<words;i++)snprintf(out+i*8,9,"%08x",(unsigned)esp_random());
}
static bool ensure_identity(const char *origin) {
    if(identity.version==1 && !strcmp(identity.origin,origin))return true;
    identity_t loaded={0};size_t n=sizeof(loaded);nvs_handle_t h;
    esp_err_t e=nvs_open("cast_device",NVS_READWRITE,&h);if(e!=ESP_OK)return false;
    e=nvs_get_blob(h,"identity",&loaded,&n);nvs_close(h);
    if(e==ESP_OK) {
        if(n!=sizeof(loaded) || loaded.version!=1 || !memchr(loaded.origin,0,sizeof(loaded.origin)) || !cast_net_origin_valid(loaded.origin) ||
           !hex_value(loaded.id,32) || !hex_value(loaded.secret,64) || (loaded.ack[0] && !hex_value(loaded.ack,32)) || loaded.result>2)return false;
        if(!strcmp(loaded.origin,origin)) {identity=loaded;memset(&loaded,0,sizeof(loaded));return true;}
    } else if(e!=ESP_ERR_NVS_NOT_FOUND)return false;
    memset(&loaded,0,sizeof(loaded));memset(&identity,0,sizeof(identity));identity.version=1;strcpy(identity.origin,origin);
    random_hex(identity.id,4);random_hex(identity.secret,8);
    if(!save_identity()) {memset(&identity,0,sizeof(identity));return false;}
    return true;
}
static bool exchange(const char *path,const char *send,char *receive,unsigned capacity,unsigned *length) {
    response_status=0;
    char url[320],authorization[72];
    if(snprintf(url,sizeof(url),"%s%s",identity.origin,path)>=(int)sizeof(url))return false;
    snprintf(authorization,sizeof(authorization),"Bearer %s",identity.secret);
    esp_http_client_config_t cfg={.url=url,.method=send?HTTP_METHOD_POST:HTTP_METHOD_GET,.timeout_ms=8000,.disable_auto_redirect=true,.crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);if(!h)return false;
    bool ok=false;unsigned used=0;size_t bytes=send?strlen(send):0;
    if(esp_http_client_set_header(h,"Authorization",authorization)!=ESP_OK)goto done;
    if(send && esp_http_client_set_header(h,"Content-Type","application/json")!=ESP_OK)goto done;
    if(esp_http_client_open(h,(int)bytes)!=ESP_OK)goto done;
    if(send && esp_http_client_write(h,send,(int)bytes)!=(int)bytes)goto done;
    int64_t available=esp_http_client_fetch_headers(h);response_status=esp_http_client_get_status_code(h);
    if(available>(int64_t)capacity-1 || response_status!=200)goto done;
    while(used<capacity) {int n=esp_http_client_read(h,receive+used,capacity-used);if(n<0)goto done;if(!n)break;used+=(unsigned)n;}
    if(used>=capacity || !esp_http_client_is_complete_data_received(h))goto done;
    receive[used]=0;*length=used;ok=true;
done:esp_http_client_close(h);esp_http_client_cleanup(h);memset(authorization,0,sizeof(authorization));return ok;
}
bool cast_remote_poll(cast_remote_job_t *job) {
    memset(job,0,sizeof(*job));if(!lock)return false;
    cast_net_view_t network;cast_network_view(&network);char origin[161],today[11];
    if(!cast_network_origin(origin,sizeof(origin))) {show(REMOTE_UNCONFIGURED);return false;}
    if(!network.connected) {show(REMOTE_OFFLINE);return false;}
    if(!strncmp(origin,"https://",8) && !cast_network_today(today)) {show(REMOTE_WAIT_CLOCK);return false;}
    int64_t now=esp_timer_get_time();
    if(strcmp(last_origin,origin)) {
        strcpy(last_origin,origin);
        pair_until=next_poll=0;xSemaphoreTake(lock,portMAX_DELAY);memset(view.code,0,sizeof(view.code));xSemaphoreGive(lock);
    }
    xSemaphoreTake(lock,portMAX_DELAY);bool start=request_pair;request_pair=false;xSemaphoreGive(lock);
    if(start)next_poll=0;
    if(now<next_poll)return false;
    next_poll=now+10000000;
    if(!ensure_identity(origin)) {show(REMOTE_ERROR);return false;}
    if(start) {pair_until=now+300000000;next_poll=0;xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.code,sizeof(view.code),"%08X",(unsigned)esp_random());xSemaphoreGive(lock);}
    if(now>=pair_until) {xSemaphoreTake(lock,portMAX_DELAY);memset(view.code,0,sizeof(view.code));xSemaphoreGive(lock);}
    next_poll=now+10000000;
    cast_remote_view_t current;cast_remote_view(&current);
    char send[224],ack[80]="null",reply[513];unsigned length=0;
    if(identity.ack[0])snprintf(ack,sizeof(ack),"{\"id\":\"%s\",\"result\":\"%s\"}",identity.ack,identity.result==1?"saved":"failed");
#ifdef CONFIG_CAST_ARCHIVE
    const unsigned protocol=2;
#else
    const unsigned protocol=1;
#endif
    snprintf(send,sizeof(send),"{\"id\":\"%s\",\"code\":\"%s\",\"ack\":%s,\"protocol\":%u}",identity.id,current.code,ack,protocol);
    if(!exchange("/api/device/heartbeat",send,reply,sizeof(reply),&length)) {show(response_status==409 && !current.code[0]?REMOTE_READY:REMOTE_ERROR);return false;}
    bool paired=false;
    if(!cast_remote_parse_reply(reply,length,&paired,job)) {show(REMOTE_ERROR);return false;}
    xSemaphoreTake(lock,portMAX_DELAY);view.state=paired?REMOTE_PAIRED:REMOTE_PAIRING;
    if(paired) {pair_until=0;memset(view.code,0,sizeof(view.code));}view.transferring=job->id[0]!=0;xSemaphoreGive(lock);
    return job->id[0]!=0;
}
bool cast_remote_download(const cast_remote_job_t *job,char *body,unsigned capacity,unsigned *length) {
    char path[130],origin[161];
    if(!cast_network_origin(origin,sizeof(origin)) || strcmp(origin,identity.origin))return false;
    snprintf(path,sizeof(path),"/api/device/content?id=%s&job=%s",identity.id,job->id);
    return exchange(path,NULL,body,capacity,length);
}
void cast_remote_ack(const cast_remote_job_t *job,bool saved) {
    strcpy(identity.ack,job->id);identity.result=saved?1:2;
    bool stored=save_identity();next_poll=0;
    xSemaphoreTake(lock,portMAX_DELAY);view.transferring=false;if(!stored)view.state=REMOTE_ERROR;xSemaphoreGive(lock);
    if(!stored) {memset(identity.ack,0,sizeof(identity.ack));identity.result=0;}
}
