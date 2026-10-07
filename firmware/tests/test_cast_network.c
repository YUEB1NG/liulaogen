/* Actual network worker, with radio/time/NVS/RTOS boundaries simulated. */
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
static struct tm *test_gmtime_r(const time_t *t,struct tm *out) {struct tm *p=gmtime(t);if(!p)return NULL;*out=*p;return out;}
#define gmtime_r test_gmtime_r
#include "../main/cast_network.c"
const char network_wifi_event[]="wifi",network_ip_event[]="ip";

typedef struct {int command;int seconds;int link;bool fail;} step_t;
static const step_t *plan;static size_t count,position;
static jmp_buf stop;
static int64_t clock_us;
static unsigned bits,connect_calls,ap_starts,commits,random_calls,runs;
static bool fail_commit,corrupt_phone;
static cast_net_config_t disk[2],pending[2];
static wifi_config_t station;
static char associated[33];
static void reset_runtime(void) {
    memset(&saved,0,sizeof(saved));memset(&phone,0,sizeof(phone));memset(&view,0,sizeof(view));
    memset(&station,0,sizeof(station));memset(associated,0,sizeof(associated));
    wifi_ready=sntp_started=network_ready=apply_pending=false;http=NULL;
    bits=connect_calls=ap_starts=commits=random_calls=0;clock_us=0;fail_commit=corrupt_phone=false;
    commands=mutex=events=(void *)1;
}
static void run(const step_t *steps,size_t n) {
    plan=steps;count=n;position=0;runs++;
    if(setjmp(stop)==0)worker(NULL);
    assert(position==count && ap_starts==0); /* Direct phone route never starts SoftAP. */
}
#define RUN(...) do {const step_t steps[]={__VA_ARGS__};run(steps,sizeof(steps)/sizeof(*steps));} while(0)
static cast_net_config_t original(void) {
    return (cast_net_config_t){.version=1,.ssid="old-network",.password="old-pass-123",.origin="https://example.invalid"};
}
static void fresh(bool old) {
    reset_runtime();memset(disk,0,sizeof(disk));memset(pending,0,sizeof(pending));
    if(old)saved=disk[0]=original();
}
int main(void) {
    fresh(false);network_ready=true;char origin[161]="stale";
    assert(!cast_network_origin(origin,sizeof(origin)) && !origin[0]);
    saved=original();assert(cast_network_origin(origin,sizeof(origin)) && !strcmp(origin,saved.origin));
    assert(!cast_network_origin(origin,2) && !origin[0]);
    /* Preparation creates a stable preset but does not replace the live network. */
    fresh(true);RUN({PHONE_OPEN,0,0,false});
    assert(view.phone && !view.portal && view.state==NET_SETUP);
    assert(!strcmp(saved.ssid,"old-network") && !strcmp(disk[0].ssid,"old-network"));
    assert(!strcmp(view.ssid,disk[1].ssid) && strlen(view.password)==16);
    cast_net_config_t preset=phone;
    reset_runtime();saved=disk[0];RUN({PHONE_OPEN,0,0,false});
    assert(!memcmp(&phone,&preset,sizeof(phone)) && random_calls==0);

    fresh(true);RUN({PHONE_OPEN,0,0,false},{PHONE_CONNECT,0,0,false},{-1,1,1,false});
    assert(view.state==NET_SAVED && !strcmp(saved.ssid,phone.ssid));
    assert(!strcmp(saved.origin,"https://example.invalid") && !memcmp(&saved,&disk[0],sizeof(saved)));
    assert(commits==2); /* Preset, then active config after matching association + DHCP. */

    fresh(false);RUN({PHONE_OPEN,0,0,false},{PHONE_CONNECT,0,0,false},{-1,1,1,false});
    assert(view.state==NET_SAVED && !saved.origin[0] && cast_net_config_valid(&disk[0]));
    reset_runtime();saved=disk[0];RUN({-1,1,1,false});
    assert(wifi_ready && connect_calls==1); /* Boot reconnect uses the saved phone. */

    fresh(true);RUN({PHONE_OPEN,0,0,false},{PHONE_CONNECT,0,0,false},{-1,31,0,false});
    assert(view.state==NET_FAILED && !strcmp(saved.ssid,"old-network"));
    assert(!strcmp((char *)station.sta.ssid,"old-network") && commits==1);

    fresh(true);RUN({PHONE_OPEN,0,0,false},{PHONE_CONNECT,0,0,false},{-1,1,2,false},{-1,31,0,false});
    assert(view.state==NET_FAILED && !strcmp(disk[0].ssid,"old-network")); /* Stale/wrong IP cannot save. */

    fresh(true);RUN({PHONE_OPEN,0,0,false},{PHONE_CONNECT,0,0,false},{-1,1,1,true});
    assert(view.state==NET_STORAGE_FAILED && !strcmp(saved.ssid,"old-network"));
    assert(!strcmp(disk[0].ssid,"old-network") && !strcmp((char *)station.sta.ssid,"old-network"));

    fresh(true);RUN({PHONE_OPEN,0,0,true});
    assert(!view.phone && view.state==NET_STORAGE_FAILED && !phone.ssid[0]);
    assert(!strcmp(saved.ssid,"old-network") && !disk[1].ssid[0]);

    fresh(true);corrupt_phone=true;RUN({PHONE_OPEN,0,0,false});
    assert(!view.phone && view.state==NET_STORAGE_FAILED && random_calls==0);

    fresh(true);RUN({PHONE_OPEN,0,0,false},{PHONE_CONNECT,0,0,false},{CLOSE,0,0,false});
    assert(!view.phone && !view.password[0] && !strcmp(saved.ssid,"old-network"));
    assert(!strcmp((char *)station.sta.ssid,"old-network") && commits==1);

    fresh(false);RUN({PHONE_OPEN,0,0,false},{CLOSE,0,0,false});
    assert(!view.phone && !wifi_ready && !saved.ssid[0]);

    fresh(true);RUN({PHONE_OPEN,0,0,false},{CLEAR,0,0,false});
    assert(!saved.ssid[0] && !phone.ssid[0] && !disk[0].ssid[0] && !disk[1].ssid[0]);
    assert(!view.phone && !wifi_ready);

    fresh(false);RUN({PHONE_OPEN,0,0,false},{PHONE_CONNECT,0,0,false},{-1,1,1,false},{CLOSE,0,0,false},{-1,16,-1,false});
    assert(connect_calls>=3 && disk[0].ssid[0]);
    fresh(true);RUN({SCAN,0,0,false});assert(view.count==2 && view.scan_generation==1 && !view.scanning && !view.scan_failed && view.access[0].secure && !view.access[1].secure);
    fresh(true);RUN({DEVICE_APPLY,0,0,false},{-1,1,1,false});assert(view.state==NET_SAVED && !strcmp(disk[0].ssid,"entered-wifi") && !strcmp(disk[0].origin,"https://example.invalid"));
    fresh(true);RUN({DEVICE_APPLY,0,0,false},{-1,31,0,false});assert(view.state==NET_FAILED && !strcmp(saved.ssid,"old-network"));
    fresh(true);RUN({DEVICE_APPLY,0,0,false},{-1,1,1,true});assert(view.state==NET_STORAGE_FAILED && !strcmp(saved.ssid,"old-network"));
    fresh(false);RUN({SERVICE,0,0,false});assert(view.state==NET_SAVED && !saved.ssid[0] && !strcmp(saved.origin,"https://new.invalid") && cast_net_config_valid(&disk[0]));
    fresh(true);RUN({SERVICE,0,0,true});assert(view.state==NET_STORAGE_FAILED && !strcmp(saved.origin,"https://example.invalid"));
    printf("Network worker: PASS (%u runs: scanned/deduplicated APs, entered credentials, source before Wi-Fi, DHCP identity, timeout, NVS failure, cancel, clear, reconnect and legacy regression; radio mocked)\n",runs);
}

