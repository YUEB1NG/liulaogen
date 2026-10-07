#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t version; char ssid[33], password[64], origin[161]; } cast_net_config_t;
typedef enum { NET_IDLE, NET_CONNECTING, NET_ONLINE, NET_SETUP, NET_FAILED, NET_SAVED, NET_STORAGE_FAILED } cast_net_state_t;
#define CAST_WIFI_RESULTS 12
typedef struct {char ssid[33];int8_t rssi;bool secure;} cast_access_point_t;
typedef struct { cast_net_state_t state; bool portal, connected, phone; char ssid[24], password[17];
    uint32_t scan_generation,state_generation;bool scanning,scan_failed;size_t count;cast_access_point_t access[CAST_WIFI_RESULTS];
} cast_net_view_t;
/* Pure transport/config validation, also compiled by host tests. */
bool cast_net_config_valid(const cast_net_config_t *config);
bool cast_net_origin_valid(const char *origin);
bool cast_network_init(void);
bool cast_network_setup(void);
bool cast_network_phone_connect(void);
bool cast_network_web_setup(void);
bool cast_network_connect(const char *ssid,const char *password);
bool cast_network_set_origin(const char *origin);
bool cast_network_cancel(void);
bool cast_network_forget(void);
void cast_network_view(cast_net_view_t *view);
bool cast_network_origin(char *out, size_t size);
bool cast_network_today(char out[11]);
