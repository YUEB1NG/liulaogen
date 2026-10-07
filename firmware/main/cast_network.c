/* Wi-Fi and provisioning lifecycle is owned by one worker, never by LVGL. */
#include "cast_network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_http_server.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "cJSON.h"
#include "cast_portal_page.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#ifndef CONFIG_CAST_BASE_URL
#define CONFIG_CAST_BASE_URL ""
#endif

#define HAS_IP BIT0
typedef enum { OPEN, CLOSE, CLEAR, APPLY, PHONE_OPEN, PHONE_CONNECT, SCAN, DEVICE_APPLY, SERVICE } command_kind_t;
typedef struct { command_kind_t kind; cast_net_config_t config; } command_t;
static QueueHandle_t commands;
static SemaphoreHandle_t mutex;
static EventGroupHandle_t events;
static cast_net_config_t saved;
static cast_net_config_t phone;
static cast_net_view_t view;
static httpd_handle_t http;
static char token[33];
static bool wifi_ready, sntp_started, network_ready, apply_pending;
static esp_netif_t *ap;

static void state(cast_net_state_t value) {
    xSemaphoreTake(mutex,portMAX_DELAY);view.state=value;view.state_generation++;
    if(value==NET_FAILED || value==NET_SAVED || value==NET_STORAGE_FAILED || value==NET_IDLE)apply_pending=false;
    xSemaphoreGive(mutex);
}
void cast_network_view(cast_net_view_t *out) {
    memset(out,0,sizeof(*out)); if(!network_ready) return;
    xSemaphoreTake(mutex,portMAX_DELAY);*out=view;xSemaphoreGive(mutex);
    out->connected=(xEventGroupGetBits(events)&HAS_IP)!=0;
}
bool cast_network_origin(char *out,size_t size) {
    if(!network_ready || !out || !size) return false;
    xSemaphoreTake(mutex,portMAX_DELAY);
    const char *origin=saved.origin[0]?saved.origin:CONFIG_CAST_BASE_URL;
    bool ok=cast_net_origin_valid(origin) && strlen(origin)<size;
    if(ok) memcpy(out,origin,strlen(origin)+1); else *out=0;
    xSemaphoreGive(mutex); return ok;
}
bool cast_network_today(char out[11]) {
    time_t now=time(NULL); if(now<1767225600 || now>4102444799LL) return false;
    now+=8*3600;struct tm t;gmtime_r(&now,&t);return strftime(out,11,"%Y-%m-%d",&t)==10;
}
static bool submit(command_kind_t kind) {
    command_t c={.kind=kind};return network_ready && xQueueSend(commands,&c,0)==pdTRUE;
}
bool cast_network_setup(void) {return submit(SCAN);}
bool cast_network_connect(const char *ssid,const char *password) {
    if(!network_ready || !ssid || !password || !*ssid || strlen(ssid)>32 || strlen(password)>63)return false;
    command_t c={.kind=DEVICE_APPLY,.config={.version=1}};
    strcpy(c.config.ssid,ssid);strcpy(c.config.password,password);
    bool ok=cast_net_config_valid(&c.config) && xQueueSend(commands,&c,0)==pdTRUE;
    memset(&c,0,sizeof(c));return ok;
}
bool cast_network_set_origin(const char *origin) {
    if(!network_ready || !cast_net_origin_valid(origin))return false;
    command_t c={.kind=SERVICE};strcpy(c.config.origin,origin);
    return xQueueSend(commands,&c,0)==pdTRUE;
}
bool cast_network_phone_connect(void) {return submit(PHONE_CONNECT);}
bool cast_network_web_setup(void) {return submit(OPEN);}
bool cast_network_cancel(void) {return submit(CLOSE);}
bool cast_network_forget(void) {return submit(CLEAR);}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;(void)data;
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) xEventGroupSetBits(events,HAS_IP);
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) xEventGroupClearBits(events,HAS_IP);
}
static bool initialize_wifi(void) {
    if(wifi_ready) return true;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&init)!=ESP_OK) return false;
    if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK ||
       esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK || esp_wifi_start()!=ESP_OK) {esp_wifi_stop();esp_wifi_deinit();return false;}
    wifi_ready=true;return true;
}
static bool connect_to(const cast_net_config_t *config) {
    esp_wifi_disconnect(); xEventGroupClearBits(events,HAS_IP);
    if(!config->ssid[0]) return false;
    wifi_config_t station={0};
    memcpy(station.sta.ssid,config->ssid,strlen(config->ssid));
    memcpy(station.sta.password,config->password,strlen(config->password));
    station.sta.pmf_cfg.capable=true;
    station.sta.threshold.authmode=config->password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
    return esp_wifi_set_config(WIFI_IF_STA,&station)==ESP_OK && esp_wifi_connect()==ESP_OK;
}
static void restore_saved(void) {
    if(!wifi_ready)return;
    connect_to(&saved);
    if(!saved.ssid[0] && !http) {
        wifi_config_t empty={0};esp_wifi_set_config(WIFI_IF_STA,&empty);
        esp_wifi_stop();esp_wifi_deinit();wifi_ready=false;
    }
}
static bool local_request(httpd_req_t *req) {
    struct sockaddr_in addr;socklen_t size=sizeof(addr);esp_netif_ip_info_t ip;
    char host[48];
    return ap && esp_netif_get_ip_info(ap,&ip)==ESP_OK &&
      getsockname(httpd_req_to_sockfd(req),(struct sockaddr*)&addr,&size)==0 &&
      addr.sin_addr.s_addr==ip.ip.addr &&
      httpd_req_get_hdr_value_str(req,"Host",host,sizeof(host))==ESP_OK &&
      (!strcmp(host,"192.168.4.1") || !strcmp(host,"192.168.4.1:80"));
}
static esp_err_t respond(httpd_req_t *req,const char *status,const char *body) {
    httpd_resp_set_status(req,status);httpd_resp_set_type(req,"application/json; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,body,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t page(httpd_req_t *req) {
    if(!local_request(req)) return respond(req,"403 Forbidden","{}");
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-Frame-Options","DENY");
    httpd_resp_send_chunk(req,CAST_PORTAL_BEGIN,strlen(CAST_PORTAL_BEGIN));
    httpd_resp_send_chunk(req,token,strlen(token));
    httpd_resp_send_chunk(req,CAST_PORTAL_END,strlen(CAST_PORTAL_END));
    return httpd_resp_send_chunk(req,NULL,0);
}
static esp_err_t status_page(httpd_req_t *req) {
    if(!local_request(req)) return respond(req,"403 Forbidden","{}");
    cast_net_view_t v;cast_network_view(&v);char body[80];
    snprintf(body,sizeof(body),"{\"state\":%d,\"connected\":%s}",v.state,v.connected?"true":"false");
    return respond(req,"200 OK",body);
}
static esp_err_t configure(httpd_req_t *req) {
    char received[40],origin[48],type[64];
    if(!local_request(req) || httpd_req_get_hdr_value_str(req,"X-Setup-Token",received,sizeof(received))!=ESP_OK ||
       strcmp(received,token) || httpd_req_get_hdr_value_str(req,"Origin",origin,sizeof(origin))!=ESP_OK ||
       (strcmp(origin,"http://192.168.4.1") && strcmp(origin,"http://192.168.4.1:80"))) return respond(req,"403 Forbidden","{}");
    if(httpd_req_get_hdr_value_str(req,"Content-Type",type,sizeof(type))!=ESP_OK || strcmp(type,"application/json")) return respond(req,"415 Unsupported Media Type","{}");
    if(req->content_len<1 || req->content_len>768) return respond(req,"413 Content Too Large","{}");
    char body[769];size_t used=0;
    while(used<req->content_len) {int n=httpd_req_recv(req,body+used,req->content_len-used);if(n<=0) return ESP_FAIL;used+=(size_t)n;}
    body[used]=0;
    if(strlen(body)!=used || strstr(body,"\\u0000")) return respond(req,"400 Bad Request","{}");
    cJSON *json=cJSON_ParseWithLengthOpts(body,used+1,NULL,true);
    command_t c={.kind=APPLY,.config={.version=1}};bool valid=cJSON_IsObject(json) && cJSON_GetArraySize(json)==3;
    const char *names[]={"ssid","password","origin"};char *values[]={c.config.ssid,c.config.password,c.config.origin};
    size_t limits[]={sizeof(c.config.ssid),sizeof(c.config.password),sizeof(c.config.origin)};
    for(unsigned i=0;i<3;i++) {
        cJSON *v=cJSON_GetObjectItemCaseSensitive(json,names[i]);unsigned count=0;
        for(cJSON *item=json?json->child:NULL;item;item=item->next) if(item->string && !strcmp(item->string,names[i])) count++;
        if(count!=1 || !cJSON_IsString(v) || strlen(v->valuestring)>=limits[i]) {valid=false;continue;}
        strcpy(values[i],v->valuestring);
    }
    cJSON_Delete(json);memset(body,0,sizeof(body));
    if(!valid || !c.config.ssid[0] || !cast_net_config_valid(&c.config) || !cast_net_origin_valid(c.config.origin)) {memset(&c,0,sizeof(c));return respond(req,"422 Unprocessable Entity","{\"error\":\"请核对 SSID、密码和服务地址；公网须使用 HTTPS，局域网 HTTP 须为私有 IPv4。\"}");}
    xSemaphoreTake(mutex,portMAX_DELAY);
    bool queued=!apply_pending && view.state!=NET_CONNECTING && view.state!=NET_SAVED;
    if(queued)apply_pending=true;
    xSemaphoreGive(mutex);
    if(queued && xQueueSend(commands,&c,0)!=pdTRUE) {
        xSemaphoreTake(mutex,portMAX_DELAY);apply_pending=false;xSemaphoreGive(mutex);queued=false;
    }
    memset(&c,0,sizeof(c));
    return respond(req,queued?"202 Accepted":"409 Conflict",queued?"{\"ok\":true}":"{\"error\":\"设备忙，请稍后重试\"}");
}
static void close_portal(void) {
    if(http) {httpd_stop(http);http=NULL;}
    if(wifi_ready) esp_wifi_set_mode(WIFI_MODE_STA);
    xSemaphoreTake(mutex,portMAX_DELAY);view.portal=false;apply_pending=false;memset(view.password,0,sizeof(view.password));xSemaphoreGive(mutex);
    memset(token,0,sizeof(token));
}
static bool open_portal(void) {
    if(http) return true;
    wifi_config_t config={0};char name[24],password[17];
    snprintf(name,sizeof(name),"Passport-%04X",(unsigned)(esp_random()&65535));
    snprintf(password,sizeof(password),"%08x%08x",(unsigned)esp_random(),(unsigned)esp_random());
    snprintf(token,sizeof(token),"%08x%08x%08x%08x",(unsigned)esp_random(),(unsigned)esp_random(),(unsigned)esp_random(),(unsigned)esp_random());
    strcpy((char*)config.ap.ssid,name);strcpy((char*)config.ap.password,password);
    config.ap.ssid_len=strlen(name);config.ap.channel=1;config.ap.max_connection=1;config.ap.authmode=WIFI_AUTH_WPA2_PSK;
    if(esp_wifi_set_mode(WIFI_MODE_APSTA)!=ESP_OK || esp_wifi_set_config(WIFI_IF_AP,&config)!=ESP_OK) {close_portal();return false;}
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=6144;cfg.max_open_sockets=3;cfg.backlog_conn=2;
    cfg.recv_wait_timeout=5;cfg.send_wait_timeout=5;cfg.lru_purge_enable=true;
    if(httpd_start(&http,&cfg)!=ESP_OK) {close_portal();return false;}
    const httpd_uri_t handlers[]={ {.uri="/",.method=HTTP_GET,.handler=page}, {.uri="/status",.method=HTTP_GET,.handler=status_page}, {.uri="/configure",.method=HTTP_POST,.handler=configure} };
    for(unsigned i=0;i<3;i++) if(httpd_register_uri_handler(http,&handlers[i])!=ESP_OK) {close_portal();return false;}
    xSemaphoreTake(mutex,portMAX_DELAY);view.portal=true;view.state=NET_SETUP;strcpy(view.ssid,name);strcpy(view.password,password);xSemaphoreGive(mutex);
    memset(password,0,sizeof(password));memset(&config,0,sizeof(config));return true;
}
static bool persist(const cast_net_config_t *config) {
    nvs_handle_t h;if(nvs_open("cast_network",NVS_READWRITE,&h)!=ESP_OK) return false;
    bool ok=nvs_set_blob(h,"config",config,sizeof(*config))==ESP_OK && nvs_commit(h)==ESP_OK;nvs_close(h);return ok;
}
/* Persistent per-device preset, generated while the Wi-Fi RNG is active.
 * It is separate from the active station configuration until DHCP succeeds. */
static bool prepare_phone(void) {
    if(phone.ssid[0])return true;
    nvs_handle_t h;if(nvs_open("cast_network",NVS_READWRITE,&h)!=ESP_OK)return false;
    size_t n=sizeof(phone);esp_err_t e=nvs_get_blob(h,"phone",&phone,&n);
    bool valid=e==ESP_OK && n==sizeof(phone) && cast_net_config_valid(&phone) &&
        strlen(phone.ssid)<sizeof(view.ssid) && strlen(phone.password)==16 && !phone.origin[0];
    if(!valid) {
        memset(&phone,0,sizeof(phone));
        if(e!=ESP_ERR_NVS_NOT_FOUND) {nvs_close(h);return false;}
        phone.version=1;
        snprintf(phone.ssid,sizeof(phone.ssid),"Passport-%04X",(unsigned)(esp_random()&65535));
        snprintf(phone.password,sizeof(phone.password),"%08x%08x",(unsigned)esp_random(),(unsigned)esp_random());
        valid=nvs_set_blob(h,"phone",&phone,sizeof(phone))==ESP_OK && nvs_commit(h)==ESP_OK;
    }
    nvs_close(h);if(!valid)memset(&phone,0,sizeof(phone));return valid;
}
static void phone_view(bool enabled) {
    xSemaphoreTake(mutex,portMAX_DELAY);view.phone=enabled;
    if(enabled) {strcpy(view.ssid,phone.ssid);strcpy(view.password,phone.password);}
    else {memset(view.ssid,0,sizeof(view.ssid));memset(view.password,0,sizeof(view.password));}
    xSemaphoreGive(mutex);
}
static void connection_diagnostic(const char *stage) {
    ESP_LOGI("cast_network","%s; heap=%u largest=%u stack=%u",stage,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static void scan_networks(void) {
    xSemaphoreTake(mutex,portMAX_DELAY);view.scanning=true;view.scan_failed=false;xSemaphoreGive(mutex);
    uint16_t n=CAST_WIFI_RESULTS;wifi_ap_record_t *records=calloc(n,sizeof(*records));
    bool ok=records && initialize_wifi() && esp_wifi_scan_start(NULL,true)==ESP_OK && esp_wifi_scan_get_ap_records(&n,records)==ESP_OK;
    if(!ok && wifi_ready)esp_wifi_clear_ap_list();
    xSemaphoreTake(mutex,portMAX_DELAY);view.count=0;memset(view.access,0,sizeof(view.access));
    if(ok)for(unsigned i=0;i<n;i++) {
        records[i].ssid[32]=0;if(!records[i].ssid[0])continue;
        bool duplicate=false;
        for(size_t k=0;k<view.count;k++)if(!strcmp(view.access[k].ssid,(char *)records[i].ssid))duplicate=true;
        if(duplicate)continue;
        cast_access_point_t *a=&view.access[view.count++];strcpy(a->ssid,(char *)records[i].ssid);
        a->rssi=records[i].rssi;a->secure=records[i].authmode!=WIFI_AUTH_OPEN;
    }
    view.scanning=false;view.scan_failed=!ok;view.scan_generation++;xSemaphoreGive(mutex);free(records);
    connection_diagnostic(ok?"Wi-Fi scan complete":"Wi-Fi scan failed");
}
static void worker(void *arg) {
    (void)arg;command_t c;cast_net_config_t candidate={0};bool testing=false,phone_mode=false;
    int64_t deadline=0,portal_until=0,close_at=0,retry_at=0;
    if(saved.ssid[0] && initialize_wifi()) {connect_to(&saved);state(NET_CONNECTING);retry_at=esp_timer_get_time()+15000000;}
    for(;;) {
        if(xQueueReceive(commands,&c,pdMS_TO_TICKS(250))==pdTRUE) {
            if(c.kind==SCAN && !testing) {close_portal();phone_mode=false;phone_view(false);scan_networks();}
            if(c.kind==DEVICE_APPLY && !testing) {
                close_portal();phone_mode=false;phone_view(false);candidate=c.config;
                strcpy(candidate.origin,saved.origin);
                testing=initialize_wifi() && connect_to(&candidate);deadline=esp_timer_get_time()+30000000;
                state(testing?NET_CONNECTING:NET_FAILED);connection_diagnostic("on-device Wi-Fi connection requested");
                if(!testing) {memset(&candidate,0,sizeof(candidate));restore_saved();}
            }
            if(c.kind==SERVICE && !testing) {
                cast_net_config_t next=saved;next.version=1;strcpy(next.origin,c.config.origin);
                if(persist(&next)) {xSemaphoreTake(mutex,portMAX_DELAY);saved=next;xSemaphoreGive(mutex);state(NET_SAVED);}
                else state(NET_STORAGE_FAILED);
                memset(&next,0,sizeof(next));
            }
            if(c.kind==PHONE_OPEN && !testing) {
                close_portal();close_at=0;phone_mode=false;phone_view(false);
                if(!initialize_wifi())state(NET_FAILED);
                else if(!prepare_phone()) {state(NET_STORAGE_FAILED);restore_saved();}
                else {phone_mode=true;phone_view(true);state(NET_SETUP);connection_diagnostic("phone hotspot setup ready");}
            }
            if(c.kind==PHONE_CONNECT && phone_mode && !testing) {
                candidate=phone;strcpy(candidate.origin,saved.origin);
                if(!candidate.origin[0] && cast_net_origin_valid(CONFIG_CAST_BASE_URL))strcpy(candidate.origin,CONFIG_CAST_BASE_URL);
                testing=connect_to(&candidate);deadline=esp_timer_get_time()+30000000;
                state(testing?NET_CONNECTING:NET_FAILED);connection_diagnostic("phone hotspot connection requested");
                if(!testing) {memset(&candidate,0,sizeof(candidate));restore_saved();}
            }
            if(c.kind==OPEN && !testing) {phone_mode=false;phone_view(false);if(initialize_wifi() && open_portal()) {portal_until=esp_timer_get_time()+300000000;close_at=0;}else state(NET_FAILED);}
            if(c.kind==CLOSE) {testing=false;phone_mode=false;memset(&candidate,0,sizeof(candidate));close_at=0;close_portal();phone_view(false);restore_saved();state(NET_IDLE);}
            if(c.kind==CLEAR) {
                nvs_handle_t h;bool ok=nvs_open("cast_network",NVS_READWRITE,&h)==ESP_OK;
                if(ok) {esp_err_t e=nvs_erase_key(h,"config"),p=nvs_erase_key(h,"phone");ok=(e==ESP_OK || e==ESP_ERR_NVS_NOT_FOUND) && (p==ESP_OK || p==ESP_ERR_NVS_NOT_FOUND) && nvs_commit(h)==ESP_OK;nvs_close(h);}
                if(ok) {testing=false;phone_mode=false;memset(&candidate,0,sizeof(candidate));memset(&phone,0,sizeof(phone));close_at=0;close_portal();phone_view(false);xSemaphoreTake(mutex,portMAX_DELAY);memset(&saved,0,sizeof(saved));xSemaphoreGive(mutex);restore_saved();state(NET_IDLE);} else state(NET_STORAGE_FAILED);
            }
            if(c.kind==APPLY && http && !testing) {
                candidate=c.config;testing=connect_to(&candidate);deadline=esp_timer_get_time()+30000000;state(testing?NET_CONNECTING:NET_FAILED);
                if(!testing) {memset(&candidate,0,sizeof(candidate));connect_to(&saved);}
            }
            memset(&c,0,sizeof(c));
        }
        int64_t now=esp_timer_get_time();bool connected=(xEventGroupGetBits(events)&HAS_IP)!=0;
        if(connected && !sntp_started) {esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();sntp_started=true;}
        wifi_ap_record_t association={0};
        bool candidate_connected=testing && connected && esp_wifi_sta_get_ap_info(&association)==ESP_OK &&
            !strncmp((const char*)association.ssid,candidate.ssid,32);
        if(candidate_connected) {
            testing=false;
            if(persist(&candidate)) {xSemaphoreTake(mutex,portMAX_DELAY);saved=candidate;xSemaphoreGive(mutex);state(NET_SAVED);close_at=http?now+10000000:0;connection_diagnostic("station configuration saved after DHCP");}
            else {connect_to(&saved);state(NET_STORAGE_FAILED);connection_diagnostic("station configuration save failed");}
            memset(&candidate,0,sizeof(candidate));
        } else if(testing && now>=deadline) {testing=false;memset(&candidate,0,sizeof(candidate));connect_to(&saved);state(NET_FAILED);connection_diagnostic("station trial timed out; restored previous configuration");}
        if(http && ((close_at && now>=close_at) || now>=portal_until)) {testing=false;memset(&candidate,0,sizeof(candidate));close_portal();close_at=0;restore_saved();state(NET_IDLE);}
        if(!testing && !connected && saved.ssid[0] && now>=retry_at) {esp_wifi_connect();retry_at=now+15000000;}
        if(!http && !phone_mode && !testing && connected && (view.state==NET_IDLE || view.state==NET_CONNECTING)) state(NET_ONLINE);
    }
}
bool cast_network_init(void) {
    if(network_ready) return true;
    if(esp_netif_init()!=ESP_OK) return false;
    esp_err_t e=esp_event_loop_create_default();if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE) return false;
    bool wifi_registered=false,ip_registered=false;
    esp_netif_t *sta=esp_netif_create_default_wifi_sta();if(!sta) return false;
    ap=esp_netif_create_default_wifi_ap();if(!ap)goto failed;
    mutex=xSemaphoreCreateMutex();events=xEventGroupCreate();commands=xQueueCreate(2,sizeof(command_t));
    if(!mutex || !events || !commands)goto failed;
    wifi_registered=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL)==ESP_OK;
    ip_registered=esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL)==ESP_OK;
    if(!wifi_registered || !ip_registered)goto failed;
    nvs_handle_t h;size_t n=sizeof(saved);
    if(nvs_open("cast_network",NVS_READONLY,&h)==ESP_OK) {
        if(nvs_get_blob(h,"config",&saved,&n)!=ESP_OK || n!=sizeof(saved) || !cast_net_config_valid(&saved)) memset(&saved,0,sizeof(saved));
        nvs_close(h);
    }
    network_ready=xTaskCreate(worker,"cast_network",6144,NULL,3,NULL)==pdPASS;
    if(network_ready)return true;
failed:
    if(ip_registered)esp_event_handler_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event);
    if(wifi_registered)esp_event_handler_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event);
    if(commands)vQueueDelete(commands);
    if(events)vEventGroupDelete(events);
    if(mutex)vSemaphoreDelete(mutex);
    commands=NULL;events=NULL;mutex=NULL;
    if(ap)esp_netif_destroy_default_wifi(ap);
    esp_netif_destroy_default_wifi(sta);ap=NULL;return false;
}
