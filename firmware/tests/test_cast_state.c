#include <assert.h>
#include <stdio.h>
#include "cast_state.h"
int main(void) {
    const size_t expected[] = {5,5,5,5,0,3,5,0,0};
    size_t actors=0, visits=0;
    for (size_t v=0; v<9; ++v) { assert(cast_venues[v].count==expected[v]); actors+=expected[v]; }
    assert(actors==28);
    assert(cast_session_count(5)==2);
    assert(cast_session_index(5,0)==5 && cast_session_index(5,1)==6);
    cast_state_t s={0};
    cast_navigate(&s,CAST_UP); assert(s.city==7);
    cast_navigate(&s,CAST_DOWN); assert(s.city==0);
    cast_navigate(&s,CAST_BACK); assert(s.level==CAST_SYNC);
    cast_navigate(&s,CAST_BACK); assert(s.level==CAST_CITY);
    for (size_t c=0;c<8;++c) {
        s=(cast_state_t){.city=c}; cast_navigate(&s,CAST_OK);
        assert(s.level==CAST_SESSION);
        for (size_t v=0;v<cast_session_count(c);++v) {
            size_t venue=s.venue;
            cast_navigate(&s,CAST_OK); assert(s.level==CAST_ACTOR);
            if (!cast_venues[venue].count) {
                cast_navigate(&s,CAST_UP); cast_navigate(&s,CAST_DOWN); cast_navigate(&s,CAST_OK);
                assert(s.level==CAST_ACTOR && s.actor==0);
            }
            for(size_t a=0;a<cast_venues[venue].count;++a) {
                assert(s.actor==a); cast_navigate(&s,CAST_OK);
                assert(s.level==CAST_READER && s.page==0);
                cast_navigate(&s,CAST_UP); assert(s.page==0);
                size_t n=cast_actors[cast_venues[venue].first+a].page_count;
                assert(n>0);
                for(size_t p=0;p<n;++p) {
                    assert(s.page==p); ++visits;
                    cast_navigate(&s,CAST_DOWN);
                }
                assert(s.page==n-1); cast_navigate(&s,CAST_DOWN); assert(s.page==n-1);
                cast_navigate(&s,CAST_BACK); assert(s.actor==a && s.level==CAST_ACTOR);
                cast_navigate(&s,CAST_DOWN);
            }
            assert(s.actor==0); cast_navigate(&s,CAST_BACK);
            assert(s.level==CAST_SESSION && s.venue==venue); cast_navigate(&s,CAST_DOWN);
        }
        assert(s.venue==cast_session_index(c,0)); cast_navigate(&s,CAST_UP);
        assert(s.venue==cast_session_index(c,cast_session_count(c)-1));
        cast_navigate(&s,CAST_BACK); assert(s.city==c);
    }
    printf("Cast state/data: PASS (8 cities, 9 sessions, 28 cards, %zu pages)\n", visits);
    return 0;
}
