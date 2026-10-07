#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NVS_NOT_FOUND 2
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(x) (x)
#define BIT0 1
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *EventGroupHandle_t;
typedef int nvs_handle_t;
typedef const char *esp_event_base_t;
extern const char network_wifi_event[],network_ip_event[];
#define WIFI_EVENT network_wifi_event
#define IP_EVENT network_ip_event
#define ESP_EVENT_ANY_ID -1
#define IP_EVENT_STA_GOT_IP 1
#define WIFI_EVENT_STA_DISCONNECTED 2
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define WIFI_STORAGE_RAM 0
#define WIFI_IF_STA 0
#define WIFI_IF_AP 1
#define WIFI_MODE_STA 1
#define WIFI_MODE_APSTA 2
#define WIFI_AUTH_WPA2_PSK 3
#define WIFI_AUTH_OPEN 0
typedef struct {int unused;} wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() {0}
typedef struct {
 struct {unsigned char ssid[32],password[64];struct {bool capable;} pmf_cfg;struct {int authmode;} threshold;} sta;
 struct {unsigned char ssid[32],password[64];unsigned ssid_len,channel,max_connection;int authmode;} ap;
} wifi_config_t;
typedef struct {unsigned char ssid[33];int8_t rssi;int authmode;} wifi_ap_record_t;
esp_err_t esp_wifi_scan_start(const void *,bool);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *,wifi_ap_record_t *);
esp_err_t esp_wifi_clear_ap_list(void);
typedef struct {int unused;} esp_netif_t;
typedef struct {struct {uint32_t addr;} ip;} esp_netif_ip_info_t;
typedef void *httpd_handle_t;
typedef struct {size_t content_len;} httpd_req_t;
typedef struct {unsigned stack_size,max_open_sockets,backlog_conn,recv_wait_timeout,send_wait_timeout;bool lru_purge_enable;} httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() (httpd_config_t){0}
#define HTTP_GET 0
#define HTTP_POST 1
#define HTTPD_RESP_USE_STRLEN -1
typedef struct {const char *uri;int method;esp_err_t (*handler)(httpd_req_t *);} httpd_uri_t;
typedef unsigned socklen_t;
struct sockaddr {int unused;};
struct sockaddr_in {struct {uint32_t s_addr;} sin_addr;};
int getsockname(int,struct sockaddr *,socklen_t *);
QueueHandle_t xQueueCreate(unsigned,unsigned);
int xQueueSend(QueueHandle_t,const void *,unsigned);
int xQueueReceive(QueueHandle_t,void *,unsigned);
void vQueueDelete(QueueHandle_t);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t,unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
EventGroupHandle_t xEventGroupCreate(void);
unsigned xEventGroupGetBits(EventGroupHandle_t);
unsigned xEventGroupSetBits(EventGroupHandle_t,unsigned);
unsigned xEventGroupClearBits(EventGroupHandle_t,unsigned);
void vEventGroupDelete(EventGroupHandle_t);
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,void *);
unsigned uxTaskGetStackHighWaterMark(void *);
int64_t esp_timer_get_time(void);
uint32_t esp_random(void);
esp_err_t nvs_open(const char *,int,nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_erase_key(nvs_handle_t,const char *);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t esp_wifi_init(const wifi_init_config_t *);
esp_err_t esp_wifi_set_storage(int);
esp_err_t esp_wifi_set_mode(int);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_set_config(int,const wifi_config_t *);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *);
esp_err_t esp_netif_init(void);
esp_err_t esp_event_loop_create_default(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
void esp_netif_destroy_default_wifi(esp_netif_t *);
esp_err_t esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *);
esp_err_t esp_event_handler_register(esp_event_base_t,int,void (*)(void *,esp_event_base_t,int32_t,void *),void *);
esp_err_t esp_event_handler_unregister(esp_event_base_t,int,void (*)(void *,esp_event_base_t,int32_t,void *));
esp_err_t httpd_start(httpd_handle_t *,const httpd_config_t *);
esp_err_t httpd_stop(httpd_handle_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t *);
esp_err_t httpd_resp_set_status(httpd_req_t *,const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *,const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *,const char *,const char *);
esp_err_t httpd_resp_send(httpd_req_t *,const char *,int);
esp_err_t httpd_resp_send_chunk(httpd_req_t *,const char *,size_t);
int httpd_req_to_sockfd(httpd_req_t *);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *,const char *,char *,size_t);
int httpd_req_recv(httpd_req_t *,char *,size_t);
#define SNTP_OPMODE_POLL 0
void esp_sntp_setoperatingmode(int);
void esp_sntp_setservername(int,const char *);
void esp_sntp_init(void);
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
size_t heap_caps_get_free_size(unsigned);
size_t heap_caps_get_largest_free_block(unsigned);
void net_test_log(const char *,const char *,...);
#define ESP_LOGI(...) net_test_log(__VA_ARGS__)
