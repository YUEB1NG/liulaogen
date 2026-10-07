#pragma once
#include <stddef.h>
typedef struct { const char *title; const char *text; } cast_page_t;
typedef struct { const char *name; const cast_page_t *pages; size_t page_count; } cast_actor_t;
typedef struct { const char *title; const char *session; size_t city, first, count; } cast_venue_t;
extern const cast_actor_t cast_actors[];
extern const cast_venue_t cast_venues[9];
extern const char *const cast_cities[8];
extern const char cast_date[];
