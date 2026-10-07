#pragma once
#include "cast_protocol.h"
#include "cast_profile_online.h"
bool cast_sync_profile_request(const cast_profile_key_t *key);
bool cast_sync_profile_poll(cast_profile_online_t **out);
/* Start once. Runtime Wi-Fi setup is selected by CONFIG_CAST_WIFI_PORTAL.
 * Never erases the NVS partition on initialization failure. */
bool cast_sync_init(void);
bool cast_sync_request(const char *date);
/* Ownership of *snapshot passes to caller. Worker never accesses LVGL. */
bool cast_sync_poll(cast_sync_status_t *status,cast_snapshot_t **snapshot);
const cast_snapshot_t *cast_sync_cached(void);
