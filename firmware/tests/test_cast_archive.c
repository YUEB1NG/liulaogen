#define _POSIX_C_SOURCE 200809L
/* Real filesystem transaction logic; deterministic transport/hash boundary.
 * Production hashes use ESP-IDF mbedTLS SHA-256; fixture hashes come from Python hashlib. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>
static bool disk_full;
static size_t test_write(const void *p,size_t size,size_t count,FILE *f) {
    return disk_full?0:fwrite(p,size,count,f);
}
#define fwrite test_write
#include "../main/cast_archive.c"
#undef fwrite
static unsigned version,requests,fail_request;
static bool fixture_hash(const void *body,size_t size,char out[65]) {
    DIR *dir=opendir("build/host/archive-fixtures");assert(dir);struct dirent *e;bool ok=false;
    char *copy=malloc(CAST_MAX_BODY+1);assert(copy);
    while((e=readdir(dir)))if(hash_valid(e->d_name)) {
        char path[160];snprintf(path,sizeof(path),"build/host/archive-fixtures/%.64s",e->d_name);
        FILE *f=fopen(path,"rb");assert(f);size_t n=fread(copy,1,CAST_MAX_BODY+1,f);fclose(f);
        if(n==size && !memcmp(copy,body,size)){strcpy(out,e->d_name);ok=true;break;}
    }
    free(copy);closedir(dir);return ok;
}
static bool get(const char *path,char *body,size_t *size) {
    if(++requests==fail_request)return false;
    char file[180];
    if(strncmp(path,"/api/device/window?",19)==0)snprintf(file,sizeof(file),"build/host/archive-fixtures/window%u.json",version);
    else {assert(strncmp(path,"/api/device/blob?id=",20)==0);snprintf(file,sizeof(file),"build/host/archive-fixtures/%s",path+20);}
    FILE *f=fopen(file,"rb");if(!f)fprintf(stderr,"missing fixture: %s\n",file);assert(f);size_t n=fread(body,1,*size+1,f);fclose(f);
    if(n>*size)return false;
    *size=n;return true;
}
static void check_window(unsigned revision_number) {
    char date[11]="2026-10-01";cast_snapshot_t *s=malloc(sizeof(*s));cast_profile_online_t *p=malloc(sizeof(*p));assert(s && p);
    for(unsigned i=0;i<15;i++) {
        assert(cast_archive_lineup("https://test.invalid",date,s));assert(s->revision==revision_number);
        assert(s->city_count==1 && !strcmp(s->cities[0],"天津") && !strcmp(s->sessions[0].label,"晚场 (19:00)"));
        cast_profile_key_t key={.revision=revision_number,.ids={"a","b"}};strcpy(key.date,date);
        assert(cast_archive_profile("https://test.invalid",&key,p));assert(p->count==1);
        assert(!cast_archive_lineup("https://other.invalid",date,s));
        cast_date_step(date,1);
    }
    free(s);free(p);
}
int main(void) {
    assert(cast_archive_init(fixture_hash));
    assert(cast_archive_update("https://test.invalid","2026-10-08",1,get));
    assert(requests==32); /* One window, fifteen lineups, fifteen day manifests, ONE shared biography. */
    check_window(1);assert(cast_archive_init(fixture_hash));check_window(1);
    version=1;requests=0;fail_request=4;
    assert(!cast_archive_update("https://test.invalid","2026-10-08",2,get));check_window(1);
    requests=fail_request=0;disk_full=true;
    assert(!cast_archive_update("https://test.invalid","2026-10-08",2,get));disk_full=false;check_window(1);
    assert(cast_archive_update("https://test.invalid","2026-10-08",2,get));check_window(2);
    assert(cast_archive_init(fixture_hash));check_window(2);
    version=0;assert(!cast_archive_update("https://test.invalid","2026-10-08",1,get));check_window(2);
    version=2;assert(!cast_archive_update("https://test.invalid","2026-10-08",3,get));check_window(2);
    char date[11];assert(cast_archive_today("https://test.invalid",date) && !strcmp(date,"2026-10-08"));
    assert(!cast_archive_update("https://test.invalid","2026-09-30",0,get));
    puts("Archive: PASS (15 dates+profiles, 13-column pages, deduplication, restart, wrong origin, interrupted download, disk full, mismatched actor IDs, old-window preservation)");
    return 0;
}
