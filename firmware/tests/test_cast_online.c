#include "cast_protocol.h"
#include "cast_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    assert(cast_valid_date("2028-02-29"));assert(!cast_valid_date("2026-02-29"));
    char d[11]="2026-12-31";cast_date_step(d,1);assert(!strcmp(d,"2027-01-01"));cast_date_step(d,-1);assert(!strcmp(d,"2026-12-31"));
    strcpy(d,"2028-02-28");cast_date_step(d,1);assert(!strcmp(d,"2028-02-29"));cast_date_step(d,1);assert(!strcmp(d,"2028-03-01"));
    strcpy(d,"2020-01-01");cast_date_step(d,-1);assert(!strcmp(d,"2020-01-01"));
    strcpy(d,"2099-12-31");cast_date_step(d,1);assert(!strcmp(d,"2099-12-31"));
    for(size_t city=0;city<8;city++) {
        cast_state_t grid={.city=city};
        cast_navigate(&grid,CAST_LONG_UP);assert(grid.city==(city+6)%8);
        cast_navigate(&grid,CAST_LONG_DOWN);assert(grid.city==city);
    }
    cast_state_t s={0};cast_navigate(&s,CAST_UP);assert(s.city==7);cast_navigate(&s,CAST_LONG_UP);assert(s.city==5);cast_navigate(&s,CAST_LONG_DOWN);assert(s.city==7);
    cast_navigate(&s,CAST_OK);cast_navigate(&s,CAST_OK);assert(s.level==CAST_ACTOR && !cast_group_count(s.venue));cast_navigate(&s,CAST_OK);assert(s.level==CAST_ACTOR);
    cast_snapshot_t *n=calloc(1,sizeof(*n)),*old=calloc(1,sizeof(*old));
    const char *valid="{\"schema_version\":1,\"date\":\"2026-10-03\",\"revision\":1,\"venues\":[]}";
    assert(cast_protocol_parse(valid,strlen(valid),"2026-10-03",n));*old=*n;
    assert(!cast_protocol_parse(valid,strlen(valid),NULL,n));
    assert(!cast_protocol_parse(valid,strlen(valid),"2026-10-04",n));assert(!memcmp(n,old,sizeof(*n)));
    assert(cast_revision_accept(old,n));n->revision=0;assert(!cast_revision_accept(old,n));
    if(argc>1) { FILE *f=fopen(argv[1],"rb");assert(f);char b[CAST_MAX_BODY+1];size_t l=fread(b,1,sizeof(b),f);fclose(f);
        *old=*n;
        bool ok=cast_protocol_parse(b,l,"2026-10-03",n);
        if(!ok) assert(!memcmp(n,old,sizeof(*n)));
        else {
            cast_use_snapshot(n);
            for(size_t v=0;v<n->session_count;v++) {
                s=(cast_state_t){.level=CAST_ACTOR,.venue=v,.city=n->sessions[v].city};
                for(size_t g=0;g<n->sessions[v].count;g++) {
                    assert(s.actor==g);assert(*cast_group_name(v,g));
                    cast_navigate(&s,CAST_OK);assert(s.level==CAST_READER);
                    cast_navigate(&s,CAST_BACK);assert(s.level==CAST_ACTOR && s.actor==g);
                    cast_navigate(&s,CAST_DOWN);
                }
                assert(s.actor==0);
            }
            cast_use_snapshot(NULL);
        }
        free(n);free(old);return ok?0:2; }
    free(n);free(old);puts("Cast online state/date/protocol: PASS");return 0;
}
