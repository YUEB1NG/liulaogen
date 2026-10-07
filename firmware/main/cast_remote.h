#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { REMOTE_UNCONFIGURED,REMOTE_OFFLINE,REMOTE_WAIT_CLOCK,REMOTE_READY,REMOTE_PAIRING,REMOTE_PAIRED,REMOTE_ERROR } cast_remote_state_t;
typedef struct {cast_remote_state_t state;char code[9];bool transferring;} cast_remote_view_t;
typedef struct {char id[33],date[11];uint32_t revision;} cast_remote_job_t;
bool cast_remote_init(void);
bool cast_remote_pair(void);
void cast_remote_view(cast_remote_view_t *out);
/* Called by the existing sync worker only: serialize HTTP/TLS and cache writes. */
bool cast_remote_poll(cast_remote_job_t *job);
bool cast_remote_download(const cast_remote_job_t *job,char *body,unsigned capacity,unsigned *length);
void cast_remote_ack(const cast_remote_job_t *job,bool saved);
bool cast_remote_parse_reply(const char *body,unsigned length,bool *paired,cast_remote_job_t *job);
