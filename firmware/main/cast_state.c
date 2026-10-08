#include "cast_state.h"
#include <stdio.h>
#include <string.h>
static const cast_snapshot_t *online;
static const cast_profile_online_t *downloaded;
void cast_use_profile(const cast_profile_online_t *profile) { downloaded=profile; }
bool cast_current_profile_key(size_t v,size_t g,cast_profile_key_t *out) {
    if(!online || v>=online->session_count || g>=online->sessions[v].count)return false;
    memset(out,0,sizeof(*out));strcpy(out->date,online->date);out->revision=online->revision;
    const cast_group_t *group=&online->groups[online->sessions[v].first+g];
    for(unsigned i=0;i<2;i++)strcpy(out->ids[i],group->members[i].id);
    return true;
}
void cast_use_snapshot(const cast_snapshot_t *s) { online=s; }
size_t cast_city_count(void) {return online?online->city_count:8;}
const char *cast_city_name(size_t city) {return city<cast_city_count()?(online?online->cities[city]:cast_cities[city]):"";}
const char *cast_display_date(void) { return online?online->date:cast_date; }
size_t cast_group_count(size_t v) { return online?(v<online->session_count?online->sessions[v].count:0):(v<9?cast_venues[v].count:0); }
const char *cast_session_label(size_t v) { return online?(v<online->session_count?online->sessions[v].label:""):(v<9?(!strcmp(cast_venues[v].session,"今日场")?"历史场":cast_venues[v].session):""); }
const char *cast_group_name(size_t v,size_t g) {
    static char name[64];
    if(g>=cast_group_count(v)) return "";
    if(!online) return cast_actors[cast_venues[v].first+g].name;
    const cast_group_t *p=&online->groups[online->sessions[v].first+g];
    snprintf(name,sizeof(name),"%s · %s",p->members[0].name,p->members[1].name);return name;
}
const cast_actor_t *cast_profile(size_t v,size_t g) {
    static const cast_page_t page[]={ {"资料说明","资料待补充\n仅供历史参考"} };
    static const cast_actor_t empty={"",page,1};
    if(g>=cast_group_count(v)) return &empty;
    if(!online) return &cast_actors[cast_venues[v].first+g];
    cast_profile_key_t key;
    if(downloaded && cast_current_profile_key(v,g,&key) && key.revision==downloaded->key.revision &&
       !strcmp(key.date,downloaded->key.date) && !strcmp(key.ids[0],downloaded->key.ids[0]) && !strcmp(key.ids[1],downloaded->key.ids[1])) {
        static cast_page_t pages[CAST_PROFILE_PAGES];static cast_actor_t actor;
        for(size_t i=0;i<downloaded->count;i++) pages[i]=(cast_page_t){downloaded->pages[i].title,downloaded->pages[i].text};
        actor=(cast_actor_t){"",pages,downloaded->count};return &actor;
    }
    const char *name=cast_group_name(v,g);
    for(size_t i=0;i<28;i++) if(!strcmp(name,cast_actors[i].name)) return &cast_actors[i];
    return &empty;
}
size_t cast_session_count(size_t city) {
    size_t n=0;
    for(size_t i=0;i<(online?online->session_count:9);i++)
        if((online?online->sessions[i].city:cast_venues[i].city)==city) n++;
    return n;
}
size_t cast_session_index(size_t city,size_t ordinal) {
    for(size_t i=0;i<(online?online->session_count:9);i++)
        if((online?online->sessions[i].city:cast_venues[i].city)==city && ordinal--==0) return i;
    return (size_t)-1;
}
static size_t move(size_t value,size_t count,cast_key_t key) {
    return !count?0:(key==CAST_UP?(value+count-1)%count:(value+1)%count);
}
void cast_navigate(cast_state_t *s,cast_key_t key) {
    if(key==CAST_BACK) {
        if(s->level==CAST_READER) s->level=CAST_ACTOR;
        else if(s->level==CAST_ACTOR) s->level=CAST_SESSION;
        else if(s->level==CAST_SESSION || s->level==CAST_SYNC) s->level=CAST_CITY;
        else s->level=CAST_SYNC;
        return;
    }
    if(s->level==CAST_SYNC) return;
    if(key==CAST_LONG_UP || key==CAST_LONG_DOWN) {
        if(s->level==CAST_CITY) s->city=(s->city+cast_city_count()+(key==CAST_LONG_UP?-2:2))%cast_city_count();
        return;
    }
    if(key==CAST_OK) {
        if(s->level==CAST_CITY) {s->venue=cast_session_index(s->city,0);s->level=CAST_SESSION;}
        else if(s->level==CAST_SESSION && cast_session_count(s->city)) {s->actor=0;s->level=CAST_ACTOR;}
        else if(s->level==CAST_ACTOR && cast_group_count(s->venue)) {s->page=0;s->level=CAST_READER;}
        return;
    }
    if(s->level==CAST_CITY) s->city=move(s->city,cast_city_count(),key);
    else if(s->level==CAST_SESSION) {
        size_t count=cast_session_count(s->city),ordinal=0;
        for(size_t i=0;i<count;i++) if(cast_session_index(s->city,i)==s->venue) ordinal=i;
        s->venue=cast_session_index(s->city,move(ordinal,count,key));
    } else if(s->level==CAST_ACTOR) s->actor=move(s->actor,cast_group_count(s->venue),key);
    else {
        size_t count=cast_profile(s->venue,s->actor)->page_count;
        if(key==CAST_UP && s->page) s->page--;
        if(key==CAST_DOWN && s->page+1<count) s->page++;
    }
}
