#pragma once
#include "cast_profile_online.h"
/* Single worker owns this module. Immutable files are activated by one durable
 * NVS record only after the entire window has been verified. No UI thread I/O. */
typedef bool (*cast_archive_get_t)(const char *path,char *body,size_t *size);
typedef bool (*cast_archive_hash_t)(const void *body,size_t size,char hex[65]);
bool cast_archive_init(cast_archive_hash_t hash);
bool cast_archive_update(const char *origin,const char *date,uint32_t revision,cast_archive_get_t get);
bool cast_archive_lineup(const char *origin,const char *date,cast_snapshot_t *out);
bool cast_archive_profile(const char *origin,const cast_profile_key_t *key,cast_profile_online_t *out);
/* Useful for first installation and offline date selection; does not modify NVS. */
bool cast_archive_today(const char *origin,char date[11]);
