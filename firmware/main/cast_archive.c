#define _POSIX_C_SOURCE 200809L
#include "cast_archive.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define fsync _commit
#endif
#ifdef ESP_PLATFORM
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "nvs.h"
#define CACHE_ROOT "/castcache"
#else
#ifndef CACHE_ROOT
#define CACHE_ROOT "build/host/archive"
#endif
#endif

static cast_archive_hash_t digest_fn;
static bool ready;
static char active_origin[161],active_hash[65];
static bool unique_fields(cJSON *node) {
    if(cJSON_IsObject(node))for(cJSON *a=node->child;a;a=a->next)
        for(cJSON *b=a->next;b;b=b->next)if(!strcmp(a->string,b->string))return false;
    for(cJSON *a=node->child;a;a=a->next)if(!unique_fields(a))return false;
    return true;
}
static cJSON *parse_json(const char *body,size_t n) {
    int depth=0;bool quoted=false,escape=false;
    if(!n || n>CAST_MAX_BODY || memchr(body,0,n))return NULL;
    for(size_t i=0;i<n;i++) {
        char c=body[i];
        if(quoted){if(escape){escape=false;if(c=='u' && i+4<n && !memcmp(body+i+1,"0000",4))return NULL;}
            else if(c=='\\')escape=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c=='{' || c=='['){if(++depth>10)return NULL;}
        else if(c=='}' || c==']'){if(--depth<0)return NULL;}
    }
    if(depth || quoted)return NULL;
    const char *end=NULL;cJSON *json=cJSON_ParseWithLengthOpts(body,n,&end,false);
    if(!json || end!=body+n || !unique_fields(json)){cJSON_Delete(json);return NULL;}return json;
}
static bool hash_valid(const char *s) {
    if(!s || strlen(s)!=64)return false;
    for(unsigned i=0;i<64;i++)if(!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f')))return false;
    return true;
}
static void path_for(const char *hash,char path[128]) {snprintf(path,128,CACHE_ROOT "/%s",hash);}
static char *read_blob(const char *hash,size_t *size) {
    if(!ready || !hash_valid(hash))return NULL;
    char path[128],actual[65];path_for(hash,path);FILE *f=fopen(path,"rb");if(!f)return NULL;
    char *body=malloc(CAST_MAX_BODY+1);
    size_t n=body?fread(body,1,CAST_MAX_BODY+1,f):0;bool ok=body && !ferror(f) && n>0 && n<=CAST_MAX_BODY;
    fclose(f);
    if(ok)ok=digest_fn(body,n,actual) && !strcmp(actual,hash);
    if(!ok){free(body);return NULL;}
    body[n]=0;*size=n;return body;
}
static bool write_blob(const char *hash,const char *body,size_t size) {
    char actual[65],path[128];size_t n;
    if(!ready || !size || size>CAST_MAX_BODY || !hash_valid(hash) || !digest_fn(body,size,actual) || strcmp(actual,hash))return false;
    char *old=read_blob(hash,&n);if(old){bool same=n==size;free(old);return same;}
#ifdef ESP_PLATFORM
    size_t total,used;
    if(esp_spiffs_info("cast_cache",&total,&used)!=ESP_OK || used+size+1024>total*3/4)return false;
#endif
    /* An incomplete immutable file is never referenced by a committed window. */
    path_for(hash,path);FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(body,1,size,f)==size && fflush(f)==0 && fsync(fileno(f))==0;
    if(fclose(f)!=0)ok=false;
    if(ok){char *check=read_blob(hash,&n);ok=check && n==size;free(check);}
    if(!ok)remove(path);
    return ok;
}
static cJSON *json_blob(const char *hash) {
    size_t n;char *body=read_blob(hash,&n);if(!body)return NULL;
    cJSON *json=parse_json(body,n);
    free(body);return json;
}
static const char *str(cJSON *o,const char *key) {return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(o,key));}
static uint32_t revision(cJSON *o) {
    cJSON *r=cJSON_GetObjectItemCaseSensitive(o,"revision");
    return cJSON_IsNumber(r) && r->valuedouble>=0 && r->valuedouble<=4294967295.0 && r->valuedouble==(uint32_t)r->valuedouble?(uint32_t)r->valuedouble:UINT32_MAX;
}
static bool origin_ok(const char *origin) {return ready && origin && !strcmp(origin,active_origin) && hash_valid(active_hash);}
static cJSON *day_record(cJSON *window,const char *date) {
    cJSON *day,*days=cJSON_GetObjectItemCaseSensitive(window,"days");
    cJSON_ArrayForEach(day,days)if(str(day,"date") && !strcmp(str(day,"date"),date))return day;
    return NULL;
}
static bool download(const char *hash,cast_archive_get_t get) {
    if(!hash_valid(hash))return false;
    size_t size;char *body=read_blob(hash,&size);if(body){free(body);return true;}
    body=malloc(CAST_MAX_BODY+1);if(!body)return false;
    char path[96];snprintf(path,sizeof(path),"/api/device/blob?id=%s",hash);size=CAST_MAX_BODY;
    bool ok=get(path,body,&size) && write_blob(hash,body,size);free(body);return ok;
}
static bool profile_blob(const char *hash,const cast_profile_key_t *key,cast_profile_online_t *out) {
    cJSON *p=json_blob(hash);if(!p)return false;
    /* Metadata comes from the validated day, never from a recycled profile. */
    bool ok=cJSON_IsObject(p) && !cJSON_GetObjectItemCaseSensitive(p,"date") &&
        !cJSON_GetObjectItemCaseSensitive(p,"revision") && !cJSON_GetObjectItemCaseSensitive(p,"schema_version") &&
        cJSON_AddNumberToObject(p,"schema_version",1) && cJSON_AddStringToObject(p,"date",key->date) &&
        cJSON_AddNumberToObject(p,"revision",key->revision);
    char *body=ok?cJSON_PrintUnformatted(p):NULL;cJSON_Delete(p);
    ok=body && cast_profile_parse(body,strlen(body),key,out);free(body);return ok;
}
static bool validate_day(cJSON *day,cast_archive_get_t get) {
    const char *date=str(day,"date"),*hash=str(day,"archive");uint32_t rev=revision(day);
    if(!date || !cast_valid_date(date) || rev==UINT32_MAX || !hash)return false;
    if(!rev)return !*hash;
    if(!download(hash,get))return false;
    cJSON *archive=json_blob(hash);cast_snapshot_t *snapshot=malloc(sizeof(*snapshot));
    bool ok=false;char *body=NULL;cast_profile_online_t *profile=NULL;
    if(!archive || !snapshot || !str(archive,"date") || strcmp(str(archive,"date"),date) || revision(archive)!=rev)goto done;
    const char *lineup=str(archive,"lineup");
    if(!download(lineup,get))goto done;
    size_t size;body=read_blob(lineup,&size);
    if(!body || !cast_protocol_parse(body,size,date,snapshot) || snapshot->revision!=rev)goto done;
    free(body);body=NULL;
    cJSON *profiles=cJSON_GetObjectItemCaseSensitive(archive,"profiles");
    if(!cJSON_IsArray(profiles) || cJSON_GetArraySize(profiles)!=snapshot->group_count)goto done;
    profile=malloc(sizeof(*profile));if(!profile)goto done;
    for(unsigned i=0;i<snapshot->group_count;i++) {
        const char *ph=cJSON_GetStringValue(cJSON_GetArrayItem(profiles,i));
        cast_profile_key_t key={.revision=rev};strcpy(key.date,date);
        for(unsigned m=0;m<2;m++)strcpy(key.ids[m],snapshot->groups[i].members[m].id);
        if(!download(ph,get) || !profile_blob(ph,&key,profile))goto done;
    }
    ok=true;
done:free(profile);free(body);free(snapshot);cJSON_Delete(archive);return ok;
}
static bool commit_window(const char *origin,const char *hash) {
    char record[226]={0};strcpy(record,origin);strcpy(record+161,hash);
#ifdef ESP_PLATFORM
    nvs_handle_t h;if(nvs_open("cast_archive",NVS_READWRITE,&h)!=ESP_OK)return false;
    bool ok=nvs_set_blob(h,"active",record,sizeof(record))==ESP_OK && nvs_commit(h)==ESP_OK;nvs_close(h);
#else
    FILE *f=fopen(CACHE_ROOT "/active.next","wb");if(!f)return false;
    bool ok=fwrite(record,1,sizeof(record),f)==sizeof(record) && fflush(f)==0 && fsync(fileno(f))==0;
    if(fclose(f)!=0)ok=false;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(CACHE_ROOT "/active.next",CACHE_ROOT "/active",MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    if(ok)ok=rename(CACHE_ROOT "/active.next",CACHE_ROOT "/active")==0;
#endif
#endif
    if(ok){strcpy(active_origin,origin);strcpy(active_hash,hash);}return ok;
}
static bool retain(char (**keep)[65],unsigned *count,unsigned *capacity,const char *hash) {
    if(!hash_valid(hash))return false;
    for(unsigned i=0;i<*count;i++)if(!strcmp((*keep)[i],hash))return true;
    if(*count>=1+15*(2+CAST_MAX_GROUPS))return false;
    if(*count==*capacity) {
        unsigned next=*capacity+16;
        void *grown=realloc(*keep,next*65);if(!grown)return false;
        *keep=grown;*capacity=next;
    }
    strcpy((*keep)[(*count)++],hash);return true;
}
/* Keep only unique hashes; allocation failure cancels collection before deletion. */
static void collect(cJSON *window) {
    char (*keep)[65]=NULL;unsigned count=0,capacity=0;cJSON *day;
    if(!retain(&keep,&count,&capacity,active_hash))goto done;
    cJSON_ArrayForEach(day,cJSON_GetObjectItemCaseSensitive(window,"days")) {
        if(!revision(day))continue;
        const char *h=str(day,"archive");if(!retain(&keep,&count,&capacity,h))goto done;
        cJSON *a=json_blob(h);if(!a)goto done;
        h=str(a,"lineup");if(!retain(&keep,&count,&capacity,h)){cJSON_Delete(a);goto done;}
        cJSON *p;cJSON_ArrayForEach(p,cJSON_GetObjectItemCaseSensitive(a,"profiles")) {
            h=cJSON_GetStringValue(p);if(!retain(&keep,&count,&capacity,h)){cJSON_Delete(a);goto done;}
        }
        cJSON_Delete(a);
    }
    DIR *dir=opendir(CACHE_ROOT);if(!dir)goto done;
    struct dirent *entry;
    while((entry=readdir(dir)))if(hash_valid(entry->d_name)) {
        unsigned i=0;while(i<count && strcmp(keep[i],entry->d_name))i++;
        if(i==count){char path[128];path_for(entry->d_name,path);remove(path);}
    }
    closedir(dir);
done:free(keep);
}
bool cast_archive_init(cast_archive_hash_t hash) {
    digest_fn=hash;ready=false;active_hash[0]=active_origin[0]=0;
    if(!hash)return false;
#ifdef ESP_PLATFORM
    esp_vfs_spiffs_conf_t cfg={.base_path=CACHE_ROOT,.partition_label="cast_cache",.max_files=4,.format_if_mount_failed=false};
    if(esp_vfs_spiffs_register(&cfg)!=ESP_OK) {
        /* Format only provably erased first-use storage, never a damaged cache. */
        const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"cast_cache");
        uint8_t *block=malloc(4096);bool blank=p && block;
        for(size_t off=0;blank && off<p->size;off+=4096) {
            if(esp_partition_read(p,off,block,4096)!=ESP_OK){blank=false;break;}
            for(unsigned i=0;i<4096;i++)if(block[i]!=0xff){blank=false;break;}
        }
        free(block);
        if(!blank || esp_spiffs_format("cast_cache")!=ESP_OK || esp_vfs_spiffs_register(&cfg)!=ESP_OK)return false;
    }
    nvs_handle_t h;char record[226];size_t size=sizeof(record);
    if(nvs_open("cast_archive",NVS_READONLY,&h)==ESP_OK) {
        if(nvs_get_blob(h,"active",record,&size)==ESP_OK && size==sizeof(record) && memchr(record,0,161) && record[225]==0 && hash_valid(record+161)) {
            strcpy(active_origin,record);strcpy(active_hash,record+161);
        }nvs_close(h);
    }
#else
    FILE *f=fopen(CACHE_ROOT "/active","rb");char record[226];
    if(f){if(fread(record,1,sizeof(record),f)==sizeof(record) && memchr(record,0,161) && record[225]==0 && hash_valid(record+161)){strcpy(active_origin,record);strcpy(active_hash,record+161);}fclose(f);}
#endif
    ready=true;return true;
}
bool cast_archive_update(const char *origin,const char *date,uint32_t rev,cast_archive_get_t get) {
    if(!ready || !origin || strlen(origin)>160 || !cast_valid_date(date) || !get)return false;
    cJSON *old=json_blob(active_hash);if(old){collect(old);cJSON_Delete(old);}
    char path[100],hash[65];snprintf(path,sizeof(path),"/api/device/window?date=%s&revision=%lu",date,(unsigned long)rev);
    char *body=malloc(CAST_MAX_BODY+1);if(!body)return false;size_t size=CAST_MAX_BODY;
    cJSON *window=NULL,*previous=origin_ok(origin)?json_blob(active_hash):NULL;bool ok=false;
    if(!get(path,body,&size) || !size || size>CAST_MAX_BODY || memchr(body,0,size))goto done;
    body[size]=0;window=parse_json(body,size);if(!window)goto done;
    const char *today=str(window,"today");cJSON *days=cJSON_GetObjectItemCaseSensitive(window,"days");
    if(!today || !cast_valid_date(today) || !cJSON_IsArray(days) || cJSON_GetArraySize(days)!=15 || !day_record(window,date))goto done;
    char expected[11];strcpy(expected,today);for(int i=0;i<7;i++)cast_date_step(expected,-1);
    for(int i=0;i<15;i++) {
        cJSON *day=cJSON_GetArrayItem(days,i);
        cJSON *prior=day_record(previous,expected);
        if(prior && revision(prior)>0) {
            if(revision(day)<revision(prior))goto done;
            if(revision(day)==revision(prior) && (!str(day,"archive") || !str(prior,"archive") || strcmp(str(day,"archive"),str(prior,"archive"))))goto done;
        }
        if(!str(day,"date") || strcmp(str(day,"date"),expected) || !validate_day(day,get))goto done;
        cast_date_step(expected,1);
    }
    cJSON *selected=day_record(window,date);
    if(rev && revision(selected)!=rev)goto done;
    if(!digest_fn(body,size,hash) || !write_blob(hash,body,size) || !commit_window(origin,hash))goto done;
    ok=true;collect(window);
done:cJSON_Delete(previous);cJSON_Delete(window);free(body);return ok;
}
bool cast_archive_lineup(const char *origin,const char *date,cast_snapshot_t *out) {
    if(!origin_ok(origin) || !out)return false;
    cJSON *w=json_blob(active_hash),*d=day_record(w,date);cJSON *a=d?json_blob(str(d,"archive")):NULL;
    size_t n;char *body=a?read_blob(str(a,"lineup"),&n):NULL;
    bool ok=body && cast_protocol_parse(body,n,date,out) && out->revision==revision(d);
    free(body);cJSON_Delete(a);cJSON_Delete(w);return ok;
}
bool cast_archive_profile(const char *origin,const cast_profile_key_t *key,cast_profile_online_t *out) {
    if(!origin_ok(origin) || !key || !out)return false;
    cJSON *w=json_blob(active_hash),*d=day_record(w,key->date),*a=NULL;bool ok=false;
    cast_snapshot_t *snapshot=NULL;char *body=NULL;
    if(!d || revision(d)!=key->revision || !(a=json_blob(str(d,"archive"))))goto done;
    size_t n;body=read_blob(str(a,"lineup"),&n);snapshot=malloc(sizeof(*snapshot));
    if(!body || !snapshot || !cast_protocol_parse(body,n,key->date,snapshot))goto done;
    free(body);body=NULL;
    for(unsigned i=0;i<snapshot->group_count;i++)if(!strcmp(key->ids[0],snapshot->groups[i].members[0].id) && !strcmp(key->ids[1],snapshot->groups[i].members[1].id)) {
        const char *hash=cJSON_GetStringValue(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(a,"profiles"),i));
        ok=profile_blob(hash,key,out);break;
    }
done:free(snapshot);free(body);cJSON_Delete(a);cJSON_Delete(w);return ok;
}
bool cast_archive_today(const char *origin,char date[11]) {
    if(!origin_ok(origin))return false;
    cJSON *w=json_blob(active_hash);const char *today=str(w,"today");
    bool ok=today && cast_valid_date(today);
    if(ok) {
        cJSON *day=day_record(w,today);
        if(!day || !revision(day)) {
            const char *selected=str(w,"selected");day=selected?day_record(w,selected):NULL;
            if(!day || !revision(day)) {
                day=NULL;cJSON *item;
                cJSON_ArrayForEach(item,cJSON_GetObjectItemCaseSensitive(w,"days"))
                    if(revision(item) && (!day || strcmp(str(item,"date"),today)<=0))day=item;
            }
        }
        ok=day && str(day,"date") && cast_valid_date(str(day,"date"));
        if(ok)strcpy(date,str(day,"date"));
    }
    cJSON_Delete(w);return ok;
}
