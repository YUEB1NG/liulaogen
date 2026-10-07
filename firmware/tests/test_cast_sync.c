/* Execute the real cache/sync implementation with deterministic fault injection. */
#include "cast_stubs/platform.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../main/cast_sync.c"

struct queue { unsigned size; bool full; unsigned char data[128]; };
static unsigned task_count, radio_count, fault, operation;
static bool online=true, complete=true, nvs_available=true;
static bool http_live;
static int http_code=200;
static const char *http_body;
static size_t http_cursor;
static char slots[2][CAST_MAX_BODY];
static size_t lengths[2];
static uint8_t active_slot, staged_slot;
static bool has_active, staged;
static char sources[2][161],profile_record[CAST_MAX_BODY+161];
static size_t profile_size;
static const char *server_origin="https://example.invalid";
static bool clock_ready=true;
bool cast_network_init(void) {return true;}
bool cast_network_origin(char *out,size_t capacity) {if(strlen(server_origin)>=capacity)return false;strcpy(out,server_origin);return *out!=0;}
bool cast_network_today(char out[11]) {strcpy(out,"2026-10-03");return clock_ready;}
void cast_network_view(cast_net_view_t *out) {memset(out,0,sizeof(*out));out->connected=online;}
bool cast_remote_init(void) {return true;}
bool cast_remote_poll(cast_remote_job_t *j) {(void)j;return false;}
bool cast_remote_download(const cast_remote_job_t *j,char *b,unsigned c,unsigned *n) {(void)j;(void)b;(void)c;(void)n;return false;}
void cast_remote_ack(const cast_remote_job_t *j,bool s) {(void)j;(void)s;}
static bool fail(void) { return fault && ++operation==fault; }
QueueHandle_t xQueueCreate(unsigned n,unsigned size) { assert(n==1 && size<=128);struct queue *q=calloc(1,sizeof(*q));q->size=size;return q; }
int xQueueSend(QueueHandle_t q,const void *data,unsigned ticks) { (void)ticks;if(q->full)return 0;memcpy(q->data,data,q->size);q->full=true;return 1; }
int xQueueReceive(QueueHandle_t q,void *data,unsigned ticks) { (void)ticks;if(!q->full)return 0;memcpy(data,q->data,q->size);q->full=false;return 1; }
void vQueueDelete(QueueHandle_t q) { free(q); }
int xTaskCreate(void (*fn)(void *),const char *n,unsigned s,void *a,unsigned p,void *h) { (void)fn;(void)n;(void)s;(void)a;(void)p;(void)h;task_count++;return 1; }
esp_err_t nvs_flash_init(void) { return nvs_available?ESP_OK:ESP_FAIL; }
esp_err_t nvs_open(const char *n,int m,nvs_handle_t *h) { (void)n;(void)m;*h=1;return nvs_available?ESP_OK:ESP_FAIL; }
void nvs_close(nvs_handle_t h) { (void)h;staged=false; }
esp_err_t nvs_get_u8(nvs_handle_t h,const char *k,uint8_t *v) { (void)h;(void)k;if(!has_active)return ESP_FAIL;*v=active_slot;return ESP_OK; }
esp_err_t nvs_set_u8(nvs_handle_t h,const char *k,uint8_t v) { (void)h;(void)k;if(fail())return ESP_FAIL;staged=true;staged_slot=v;return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h,const char *k,void *d,size_t *n) { (void)h;if(!strcmp(k,"record")){if(!profile_size || *n<profile_size)return ESP_FAIL;memcpy(d,profile_record,profile_size);*n=profile_size;return ESP_OK;}unsigned i=k[4]-'0';assert(i<2);if(fail() || !lengths[i])return ESP_FAIL;if(d) {if(*n<lengths[i])return ESP_FAIL;memcpy(d,slots[i],lengths[i]);} *n=lengths[i];return ESP_OK; }
esp_err_t nvs_set_blob(nvs_handle_t h,const char *k,const void *d,size_t n) { (void)h;assert(!http_live);if(fail())return ESP_FAIL;if(!strcmp(k,"record")){assert(n<=sizeof(profile_record));memcpy(profile_record,d,n);profile_size=n;return ESP_OK;}unsigned i=k[4]-'0';assert(i<2);assert(n<=CAST_MAX_BODY);memcpy(slots[i],d,n);lengths[i]=n;return ESP_OK; }
esp_err_t nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *n) {(void)h;unsigned i=key[6]-'0';assert(i<2);size_t len=strlen(sources[i])+1;if(*n<len)return ESP_FAIL;memcpy(out,sources[i],len);*n=len;return ESP_OK;}
esp_err_t nvs_set_str(nvs_handle_t h,const char *key,const char *in) {(void)h;unsigned i=key[6]-'0';assert(i<2);strcpy(sources[i],in);return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) { (void)h;if(fail())return ESP_FAIL;if(staged){active_slot=staged_slot;has_active=true;staged=false;}return ESP_OK; }
static esp_netif_t net;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *k) { (void)k;return online?&net:NULL; }
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *ip) { (void)n;ip->ip.addr=online;return ESP_OK; }
esp_err_t esp_netif_init(void) { radio_count++;return ESP_OK; }
esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }
esp_netif_t *esp_netif_create_default_wifi_sta(void) { return &net; }
esp_err_t esp_wifi_init(const wifi_init_config_t *c) { (void)c;radio_count++;return ESP_OK; }
esp_err_t esp_wifi_get_config(int i,wifi_config_t *c) { (void)i;memset(c,0,sizeof(*c));return ESP_OK; }
esp_err_t esp_wifi_set_mode(int m) { (void)m;return ESP_OK; }
esp_err_t esp_wifi_start(void) { return ESP_OK; }
esp_err_t esp_wifi_connect(void) { return ESP_OK; }
esp_err_t esp_crt_bundle_attach(void *c) { (void)c;return ESP_OK; }
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c) { assert(c->crt_bundle_attach==esp_crt_bundle_attach);assert(c->disable_auto_redirect);http_cursor=0;return &net; }
esp_err_t esp_http_client_open(esp_http_client_handle_t h,int n) { (void)h;(void)n;assert(!http_live);http_live=true;return ESP_OK; }
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) { (void)h;return strlen(http_body); }
int esp_http_client_get_status_code(esp_http_client_handle_t h) { (void)h;return http_code; }
int esp_http_client_read(esp_http_client_handle_t h,char *b,int n) { (void)h;size_t left=strlen(http_body)-http_cursor;size_t got=left<(size_t)n?left:(size_t)n;memcpy(b,http_body+http_cursor,got);http_cursor+=got;return (int)got; }
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) { (void)h;return complete; }
esp_err_t esp_http_client_close(esp_http_client_handle_t h) { (void)h;assert(http_live);http_live=false;return ESP_OK; }
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h) { (void)h;assert(!http_live);return ESP_OK; }

