#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "cast_state.h"
typedef struct {char value[161];size_t length,limit,cursor;unsigned group;} cast_input_t;
typedef enum { INPUT_EDITING,INPUT_DONE,INPUT_CANCELLED } cast_input_result_t;
void cast_input_begin(cast_input_t *s,const char *initial,size_t limit);
cast_input_result_t cast_input_key(cast_input_t *s,cast_key_t key);
void cast_input_description(const cast_input_t *s,bool password,char *out,size_t size);
