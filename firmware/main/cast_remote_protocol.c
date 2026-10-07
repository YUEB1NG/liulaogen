#include "cast_remote.h"
#include "cast_protocol.h"
#include "cJSON.h"
#include <math.h>
#include <string.h>
#include <ctype.h>
bool cast_remote_parse_reply(const char *body,unsigned length,bool *paired,cast_remote_job_t *job) {
    memset(job,0,sizeof(*job));*paired=false;
    if(!body || !length || length>512 || memchr(body,0,length))return false;
    int depth=0;bool string=false,escape=false;
    for(unsigned i=0;i<length;i++) {
        char c=body[i];
        if(string) {if(escape) {escape=false;if(c=='u' && i+4<length && !memcmp(body+i+1,"0000",4))return false;}else if(c=='\\')escape=true;else if(c=='"')string=false;}
        else if(c=='"')string=true;else if(c=='{' || c=='[') {if(++depth>3)return false;}else if(c=='}' || c==']') {if(--depth<0)return false;}
    }
    if(depth || string)return false;
    const char *end=NULL;cJSON *root=cJSON_ParseWithLengthOpts(body,length,&end,false);bool ok=false;
    if(!root)goto done;
    while(end<body+length && isspace((unsigned char)*end))end++;
    if(end!=body+length || !cJSON_IsObject(root) || cJSON_GetArraySize(root)!=2)goto done;
    cJSON *p=cJSON_GetObjectItemCaseSensitive(root,"paired"),*j=cJSON_GetObjectItemCaseSensitive(root,"job");
    if(!cJSON_IsBool(p))goto done;
    if(cJSON_IsNull(j)) {ok=true;*paired=cJSON_IsTrue(p);goto done;}
    if(!cJSON_IsTrue(p) || !cJSON_IsObject(j) || cJSON_GetArraySize(j)!=3)goto done;
    cJSON *id=cJSON_GetObjectItemCaseSensitive(j,"id"),*date=cJSON_GetObjectItemCaseSensitive(j,"date"),*r=cJSON_GetObjectItemCaseSensitive(j,"revision");
    if(!cJSON_IsString(id) || strlen(id->valuestring)!=32 || !cJSON_IsString(date) || !cast_valid_date(date->valuestring) || !cJSON_IsNumber(r))goto done;
    for(const char *s=id->valuestring;*s;s++)if(!((*s>='0' && *s<='9') || (*s>='a' && *s<='f')))goto done;
    if(!isfinite(r->valuedouble) || r->valuedouble<1 || r->valuedouble>UINT32_MAX || r->valuedouble!=(double)(uint32_t)r->valuedouble)goto done;
    strcpy(job->id,id->valuestring);strcpy(job->date,date->valuestring);job->revision=(uint32_t)r->valuedouble;*paired=true;ok=true;
done:cJSON_Delete(root);return ok;
}
