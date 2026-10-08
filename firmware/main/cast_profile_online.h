#pragma once
#include "cast_protocol.h"
#define CAST_PROFILE_PAGES 24
typedef struct { char date[11];uint32_t revision;char ids[2][37]; } cast_profile_key_t;
typedef struct { char title[25],text[320]; } cast_profile_page_t;
typedef struct { cast_profile_key_t key;size_t count;cast_profile_page_t pages[CAST_PROFILE_PAGES]; } cast_profile_online_t;
bool cast_profile_key_equal(const cast_profile_key_t *a,const cast_profile_key_t *b);
bool cast_profile_parse(const char *body,size_t size,const cast_profile_key_t *key,cast_profile_online_t *out);
