#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
typedef struct queue *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned count,unsigned size);
int xQueueSend(QueueHandle_t q,const void *data,unsigned ticks);
int xQueueReceive(QueueHandle_t q,void *data,unsigned ticks);
void vQueueDelete(QueueHandle_t q);
int xTaskCreate(void (*worker)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle);
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
esp_err_t nvs_flash_init(void);
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h);
void nvs_close(nvs_handle_t h);
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *value);
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value);
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *data,size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size);
esp_err_t nvs_commit(nvs_handle_t h);
esp_err_t nvs_get_str(nvs_handle_t h,const char *key,char *data,size_t *size);
esp_err_t nvs_set_str(nvs_handle_t h,const char *key,const char *data);
typedef struct { int unused; } esp_netif_t;
typedef struct { struct { unsigned addr; } ip; } esp_netif_ip_info_t;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key);
esp_err_t esp_netif_get_ip_info(esp_netif_t *net,esp_netif_ip_info_t *ip);
esp_err_t esp_netif_init(void);
esp_err_t esp_event_loop_create_default(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
typedef struct {int unused;} wifi_init_config_t;
typedef struct { struct { char ssid[33]; } sta; } wifi_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() {0}
#define WIFI_IF_STA 0
#define WIFI_MODE_STA 1
esp_err_t esp_wifi_init(const wifi_init_config_t *c);
esp_err_t esp_wifi_get_config(int interface,wifi_config_t *c);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_connect(void);
typedef void *esp_http_client_handle_t;
esp_err_t esp_crt_bundle_attach(void *c);
typedef struct {
    const char *url; int timeout_ms; bool disable_auto_redirect;
    esp_err_t (*crt_bundle_attach)(void *); int buffer_size;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c);
esp_err_t esp_http_client_open(esp_http_client_handle_t h,int length);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h);
int esp_http_client_get_status_code(esp_http_client_handle_t h);
int esp_http_client_read(esp_http_client_handle_t h,char *data,int size);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h);
esp_err_t esp_http_client_close(esp_http_client_handle_t h);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h);
