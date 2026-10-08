#include "cast_profile_online.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
bool cast_profile_key_equal(const cast_profile_key_t *a,const cast_profile_key_t *b) {
    return a->revision==b->revision && !strcmp(a->date,b->date) && !strcmp(a->ids[0],b->ids[0]) && !strcmp(a->ids[1],b->ids[1]);
}
static bool unique(cJSON *o) {
    if(cJSON_IsObject(o)) for(cJSON *a=o->child;a;a=a->next) for(cJSON *b=a->next;b;b=b->next) if(!strcmp(a->string,b->string))return false;
    for(cJSON *a=o->child;a;a=a->next) if(!unique(a)) return false;
    return true;
}
static bool string(cJSON *v,char *out,size_t capacity,bool paragraph) {
    if(!cJSON_IsString(v) || !*v->valuestring || strlen(v->valuestring)>=capacity) return false;
    char copy[320];strcpy(copy,v->valuestring);unsigned columns=0,lines=1;
    for(char *p=copy;*p;p++) {
        if(*p=='\n' && paragraph) {if(++lines>7)return false;columns=0;*p=' ';}
        else {if((unsigned char)*p<32 || *p==127)return false;if(((unsigned char)*p&0xc0)!=0x80 && ++columns>(paragraph?13u:8u))return false;}
    }
    if(!cast_text_supported(copy))return false;
    strcpy(out,v->valuestring);return true;
}
bool cast_profile_parse(const char *body,size_t size,const cast_profile_key_t *key,cast_profile_online_t *out) {
    if(!body || !key || !out || !size || size>CAST_MAX_BODY || memchr(body,0,size) || !cast_valid_date(key->date))return false;
    int depth=0;bool quoted=false,escape=false;
    for(size_t i=0;i<size;i++) {char c=body[i];if(quoted){if(escape){escape=false;if(c=='u' && i+4<size && !memcmp(body+i+1,"0000",4))return false;}else if(c=='\\')escape=true;else if(c=='"')quoted=false;}else if(c=='"')quoted=true;else if(c=='{' || c=='['){if(++depth>6)return false;}else if(c=='}' || c==']'){if(--depth<0)return false;}}
    if(depth || quoted)return false;
    const char *end; cJSON *root=cJSON_ParseWithLengthOpts(body,size,&end,false);
    cast_profile_online_t *next=calloc(1,sizeof(*next));bool ok=false;
    if(!root || !next || !cJSON_IsObject(root) || !unique(root))goto done;
    while(end<body+size && isspace((unsigned char)*end))end++;
    if(end!=body+size)goto done;
    cJSON *version=cJSON_GetObjectItemCaseSensitive(root,"schema_version"),*date=cJSON_GetObjectItemCaseSensitive(root,"date"),*revision=cJSON_GetObjectItemCaseSensitive(root,"revision"),*members=cJSON_GetObjectItemCaseSensitive(root,"members"),*pages=cJSON_GetObjectItemCaseSensitive(root,"pages");
    if(!cJSON_IsNumber(version) || version->valuedouble!=1 || !cJSON_IsString(date) || strcmp(date->valuestring,key->date) ||
       !cJSON_IsNumber(revision) || revision->valuedouble!=key->revision || !cJSON_IsArray(members) || cJSON_GetArraySize(members)!=2 || !cJSON_IsArray(pages))goto done;
    for(unsigned i=0;i<2;i++) {cJSON *v=cJSON_GetArrayItem(members,i);if(!cJSON_IsString(v) || strcmp(v->valuestring,key->ids[i]))goto done;}
    int count=cJSON_GetArraySize(pages);if(count<1 || count>CAST_PROFILE_PAGES)goto done;
    for(int i=0;i<count;i++) {cJSON *p=cJSON_GetArrayItem(pages,i);if(!cJSON_IsObject(p) || !string(cJSON_GetObjectItemCaseSensitive(p,"title"),next->pages[i].title,25,false) || !string(cJSON_GetObjectItemCaseSensitive(p,"text"),next->pages[i].text,sizeof(next->pages[i].text),true))goto done;}
    next->key=*key;next->count=(size_t)count;*out=*next;ok=true;
done:free(next);cJSON_Delete(root);return ok;
}
