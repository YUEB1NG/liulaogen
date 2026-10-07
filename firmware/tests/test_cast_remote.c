/* Actual device identity, HTTP lifecycle, polling and protocol; platform boundaries mocked. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/cast_remote.c"
static identity_t disk,staged;
static bool has_disk,fail_save,live,online=true,clock_ok=true,complete=true;
static int status_code=200,requests;
static int64_t now;
static const char *origin="https://example.invalid",*response="{\"paired\":false,\"job\":null}";
static unsigned cursor;
static char sent[224],url_used[320];
SemaphoreHandle_t xSemaphoreCreateMutex(void) {return (void *)1;}
int xSemaphoreTake(SemaphoreHandle_t m,unsigned t) {(void)m;(void)t;return 1;}
int xSemaphoreGive(SemaphoreHandle_t m) {(void)m;return 1;}
int64_t esp_timer_get_time(void) {return now;}
uint32_t esp_random(void) {static uint32_t n=100;return ++n;}
void cast_network_view(cast_net_view_t *v) {memset(v,0,sizeof(*v));v->connected=online;}
bool cast_network_origin(char *out,size_t n) {assert(n>strlen(origin));strcpy(out,origin);return *origin!=0;}
bool cast_network_today(char out[11]) {strcpy(out,"2026-10-07");return clock_ok;}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h) {assert(!strcmp(ns,"cast_device") && mode==NVS_READWRITE);*h=1;return ESP_OK;}
void nvs_close(nvs_handle_t h) {(void)h;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n) {(void)h;assert(!strcmp(key,"identity") && *n==sizeof(disk));if(!has_disk)return ESP_ERR_NVS_NOT_FOUND;memcpy(out,&disk,*n);return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n) {(void)h;assert(!live && !strcmp(key,"identity") && n==sizeof(disk));staged=*(identity_t *)data;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {(void)h;if(fail_save)return ESP_FAIL;disk=staged;has_disk=true;return ESP_OK;}
esp_err_t esp_crt_bundle_attach(void *v) {(void)v;return ESP_OK;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c) {assert(!live && c->disable_auto_redirect && c->timeout_ms==8000 && c->crt_bundle_attach);strcpy(url_used,c->url);requests++;cursor=0;return (void *)1;}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h,const char *k,const char *v) {(void)h;if(!strcmp(k,"Authorization")){assert(strlen(v)==71 && !strncmp(v,"Bearer ",7));}return ESP_OK;}
esp_err_t esp_http_client_open(esp_http_client_handle_t h,int n) {(void)h;(void)n;assert(!live);live=true;return ESP_OK;}
int esp_http_client_write(esp_http_client_handle_t h,const char *s,int n) {(void)h;assert(n<(int)sizeof(sent));memcpy(sent,s,n);sent[n]=0;return n;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) {(void)h;return strlen(response);}
int esp_http_client_get_status_code(esp_http_client_handle_t h) {(void)h;return status_code;}
int esp_http_client_read(esp_http_client_handle_t h,char *out,int max) {(void)h;unsigned n=(unsigned)strlen(response)-cursor;if(n>(unsigned)max)n=(unsigned)max;memcpy(out,response+cursor,n);cursor+=n;return (int)n;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) {(void)h;return complete;}
esp_err_t esp_http_client_close(esp_http_client_handle_t h) {(void)h;assert(live);live=false;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h) {(void)h;assert(!live);return ESP_OK;}
static void reset_memory(void) {memset(&identity,0,sizeof(identity));memset(&view,0,sizeof(view));memset(last_origin,0,sizeof(last_origin));next_poll=pair_until=0;}
int main(int argc,char **argv) {
    cast_remote_job_t job;bool paired;
    if(argc==2) {
        FILE *file=fopen(argv[1],"rb");assert(file);char body[513];unsigned n=(unsigned)fread(body,1,sizeof(body),file);fclose(file);
        assert(cast_remote_parse_reply(body,n,&paired,&job) && paired && job.id[0]);
        puts("Actual website heartbeat accepted by firmware C parser: PASS");return 0;
    }
    const char *bad[]={"{}","{\"paired\":true,\"paired\":false}","{\"paired\":1,\"job\":null}","{\"paired\":true,\"job\":null}x","{\"paired\":true,\"job\":{\"id\":\"bad\",\"date\":\"2026-10-07\",\"revision\":1}}","{\"paired\":true,\"job\":[[[[]]]]}","{\"paired\":true,\"job\\u0000\":null}"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!cast_remote_parse_reply(bad[i],(unsigned)strlen(bad[i]),&paired,&job));
    assert(cast_remote_init());origin="";assert(!cast_remote_poll(&job) && view.state==REMOTE_UNCONFIGURED && !requests);
    origin="https://example.invalid";online=false;assert(!cast_remote_poll(&job) && view.state==REMOTE_OFFLINE && !requests);
    online=true;clock_ok=false;assert(!cast_remote_poll(&job) && view.state==REMOTE_WAIT_CLOCK && !requests);
    clock_ok=true;fail_save=true;cast_remote_pair();assert(!cast_remote_poll(&job) && view.state==REMOTE_ERROR && !requests && !has_disk);
    fail_save=false;cast_remote_pair();assert(!cast_remote_poll(&job) && view.state==REMOTE_PAIRING && strlen(view.code)==8 && has_disk);
    identity_t original=disk;int previous=requests;assert(!cast_remote_poll(&job) && requests==previous);
    now+=10000000;response="{\"paired\":true,\"job\":{\"id\":\"0123456789abcdef0123456789abcdef\",\"date\":\"2026-10-07\",\"revision\":4}}";
    assert(cast_remote_poll(&job) && view.state==REMOTE_PAIRED && job.revision==4 && !view.code[0]);
    char body[32];unsigned n;response="payload";assert(cast_remote_download(&job,body,sizeof(body),&n) && n==7 && !strcmp(body,"payload") && strstr(url_used,"/api/device/content?"));
    complete=false;assert(!cast_remote_download(&job,body,sizeof(body),&n));complete=true;
    assert(!cast_remote_download(&job,body,7,&n));
    cast_remote_ack(&job,true);assert(disk.result==1 && !strcmp(disk.ack,job.id));reset_memory();
    response="{\"paired\":true,\"job\":null}";assert(!cast_remote_poll(&job) && strstr(sent,"\"result\":\"saved\"") && !strcmp(identity.secret,original.secret));
    strcpy(job.id,"11111111111111111111111111111111");fail_save=true;cast_remote_ack(&job,false);assert(view.state==REMOTE_ERROR && !identity.ack[0]);fail_save=false;
    origin="https://other.invalid";assert(!cast_remote_download(&job,body,sizeof(body),&n));assert(!cast_remote_poll(&job) && strcmp(identity.id,original.id) && !identity.ack[0]);
    status_code=302;now+=10000000;assert(!cast_remote_poll(&job) && view.state==REMOTE_ERROR && !live);
    status_code=409;now+=10000000;assert(!cast_remote_poll(&job) && view.state==REMOTE_READY);
    reset_memory();disk.version=99;previous=requests;assert(!cast_remote_poll(&job) && view.state==REMOTE_ERROR && previous==requests);
    puts("Device link: PASS (identity persistence/failure, pairing, heartbeat pacing, TLS clock, redirects, bounded complete HTTP, ACK reboot, source isolation and malformed replies)");
}
