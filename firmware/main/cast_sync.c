#include "cast_sync.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "cast_network.h"
#include "cast_remote.h"
#ifdef CONFIG_CAST_ARCHIVE
#include "cast_archive.h"
#include "mbedtls/sha256.h"
static bool archive_hash(const void *body,size_t size,char hex[65]) {
    unsigned char digest[32];
    if(mbedtls_sha256(body,size,digest,0)!=0)return false;
    for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);
    return true;
}
#endif
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
static void resources(const char *stage) {
    ESP_LOGI("cast_sync","%s: heap=%u largest=%u stack=%u",stage,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
}
#else
static void resources(const char *stage) {(void)stage;}
#endif
#ifndef CONFIG_CAST_BASE_URL
#define CONFIG_CAST_BASE_URL ""
#endif
static QueueHandle_t requests,results;
static cast_snapshot_t *cached,*boot_copy;
static bool nvs_ok;
#ifdef CONFIG_CAST_WIFI_PORTAL
static QueueHandle_t profile_requests,profile_results;
static char fetch_origin[161];
static bool same_source(void) {
    nvs_handle_t h;uint8_t active=0;char origin[161];size_t n=sizeof(origin);bool same=false;
    if(nvs_open("cast_online",NVS_READONLY,&h)==ESP_OK) {
        if(nvs_get_u8(h,"active",&active)==ESP_OK && active<=1 &&
           nvs_get_str(h,active?"source1":"source0",origin,&n)==ESP_OK) same=!strcmp(origin,fetch_origin);
        nvs_close(h);
    }
    return same;
}
/* Single cached pair; the published date/revision/IDs must all match. */
static cast_profile_online_t *fetch_profile(const cast_profile_key_t *key) {
    cast_profile_online_t *out=malloc(sizeof(*out));char *record=calloc(1,161+CAST_MAX_BODY+1);
    bool valid=false;size_t size=161+CAST_MAX_BODY;nvs_handle_t nh;char source[161];
    if(!out || !record) {free(out);free(record);return NULL;}
    char *body=record+161;
    if(!cast_network_origin(source,sizeof(source))) {free(out);free(record);return NULL;}
#ifdef CONFIG_CAST_ARCHIVE
    if(cast_archive_profile(source,key,out)){free(record);return out;}
#endif
    if(nvs_open("cast_profile",NVS_READONLY,&nh)==ESP_OK) {
        if(nvs_get_blob(nh,"record",record,&size)==ESP_OK && size>161 && size<=161+CAST_MAX_BODY && memchr(record,0,161) && !strcmp(record,source))valid=cast_profile_parse(body,size-161,key,out);
        nvs_close(nh);
    }
    /* Immutable profile for a matching revision never needs to be refetched. */
    if(valid) {free(record);return out;}
    char origin[161],url[384],today[11];cast_net_view_t net;cast_network_view(&net);
    if(!net.connected || !cast_network_origin(origin,sizeof(origin)) ||
       (!strncmp(origin,"https://",8) && !cast_network_today(today)))goto done_profile;
    if(snprintf(url,sizeof(url),"%s/api/device/profile?date=%s&revision=%lu&m0=%s&m1=%s&columns=13",origin,key->date,(unsigned long)key->revision,key->ids[0],key->ids[1])>=(int)sizeof(url))goto done_profile;
    esp_http_client_config_t cfg={.url=url,.timeout_ms=8000,.disable_auto_redirect=true,.crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);if(!h)goto done_profile;
    bool received=false;
    if(esp_http_client_open(h,0)==ESP_OK && esp_http_client_fetch_headers(h)<=CAST_MAX_BODY && esp_http_client_get_status_code(h)==200) {
        size=0;
        while(size<CAST_MAX_BODY+1) {int n=esp_http_client_read(h,body+size,CAST_MAX_BODY+1-size);if(n<=0)break;size+=(size_t)n;}
        received=size<=CAST_MAX_BODY && esp_http_client_is_complete_data_received(h);
    }
    /* Return TLS/network buffers before allocating the JSON tree and pages. */
    esp_http_client_close(h);esp_http_client_cleanup(h);
    resources("profile body received");
    if(received && cast_profile_parse(body,size,key,out)) {
            /* NVS blob replacement is atomic; failure keeps the reader on its
             * historical fallback instead of claiming a durable download. */
            if(nvs_open("cast_profile",NVS_READWRITE,&nh)==ESP_OK) {
                memset(record,0,161);strcpy(record,origin);
                valid=nvs_set_blob(nh,"record",record,size+161)==ESP_OK && nvs_commit(nh)==ESP_OK;nvs_close(nh);
            }
    }
done_profile:free(record);if(!valid){free(out);out=NULL;}return out;
}
bool cast_sync_profile_request(const cast_profile_key_t *key) {return profile_requests && xQueueSend(profile_requests,key,0)==pdTRUE;}
bool cast_sync_profile_poll(cast_profile_online_t **out) {return profile_results && xQueueReceive(profile_results,out,0)==pdTRUE;}
#endif
typedef struct { cast_sync_status_t status; cast_snapshot_t *snapshot; } result_t;
#ifdef CONFIG_CAST_ARCHIVE
static bool archive_get(const char *path,char *body,size_t *size) {
    char current[161];
    if(!cast_network_origin(current,sizeof(current)) || strcmp(current,fetch_origin))return false;
    /* Long archive transfers still advertise presence every ten seconds.
     * The same pending job may be returned; it is acknowledged only at commit. */
    cast_remote_job_t pending;cast_remote_poll(&pending);
    char url[320];
    if(snprintf(url,sizeof(url),"%s%s",fetch_origin,path)>=(int)sizeof(url))return false;
    esp_http_client_config_t cfg={.url=url,.timeout_ms=8000,.disable_auto_redirect=true,.crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);if(!h)return false;
    bool ok=false;size_t used=0;
    if(esp_http_client_open(h,0)==ESP_OK && esp_http_client_fetch_headers(h)<=(int64_t)*size && esp_http_client_get_status_code(h)==200) {
        while(used<*size+1){int n=esp_http_client_read(h,body+used,*size+1-used);if(n<=0)break;used+=(size_t)n;}
        ok=used<=*size && esp_http_client_is_complete_data_received(h);
    }
    esp_http_client_close(h);esp_http_client_cleanup(h);
    if(ok)*size=used;
    return ok;
}
static result_t fetch_window(const char *date,uint32_t revision) {
    result_t r={CAST_OFFLINE,NULL};cast_net_view_t net;char today[11];
    if(!cast_network_origin(fetch_origin,sizeof(fetch_origin))){r.status=CAST_NOT_CONFIGURED;return r;}
    cast_network_view(&net);bool connected=net.connected;
    if(connected && !strncmp(fetch_origin,"https://",8) && !cast_network_today(today)){r.status=CAST_CLOCK_WAIT;return r;}
    if(connected && !cast_archive_update(fetch_origin,date,revision,archive_get)) {
        r.status=CAST_SAVE_FAILED;return r;
    }
    char current_origin[161];
    if(!cast_network_origin(current_origin,sizeof(current_origin)) || strcmp(current_origin,fetch_origin)){r.status=CAST_INVALID;return r;}
    cast_snapshot_t *snapshot=malloc(sizeof(*snapshot)),*copy=malloc(sizeof(*copy));
    if(!snapshot || !copy){free(snapshot);free(copy);r.status=CAST_SAVE_FAILED;return r;}
    if(!cast_archive_lineup(fetch_origin,date,snapshot) || (revision && revision!=snapshot->revision)) {
        free(snapshot);free(copy);r.status=CAST_UNPUBLISHED;return r;
    }
    *copy=*snapshot;free(cached);cached=copy;r.snapshot=snapshot;r.status=connected?CAST_LATEST:CAST_CACHED;return r;
}
#endif
cast_snapshot_t *cast_sync_cached(void) {cast_snapshot_t *out=boot_copy;boot_copy=NULL;return out;}
/* Inactive NVS slot is written/committed/read-back validated before atomic active-key
 * switch. Power loss leaves either previous or next complete slot selected. */
static bool save(const char *body,size_t size,const cast_snapshot_t *next) {
    nvs_handle_t h; uint8_t active=0;bool ok=false;
    if(!nvs_ok || nvs_open("cast_online",NVS_READWRITE,&h)!=ESP_OK) return false;
    nvs_get_u8(h,"active",&active);active=active==0?1:0;
    const char *slot=active?"slot1":"slot0";
#ifdef CONFIG_CAST_WIFI_PORTAL
    if(nvs_set_str(h,active?"source1":"source0",fetch_origin)!=ESP_OK)goto done;
#endif
    if(nvs_set_blob(h,slot,body,size)!=ESP_OK || nvs_commit(h)!=ESP_OK) goto done;
    char *copy=malloc(size);cast_snapshot_t *verify=malloc(sizeof(*verify));size_t n=size;
    if(copy && verify && nvs_get_blob(h,slot,copy,&n)==ESP_OK && n==size &&
       cast_protocol_parse(copy,n,next->date,verify) && !memcmp(verify,next,sizeof(*next)))
        ok=nvs_set_u8(h,"active",active)==ESP_OK && nvs_commit(h)==ESP_OK;
    free(copy);free(verify);
done: nvs_close(h);return ok;
}
static void load(void) {
    nvs_handle_t h;uint8_t active;size_t n=0;
    if(!nvs_ok || nvs_open("cast_online",NVS_READONLY,&h)!=ESP_OK) return;
    if(nvs_get_u8(h,"active",&active)!=ESP_OK || active>1) {nvs_close(h);return;}
    const char *slot=active?"slot1":"slot0";
    if(nvs_get_blob(h,slot,NULL,&n)==ESP_OK && n>0 && n<=CAST_MAX_BODY) {
        char *b=calloc(1,n+1);cast_snapshot_t *s=malloc(sizeof(*s));
        /* Stored request date accompanies the validated JSON slot, not wall-clock. */
        if(b && s && nvs_get_blob(h,slot,b,&n)==ESP_OK) {
            if(cast_protocol_parse_cached(b,n,s)) {cached=s;s=NULL;}
        }
        free(b);free(s);
    }
    nvs_close(h);
}
static result_t accept_received(const char *body,size_t used,const char *date,uint32_t revision) {
    result_t r={CAST_INVALID,NULL};
    const cast_snapshot_t *previous=cached;
#ifdef CONFIG_CAST_WIFI_PORTAL
    char current[161];
    if(!cast_network_origin(current,sizeof(current)) || strcmp(current,fetch_origin))return r;
    if(!same_source())previous=NULL;
#endif
    cast_snapshot_t *s=malloc(sizeof(*s));
    if(!s)return r;
    if(!cast_protocol_parse(body,used,date,s) || (revision && s->revision!=revision) || !cast_revision_accept(previous,s)) {free(s);return r;}
    cast_snapshot_t *c=malloc(sizeof(*c));
    if(!c) {free(s);return r;}
    if(!save(body,used,s)) {r.status=CAST_SAVE_FAILED;free(c);free(s);return r;}
    *c=*s;free(cached);cached=c;r.snapshot=s;r.status=CAST_LATEST;return r;
}
#ifdef CONFIG_CAST_WIFI_PORTAL
static result_t fetch_remote(const cast_remote_job_t *job) {
#ifdef CONFIG_CAST_ARCHIVE
    return fetch_window(job->date,job->revision);
#else
    result_t r={CAST_OFFLINE,NULL};unsigned used=0;
    if(!cast_network_origin(fetch_origin,sizeof(fetch_origin)))return r;
    char *body=malloc(CAST_MAX_BODY+1);
    if(body && cast_remote_download(job,body,CAST_MAX_BODY+1,&used))r=accept_received(body,used,job->date,job->revision);
    free(body);resources("website upload finished");return r;
#endif
}
#endif
static result_t __attribute__((unused)) fetch(const char *date) {
    result_t r={CAST_OFFLINE,NULL};
#ifdef CONFIG_CAST_WIFI_PORTAL
    char origin[161];
    if(!cast_network_origin(origin,sizeof(origin))) {r.status=CAST_NOT_CONFIGURED;return r;}
    strcpy(fetch_origin,origin);
    char today[11];
    if(!strncmp(origin,"https://",8) && !cast_network_today(today)) {r.status=CAST_CLOCK_WAIT;return r;}
#else
    const char *origin=CONFIG_CAST_BASE_URL;
    if(!CONFIG_CAST_BASE_URL[0]) {r.status=CAST_NOT_CONFIGURED;return r;}
#endif
    esp_netif_t *net=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");esp_netif_ip_info_t ip;
    if(!net || esp_netif_get_ip_info(net,&ip)!=ESP_OK || !ip.ip.addr) {
        return r;
    }
    char url[256];
    if(snprintf(url,sizeof(url),"%s/api/lineup?date=%s",origin,date)>=(int)sizeof(url)) {r.status=CAST_INVALID;return r;}
    esp_http_client_config_t cfg={.url=url,.timeout_ms=8000,.disable_auto_redirect=true,
        .crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);
    if(!h) return r;
    char *body=malloc(CAST_MAX_BODY+1);size_t used=0;
    if(!body) goto done;
    if(esp_http_client_open(h,0)!=ESP_OK) goto done;
    int64_t length=esp_http_client_fetch_headers(h);
    int code=esp_http_client_get_status_code(h);
    if(code==404 || code==204) {r.status=CAST_UNPUBLISHED;goto done;}
    if(code!=200 || length>CAST_MAX_BODY) {r.status=CAST_INVALID;goto done;}
    while(used<CAST_MAX_BODY+1) {
        int got=esp_http_client_read(h,body+used,CAST_MAX_BODY+1-used);
        if(got<0) goto done;
        if(got==0) break;
        used+=got;
    }
    if(used>CAST_MAX_BODY || !esp_http_client_is_complete_data_received(h)) {r.status=CAST_INVALID;goto done;}
    /* TLS buffers must not overlap the parser, new snapshot and read-back copy. */
    esp_http_client_close(h);esp_http_client_cleanup(h);h=NULL;
    resources("lineup body received");
    r=accept_received(body,used,date,0);
done:
    free(body);if(h){esp_http_client_close(h);esp_http_client_cleanup(h);}resources("lineup finished");return r;
}
static void worker(void *arg) {
    (void)arg;char date[11];
#ifdef CONFIG_CAST_ARCHIVE
    /* Formatting first-use storage must never hold the screen dark at boot. */
    if(cast_archive_init(archive_hash)) {
        char origin[161],today[11];cast_snapshot_t *snapshot=malloc(sizeof(*snapshot));
        if(snapshot && cast_network_origin(origin,sizeof(origin)) && cast_archive_today(origin,today) && cast_archive_lineup(origin,today,snapshot)) {
            cast_snapshot_t *copy=malloc(sizeof(*copy));
            if(copy){*copy=*snapshot;free(cached);cached=copy;result_t r={CAST_CACHED,snapshot};xQueueSend(results,&r,portMAX_DELAY);snapshot=NULL;}
        }free(snapshot);
    }
#endif
#ifdef CONFIG_CAST_WIFI_PORTAL
    for(;;) {
        if(xQueueReceive(requests,date,pdMS_TO_TICKS(100))==pdTRUE) {
#ifdef CONFIG_CAST_ARCHIVE
            result_t r=fetch_window(date,0);
#else
            result_t r=fetch(date);
#endif
            xQueueSend(results,&r,portMAX_DELAY);
        }
        cast_profile_key_t key;
        if(xQueueReceive(profile_requests,&key,0)==pdTRUE) {
            cast_profile_online_t *p=fetch_profile(&key);xQueueSend(profile_results,&p,portMAX_DELAY);
        }
        cast_remote_job_t job;
        if(cast_remote_poll(&job)) {
            result_t r=fetch_remote(&job);
            char current[161];
            if(cast_network_origin(current,sizeof(current)) && !strcmp(current,fetch_origin))
                cast_remote_ack(&job,r.status==CAST_LATEST);
            xQueueSend(results,&r,portMAX_DELAY);
        }
    }
#else
    for(;;) if(xQueueReceive(requests,date,portMAX_DELAY)==pdTRUE) {
        result_t r=fetch(date);xQueueSend(results,&r,portMAX_DELAY);
    }
#endif
}
bool cast_sync_init(void) {
    nvs_ok=nvs_flash_init()==ESP_OK; /* Do not erase on version/full errors. */
    load();
    if(cached){boot_copy=malloc(sizeof(*boot_copy));if(boot_copy)*boot_copy=*cached;}
#ifdef CONFIG_CAST_WIFI_PORTAL
    if(!nvs_ok || !cast_network_init() || !cast_remote_init()) return false;
#else
    if(CONFIG_CAST_BASE_URL[0] && nvs_ok && esp_netif_init()==ESP_OK) {
        esp_err_t e=esp_event_loop_create_default();
        if(e==ESP_OK || e==ESP_ERR_INVALID_STATE) {
            if(!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) esp_netif_create_default_wifi_sta();
            wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
            if(esp_wifi_init(&cfg)==ESP_OK) {
                wifi_config_t w={0};
                if(esp_wifi_get_config(WIFI_IF_STA,&w)==ESP_OK && w.sta.ssid[0] &&
                   esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK && esp_wifi_start()==ESP_OK) esp_wifi_connect();
            }
        }
    }
#endif
    results=xQueueCreate(1,sizeof(result_t));
    if(!results) return false;
    /* Offline builds need neither a network worker nor its 6 KiB stack. */
#ifndef CONFIG_CAST_WIFI_PORTAL
    if(!CONFIG_CAST_BASE_URL[0]) return true;
#endif
    requests=xQueueCreate(1,11);
#ifdef CONFIG_CAST_WIFI_PORTAL
    profile_requests=xQueueCreate(1,sizeof(cast_profile_key_t));profile_results=xQueueCreate(1,sizeof(cast_profile_online_t*));
    if(!profile_requests || !profile_results)goto failed;
#endif
    if(requests && xTaskCreate(worker,"cast_sync",6144,NULL,3,NULL)==pdPASS) return true;
#ifdef CONFIG_CAST_WIFI_PORTAL
failed:
    if(profile_requests)vQueueDelete(profile_requests);
    if(profile_results)vQueueDelete(profile_results);
    profile_requests=NULL;profile_results=NULL;
#endif
    if(requests) vQueueDelete(requests);
    vQueueDelete(results);requests=NULL;results=NULL;
    return false;
}
bool cast_sync_request(const char *date) {
    if(!cast_valid_date(date)) return false;
#ifndef CONFIG_CAST_WIFI_PORTAL
    if(!CONFIG_CAST_BASE_URL[0]) {
        result_t r={CAST_NOT_CONFIGURED,NULL};
        return results && xQueueSend(results,&r,0)==pdTRUE;
    }
#endif
    return requests && cast_valid_date(date) && xQueueSend(requests,date,0)==pdTRUE;
}
bool cast_sync_poll(cast_sync_status_t *status,cast_snapshot_t **snapshot) {
    result_t r;if(!results || xQueueReceive(results,&r,0)!=pdTRUE) return false;
    *status=r.status;*snapshot=r.snapshot;return true;
}
