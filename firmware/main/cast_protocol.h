#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CAST_MAX_BODY 8192
#define CAST_MAX_SESSIONS 16
#define CAST_MAX_GROUPS 80
#define CAST_MAX_GROUPS_PER_SESSION 12
#define CAST_NAME_BYTES 25
#define CAST_LABEL_BYTES 25
/* Transport-independent, bounded schema v1 model. Never contains credentials. */
/* The editor creates canonical 36-character UUIDs for new actors. */
typedef struct { char id[37], name[CAST_NAME_BYTES]; } cast_member_t;
typedef struct { cast_member_t members[2]; } cast_group_t;
typedef struct {
    char id[33], label[CAST_LABEL_BYTES];
    uint8_t city; uint16_t first, count;
} cast_session_t;
typedef struct {
    char date[11]; uint32_t revision;
    uint16_t session_count, group_count;
    cast_session_t sessions[CAST_MAX_SESSIONS];
    cast_group_t groups[CAST_MAX_GROUPS];
} cast_snapshot_t;
typedef enum { CAST_OFFLINE, CAST_UNPUBLISHED, CAST_LATEST, CAST_INVALID, CAST_SAVE_FAILED, CAST_BUSY, CAST_NOT_CONFIGURED, CAST_CLOCK_WAIT } cast_sync_status_t;
bool cast_valid_date(const char *date);
bool cast_text_supported(const char *text);
void cast_date_step(char date[11], int delta);
/* Output is unchanged on failure. Date must exactly match the explicit request. */
bool cast_protocol_parse(const char *json, size_t len, const char *date, cast_snapshot_t *out);
/* Cache reload validates the root date, without requiring a wall clock. */
bool cast_protocol_parse_cached(const char *json, size_t len, cast_snapshot_t *out);
bool cast_revision_accept(const cast_snapshot_t *old, const cast_snapshot_t *next);
