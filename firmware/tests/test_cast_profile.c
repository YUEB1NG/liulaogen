#include "cast_profile_online.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
int main(int argc,char **argv) {
    assert(argc==2 || argc==4);
    cast_profile_key_t key={.date="2026-10-03",.revision=1,.ids={"a","b"}};
    if(argc==4) {assert(strlen(argv[2])<37 && strlen(argv[3])<37);strcpy(key.ids[0],argv[2]);strcpy(key.ids[1],argv[3]);}
    FILE *f=fopen(argv[1],"rb");assert(f);char raw[CAST_MAX_BODY+1];size_t n=fread(raw,1,sizeof(raw),f);fclose(f);
    cast_profile_online_t *out=malloc(sizeof(*out)),*before=malloc(sizeof(*before));assert(out && before);
    memset(out,0x42,sizeof(*out));*before=*out;
    bool valid=cast_profile_parse(raw,n,&key,out);
    if(!valid)assert(!memcmp(out,before,sizeof(*out)));
    else {assert(out->count>0 && out->count<=24);assert(cast_profile_key_equal(&key,&out->key));key.revision++;assert(!cast_profile_key_equal(&key,&out->key));}
    free(out);free(before);return valid?0:2;
}
