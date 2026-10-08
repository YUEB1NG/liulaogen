#include "cast_protocol.h"
#include "cast_data.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "cast_supported.h"
bool cast_text_supported(const char *s) {
    const unsigned char *p=(const unsigned char *)s;
    while(*p) {
        unsigned int cp=*p++;int n=0;
        if(cp>=0xC2 && cp<=0xDF) {cp&=31;n=1;}
        else if(cp>=0xE0 && cp<=0xEF) {cp&=15;n=2;}
        else if(cp>=128) return false;
        for(int i=0;i<n;i++) {if((*p&0xC0)!=0x80)return false;cp=(cp<<6)|(*p++&63);}
        if((n==1 && cp<128) || (n==2 && cp<2048) || (cp>=0xD800 && cp<=0xDFFF))return false;
        size_t lo=0,hi=sizeof(cast_supported)/sizeof(*cast_supported);
        while(lo<hi) {size_t mid=lo+(hi-lo)/2;if(cast_supported[mid]<cp)lo=mid+1;else hi=mid;}
        if(lo==sizeof(cast_supported)/sizeof(*cast_supported) || cast_supported[lo]!=cp)return false;
    }
    return true;
}
static int days(int y,int m) {
    static const int n[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return n[m-1]+(m==2 && y%4==0 && (y%100!=0 || y%400==0));
}
bool cast_valid_date(const char *s) {
    if (!s || strlen(s)!=10 || s[4]!='-' || s[7]!='-') return false;
    for(int i=0;i<10;i++) if(i!=4 && i!=7 && !isdigit((unsigned char)s[i])) return false;
    int y=atoi(s),m=atoi(s+5),d=atoi(s+8);
    return y>=2020 && y<=2099 && m>=1 && m<=12 && d>=1 && d<=days(y,m);
}
void cast_date_step(char s[11],int delta) {
    if(!cast_valid_date(s) || !delta) return;
    int y=atoi(s),m=atoi(s+5),d=atoi(s+8);
    if(delta>0) { if(++d>days(y,m)) { d=1; if(++m>12){m=1;y++;} } }
    else if(--d<1) { if(--m<1){m=12;y--;} d=days(y,m); }
    if(y>=2020 && y<=2099) snprintf(s,11,"%04d-%02d-%02d",y,m,d);
}
static cJSON *field(cJSON *o,const char *k) { return cJSON_GetObjectItemCaseSensitive(o,k); }
/* IDs are opaque ASCII identifiers, not display text or glyph-dependent names. */
static bool identifier(cJSON *v,char *dst,size_t cap) {
    if(!cJSON_IsString(v) || !v->valuestring || !*v->valuestring || strlen(v->valuestring)>=cap) return false;
    for(const unsigned char *p=(const unsigned char *)v->valuestring;*p;p++)
        if(!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') ||
             (*p>='0' && *p<='9') || *p=='_' || *p=='-')) return false;
    strcpy(dst,v->valuestring);return true;
}
static bool text(cJSON *v,char *dst,size_t cap) {
    if(!cJSON_IsString(v) || !v->valuestring || !*v->valuestring || strlen(v->valuestring)>=cap) return false;
    for(const unsigned char *p=(void*)v->valuestring;*p;p++) if(*p<32 || *p==127) return false;
    if(!cast_text_supported(v->valuestring)) return false;
    strcpy(dst,v->valuestring);return true;
}
static bool unique_fields(cJSON *v) {
    if(cJSON_IsObject(v)) for(cJSON *a=v->child;a;a=a->next)
        for(cJSON *b=a->next;b;b=b->next) if(!strcmp(a->string,b->string)) return false;
    for(cJSON *a=v->child;a;a=a->next) if(!unique_fields(a)) return false;
    return true;
}
static bool parse(const char *json,size_t len,const char *date,cast_snapshot_t *out) {
    if(!json || !out || (date && !cast_valid_date(date)) || !len || len>CAST_MAX_BODY || memchr(json,0,len)) return false;
    /* Bound nesting before cJSON allocation/recursion; reject escaped NUL strings. */
    int depth=0; bool str=false,escape=false;
    for(size_t i=0;i<len;i++) {
        char c=json[i];
        if(str) { if(escape) { escape=false; if(c=='u' && i+4<len && !memcmp(json+i+1,"0000",4)) return false; }
            else if(c=='\\') escape=true; else if(c=='"') str=false;
        } else if(c=='"') str=true;
        else if(c=='{' || c=='[') { if(++depth>10) return false; }
        else if(c=='}' || c==']') { if(--depth<0) return false; }
    }
    if(depth || str) return false;
    const char *end=NULL;
    cJSON *root=cJSON_ParseWithLengthOpts(json,len,&end,0);
    cast_snapshot_t *n=calloc(1,sizeof(*n)); bool ok=false;
    if(!root || !n || !cJSON_IsObject(root) || !unique_fields(root)) goto done;
    while(end<json+len && isspace((unsigned char)*end)) end++;
    if(end!=json+len) goto done;
    cJSON *v=field(root,"schema_version"),*r=field(root,"revision"),*vs=field(root,"venues");
    if(!cJSON_IsNumber(v) || (v->valuedouble!=1 && v->valuedouble!=2) || !cJSON_IsNumber(r) || !isfinite(r->valuedouble) || r->valuedouble<1 || r->valuedouble>4294967295.0 || r->valuedouble!=(uint32_t)r->valuedouble) goto done;
    if(!text(field(root,"date"),n->date,sizeof(n->date)) || !cast_valid_date(n->date) || (date && strcmp(n->date,date)) || !cJSON_IsArray(vs) || cJSON_GetArraySize(vs)>CAST_MAX_CITIES) goto done;
    n->revision=(uint32_t)r->valuedouble;
    bool seen[CAST_MAX_CITIES]={0}; char venue_ids[CAST_MAX_CITIES][33]={{0}};
    static const char *const ids[]={"zhongjie","haerbin","beijing","changchun","nanjing","taian","linyi","dalian"};
    if(v->valuedouble==1) {
        n->city_count=8;
        for(int i=0;i<8;i++) {strcpy(n->cities[i],cast_cities[i]);strcpy(venue_ids[i],ids[i]);}
    } else {
        cJSON *cs=field(root,"cities"),*c;
        if(!cJSON_IsArray(cs) || cJSON_GetArraySize(cs)<1 || cJSON_GetArraySize(cs)>CAST_MAX_CITIES)goto done;
        cJSON_ArrayForEach(c,cs) {
            unsigned ci=n->city_count;
            if(!identifier(field(c,"id"),venue_ids[ci],33) || !text(field(c,"city"),n->cities[ci],CAST_NAME_BYTES))goto done;
            for(unsigned j=0;j<ci;j++)if(!strcmp(venue_ids[j],venue_ids[ci]) || !strcmp(n->cities[j],n->cities[ci]))goto done;
            n->city_count++;
        }
    }
    cJSON *venue;
    cJSON_ArrayForEach(venue,vs) {
        char city[25],id[33];
        if(!text(field(venue,"city"),city,sizeof(city)) || !identifier(field(venue,"id"),id,sizeof(id))) goto done;
        unsigned ci=0; while(ci<n->city_count && strcmp(id,venue_ids[ci])) ci++;
        if(ci==n->city_count || seen[ci] || strcmp(city,n->cities[ci]))goto done;
        seen[ci]=true;
        cJSON *ss=field(venue,"sessions"),*s;
        if(!cJSON_IsArray(ss) || cJSON_GetArraySize(ss)>2) goto done;
        cJSON_ArrayForEach(s,ss) {
            if(n->session_count>=CAST_MAX_SESSIONS) goto done;
            cast_session_t *ns=&n->sessions[n->session_count++]; ns->city=ci;ns->first=n->group_count;
            if(!identifier(field(s,"id"),ns->id,sizeof(ns->id)) || !text(field(s,"label"),ns->label,sizeof(ns->label))) goto done;
            if(strcmp(ns->id,"afternoon") && strcmp(ns->id,"evening")) goto done;
            cJSON *time=field(s,"time");
            if(time) {
                if(!cJSON_IsString(time) || strlen(time->valuestring)!=5)goto done;
                const char *t=time->valuestring;
                if(t[2]!=':' || t[0]<'0' || t[0]>'2' || t[1]<'0' || t[1]>'9' ||
                   (t[0]=='2' && t[1]>'3') || t[3]<'0' || t[3]>'5' || t[4]<'0' || t[4]>'9')goto done;
                size_t len=strlen(ns->label);if(len+8>=sizeof(ns->label))goto done;
                snprintf(ns->label+len,sizeof(ns->label)-len," (%s)",t);
            }
            for(int j=0;j<n->session_count-1;j++) if(n->sessions[j].city==ci && !strcmp(n->sessions[j].id,ns->id)) goto done;
            cJSON *gs=field(s,"groups"),*g;
            if(!cJSON_IsArray(gs) || cJSON_GetArraySize(gs)>CAST_MAX_GROUPS_PER_SESSION) goto done;
            cJSON_ArrayForEach(g,gs) {
                if(n->group_count>=CAST_MAX_GROUPS) goto done;
                cJSON *ms=field(g,"members");
                if(!cJSON_IsArray(ms) || cJSON_GetArraySize(ms)!=2) goto done;
                cast_group_t *ng=&n->groups[n->group_count++]; ns->count++;
                for(int k=0;k<2;k++) {
                    cJSON *m=cJSON_GetArrayItem(ms,k);
                    if(!identifier(field(m,"id"),ng->members[k].id,sizeof(ng->members[k].id)) || !text(field(m,"name"),ng->members[k].name,sizeof(ng->members[k].name))) goto done;
                    for(size_t a=ns->first;a<n->group_count;a++) for(int b=0;b<2;b++) {
                        if(a+1==(size_t)n->group_count && b>=k) continue;
                        if(!strcmp(n->groups[a].members[b].id,ng->members[k].id)) goto done;
                    }
                }
            }
        }
    }
    *out=*n;ok=true;
done:
    cJSON_Delete(root);free(n);return ok;
}
bool cast_protocol_parse(const char *json,size_t len,const char *date,cast_snapshot_t *out) {
    return date && parse(json,len,date,out);
}
bool cast_protocol_parse_cached(const char *json,size_t len,cast_snapshot_t *out) {
    return parse(json,len,NULL,out);
}
bool cast_revision_accept(const cast_snapshot_t *old,const cast_snapshot_t *next) {
    return !old || strcmp(old->date,next->date) || next->revision>old->revision ||
        (next->revision==old->revision && !memcmp(old,next,sizeof(*old)));
}