esp_err_t esp_wifi_scan_start(const void *c,bool wait) {(void)c;assert(wait);return ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *n,wifi_ap_record_t *a) {assert(*n>=3);*n=3;a[0]=(wifi_ap_record_t){.ssid="test-wifi",.rssi=-50,.authmode=WIFI_AUTH_WPA2_PSK};a[1]=a[0];a[2]=(wifi_ap_record_t){.ssid="open-wifi",.rssi=-70};return ESP_OK;}
esp_err_t esp_wifi_clear_ap_list(void) {return ESP_OK;}
int xQueueReceive(QueueHandle_t q,void *out,unsigned ticks) {
    (void)q;assert(ticks==250);
    if(position==count)longjmp(stop,1);
    step_t s=plan[position++];clock_us+=(int64_t)s.seconds*1000000+250000;fail_commit=s.fail;
    if(s.link>0) {bits|=HAS_IP;snprintf(associated,sizeof(associated),"%s",s.link==2?"wrong-network":(const char *)station.sta.ssid);}
    if(s.link<0)bits=0;
    if(s.command<0)return 0;
    command_t c={.kind=(command_kind_t)s.command};
    if(s.command==DEVICE_APPLY)c.config=(cast_net_config_t){.version=1,.ssid="entered-wifi",.password="entered-pass"};
    if(s.command==SERVICE)strcpy(c.config.origin,"https://new.invalid");
    *(command_t *)out=c;return pdTRUE;
}
QueueHandle_t xQueueCreate(unsigned c,unsigned s) {(void)c;(void)s;return (void *)1;}
int xQueueSend(QueueHandle_t q,const void *d,unsigned t) {(void)q;(void)d;(void)t;return pdTRUE;}
void vQueueDelete(QueueHandle_t q) {(void)q;}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {return (void *)1;}
int xSemaphoreTake(SemaphoreHandle_t h,unsigned t) {(void)h;(void)t;return 1;}
int xSemaphoreGive(SemaphoreHandle_t h) {(void)h;return 1;}
void vSemaphoreDelete(SemaphoreHandle_t h) {(void)h;}
EventGroupHandle_t xEventGroupCreate(void) {return (void *)1;}
unsigned xEventGroupGetBits(EventGroupHandle_t h) {(void)h;return bits;}
unsigned xEventGroupSetBits(EventGroupHandle_t h,unsigned b) {(void)h;return bits|=b;}
unsigned xEventGroupClearBits(EventGroupHandle_t h,unsigned b) {(void)h;unsigned old=bits;bits&=~b;return old;}
void vEventGroupDelete(EventGroupHandle_t h) {(void)h;}
int xTaskCreate(void (*f)(void *),const char *n,unsigned s,void *a,unsigned p,void *h) {(void)f;(void)n;(void)s;(void)a;(void)p;(void)h;return pdPASS;}
unsigned uxTaskGetStackHighWaterMark(void *h) {(void)h;return 2048;}
int64_t esp_timer_get_time(void) {return clock_us;}
uint32_t esp_random(void) {return ++random_calls*0x1234567;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h) {(void)mode;assert(!strcmp(name,"cast_network"));memcpy(pending,disk,sizeof(disk));*h=1;return ESP_OK;}
void nvs_close(nvs_handle_t h) {(void)h;}
static unsigned slot(const char *key) {assert(!strcmp(key,"phone") || !strcmp(key,"config"));return !strcmp(key,"phone");}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *data,size_t *n) {
    (void)h;unsigned i=slot(key);if(i && corrupt_phone)return ESP_FAIL;
    if(!disk[i].ssid[0])return ESP_ERR_NVS_NOT_FOUND;
    assert(*n>=sizeof(disk[i]));*n=sizeof(disk[i]);memcpy(data,&disk[i],*n);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n) {(void)h;assert(n==sizeof(cast_net_config_t));memcpy(&pending[slot(key)],data,n);return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char *key) {(void)h;memset(&pending[slot(key)],0,sizeof(pending[0]));return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {(void)h;if(fail_commit)return ESP_FAIL;memcpy(disk,pending,sizeof(disk));commits++;return ESP_OK;}
esp_err_t esp_wifi_init(const wifi_init_config_t *c) {(void)c;return ESP_OK;}
esp_err_t esp_wifi_set_storage(int s) {(void)s;return ESP_OK;}
esp_err_t esp_wifi_set_mode(int m) {if(m==WIFI_MODE_APSTA)ap_starts++;return ESP_OK;}
esp_err_t esp_wifi_start(void) {return ESP_OK;}
esp_err_t esp_wifi_stop(void) {return ESP_OK;}
esp_err_t esp_wifi_deinit(void) {return ESP_OK;}
esp_err_t esp_wifi_connect(void) {connect_calls++;return ESP_OK;}
esp_err_t esp_wifi_disconnect(void) {bits=0;return ESP_OK;}
esp_err_t esp_wifi_set_config(int i,const wifi_config_t *c) {if(i==WIFI_IF_STA)station=*c;return ESP_OK;}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *a) {memcpy(a->ssid,associated,33);return bits?ESP_OK:ESP_FAIL;}
esp_err_t esp_netif_init(void) {return ESP_OK;}
esp_err_t esp_event_loop_create_default(void) {return ESP_OK;}
esp_netif_t *esp_netif_create_default_wifi_sta(void) {return (esp_netif_t *)1;}
esp_netif_t *esp_netif_create_default_wifi_ap(void) {return (esp_netif_t *)2;}
void esp_netif_destroy_default_wifi(esp_netif_t *n) {(void)n;}
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *i) {(void)n;(void)i;return ESP_FAIL;}
esp_err_t esp_event_handler_register(esp_event_base_t b,int id,void (*f)(void *,esp_event_base_t,int32_t,void *),void *a) {(void)b;(void)id;(void)f;(void)a;return ESP_OK;}
esp_err_t esp_event_handler_unregister(esp_event_base_t b,int id,void (*f)(void *,esp_event_base_t,int32_t,void *)) {(void)b;(void)id;(void)f;return ESP_OK;}
int getsockname(int fd,struct sockaddr *a,socklen_t *s) {(void)fd;(void)a;(void)s;return -1;}
esp_err_t httpd_start(httpd_handle_t *h,const httpd_config_t *c) {(void)h;(void)c;assert(!"Phone setup must not start HTTP");return ESP_FAIL;}
esp_err_t httpd_stop(httpd_handle_t h) {(void)h;return ESP_OK;}
esp_err_t httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t *u) {(void)h;(void)u;return ESP_OK;}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *s) {(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *s) {(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *k,const char *v) {(void)r;(void)k;(void)v;return ESP_OK;}
esp_err_t httpd_resp_send(httpd_req_t *r,const char *b,int n) {(void)r;(void)b;(void)n;return ESP_OK;}
esp_err_t httpd_resp_send_chunk(httpd_req_t *r,const char *b,size_t n) {(void)r;(void)b;(void)n;return ESP_OK;}
int httpd_req_to_sockfd(httpd_req_t *r) {(void)r;return 0;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *k,char *v,size_t n) {(void)r;(void)k;(void)v;(void)n;return ESP_FAIL;}
int httpd_req_recv(httpd_req_t *r,char *b,size_t n) {(void)r;(void)b;(void)n;return -1;}
void esp_sntp_setoperatingmode(int m) {(void)m;}
void esp_sntp_setservername(int i,const char *s) {(void)i;(void)s;}
void esp_sntp_init(void) {}
size_t heap_caps_get_free_size(unsigned c) {(void)c;return 40000;}
size_t heap_caps_get_largest_free_block(unsigned c) {(void)c;return 20000;}
void net_test_log(const char *tag,const char *format,...) {(void)tag;(void)format;}
