#include "cast_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    cast_input_t s;bool seen[127]={0};char description[384];
    cast_input_begin(&s,"",160);
    for(unsigned group=0;group<4;group++) {
        size_t first=s.cursor;
        do {size_t n=s.length;cast_input_key(&s,CAST_OK);if(s.length>n)seen[(unsigned char)s.value[n]]=true;cast_input_key(&s,CAST_DOWN);}while(s.cursor!=first);
        cast_input_key(&s,CAST_LONG_UP);
    }
    for(unsigned c=32;c<=126;c++)assert(seen[c]);
    cast_input_begin(&s,"secret123",63);cast_input_description(&s,true,description,sizeof(description));
    assert(!strstr(description,"secret123") && strstr(description,"*********"));
    cast_input_key(&s,CAST_UP);cast_input_key(&s,CAST_OK);assert(!strcmp(s.value,"secret12"));
    cast_input_begin(&s,"",8);for(int i=0;i<20;i++)cast_input_key(&s,CAST_OK);assert(s.length==8 && strlen(s.value)==8);
    assert(cast_input_key(&s,CAST_LONG_DOWN)==INPUT_DONE && s.length==8);
    assert(cast_input_key(&s,CAST_BACK)==INPUT_CANCELLED);cast_input_t zero={0};assert(!memcmp(&s,&zero,sizeof(s)));
    puts("On-device input: PASS (all 95 printable ASCII, limit, deletion, masked preview, submit and cancellation wipe)");
}