int main(void) {
    const char *old="{\"meta\":{\"date\":\"2000-01-01\"},\"schema_version\":1,\"date\":\"2026-10-03\",\"revision\":1,\"venues\":[]}";
    const char *next="{\"schema_version\":1,\"date\":\"2026-10-03\",\"revision\":2,\"venues\":[]}";
    cast_snapshot_t snapshot;
    assert(cast_protocol_parse_cached(old,strlen(old),&snapshot));
    assert(!strcmp(snapshot.date,"2026-10-03"));
    nvs_ok=true;
#ifdef CONFIG_CAST_WIFI_PORTAL
    strcpy(fetch_origin,server_origin);
#endif
    assert(save(old,strlen(old),&snapshot));
    load();assert(cached && cached->revision==1);
    assert(cast_protocol_parse_cached(next,strlen(next),&snapshot));
    uint8_t previous=active_slot;
    for(unsigned i=1;i<=5;i++) {
        fault=i;operation=0;
        assert(!save(next,strlen(next),&snapshot));
        assert(active_slot==previous);
        fault=0;free(cached);cached=NULL;load();assert(cached && cached->revision==1);
    }
    http_body=next;
    if(CONFIG_CAST_BASE_URL[0]) {
        result_t r;
        online=false;r=fetch("2026-10-03");assert(r.status==CAST_OFFLINE && !r.snapshot);
        online=true;http_code=404;r=fetch("2026-10-03");assert(r.status==CAST_UNPUBLISHED && !r.snapshot);
        http_code=200;complete=false;r=fetch("2026-10-03");assert(r.status==CAST_INVALID && !r.snapshot);
        complete=true;fault=1;operation=0;r=fetch("2026-10-03");assert(r.status==CAST_SAVE_FAILED && !r.snapshot);
        fault=0;assert(cached->revision==1);
        r=fetch("2026-10-03");assert(r.status==CAST_LATEST && r.snapshot && cached->revision==2);free(r.snapshot);
        http_body=old;r=fetch("2026-10-03");assert(r.status==CAST_INVALID && !r.snapshot && cached->revision==2);
    }
    free(cached);cached=NULL;
    assert(cast_sync_init());
    assert(!cast_sync_request("2026-02-29"));
    assert(cast_sync_request("2026-10-03"));
    if(!CONFIG_CAST_BASE_URL[0]) {
        assert(!task_count && !radio_count && !requests);
        cast_sync_status_t status;cast_snapshot_t *received=NULL;
        assert(cast_sync_poll(&status,&received));assert(status==CAST_NOT_CONFIGURED && !received);
        assert(!cast_sync_poll(&status,&received));
    }
    free(cached);cached=NULL;
    if(requests){vQueueDelete(requests);requests=NULL;}vQueueDelete(results);results=NULL;
    nvs_available=false;
#ifdef CONFIG_CAST_WIFI_PORTAL
    assert(!cast_sync_init());
#else
    assert(cast_sync_init());
#endif
    assert(!cached);
    if(requests)vQueueDelete(requests);vQueueDelete(results);
#ifdef CONFIG_CAST_WIFI_PORTAL
    requests=NULL;results=NULL;nvs_available=true;nvs_ok=true;http_code=200;complete=true;fault=0;
    vQueueDelete(profile_requests);vQueueDelete(profile_results);profile_requests=NULL;profile_results=NULL;
    clock_ready=false;assert(fetch("2026-10-03").status==CAST_CLOCK_WAIT);clock_ready=true;
    server_origin="";assert(fetch("2026-10-03").status==CAST_NOT_CONFIGURED);server_origin="https://example.invalid";
    cast_profile_key_t key={.date="2026-10-03",.revision=1,.ids={"a","b"}};
    http_body="{\"schema_version\":1,\"date\":\"2026-10-03\",\"revision\":1,\"members\":[\"a\",\"b\"],\"pages\":[{\"title\":\"资料说明\",\"text\":\"演员资料\"}]}";
    cast_profile_online_t *p;
    online=false;assert(!fetch_profile(&key));online=true;
    complete=false;assert(!fetch_profile(&key));complete=true;
    fault=1;operation=0;assert(!fetch_profile(&key));fault=0;
    p=fetch_profile(&key);assert(p && p->count==1);free(p);
    online=false;p=fetch_profile(&key);assert(p && p->count==1);free(p);
    key.revision=2;assert(!fetch_profile(&key));key.revision=1;
    server_origin="https://another.invalid";assert(!fetch_profile(&key));server_origin="https://example.invalid";
    online=true;http_body=old;cached=malloc(sizeof(*cached));assert(cast_protocol_parse_cached(next,strlen(next),cached));
    strcpy(sources[active_slot],server_origin);assert(fetch("2026-10-03").status==CAST_INVALID);
    server_origin="https://another.invalid";result_t switched=fetch("2026-10-03");assert(switched.status==CAST_LATEST && switched.snapshot->revision==1);free(switched.snapshot);free(cached);cached=NULL;
    strcpy(fetch_origin,server_origin);
    result_t delivered=accept_received(next,strlen(next),"2026-10-03",99);assert(delivered.status==CAST_INVALID && !cached);
    delivered=accept_received(next,strlen(next),"2026-10-04",2);assert(delivered.status==CAST_INVALID && !cached);
    fault=1;operation=0;delivered=accept_received(next,strlen(next),"2026-10-03",2);assert(delivered.status==CAST_SAVE_FAILED && !cached);fault=0;
    delivered=accept_received(next,strlen(next),"2026-10-03",2);assert(delivered.status==CAST_LATEST && cached->revision==2);free(delivered.snapshot);
    delivered=accept_received(next,strlen(next),"2026-10-03",2);assert(delivered.status==CAST_LATEST);free(delivered.snapshot);
    strcpy(fetch_origin,"https://previous.invalid");delivered=accept_received(next,strlen(next),"2026-10-03",2);assert(delivered.status==CAST_INVALID && cached->revision==2);
    free(cached);cached=NULL;
    puts("Website delivery acceptance: PASS (expected date/revision, durable cache fault, duplicate delivery, source change)");
    puts("Runtime Wi-Fi sync: PASS (TLS clock, no service, profile HTTP/cache/failures, revision/IDs/source isolation, service switch)");
#endif
    puts("Cast cache fault injection / offline fallback / sync states: PASS");
    return 0;
}
