#pragma once
#include "cast_data.h"
#include "cast_protocol.h"
#include "cast_profile_online.h"
typedef enum { CAST_CITY, CAST_SESSION, CAST_ACTOR, CAST_READER, CAST_SYNC } cast_level_t;
typedef enum { CAST_UP, CAST_DOWN, CAST_OK, CAST_BACK, CAST_LONG_UP, CAST_LONG_DOWN } cast_key_t;
typedef struct { cast_level_t level; size_t city, venue, actor, page; } cast_state_t;
void cast_navigate(cast_state_t *s, cast_key_t key);
size_t cast_session_count(size_t city);
size_t cast_session_index(size_t city, size_t ordinal);
void cast_use_snapshot(const cast_snapshot_t *snapshot);
size_t cast_group_count(size_t venue);
const char *cast_session_label(size_t venue);
const char *cast_group_name(size_t venue,size_t group);
const cast_actor_t *cast_profile(size_t venue,size_t group);
const char *cast_display_date(void);
bool cast_current_profile_key(size_t venue,size_t group,cast_profile_key_t *out);
void cast_use_profile(const cast_profile_online_t *profile);
