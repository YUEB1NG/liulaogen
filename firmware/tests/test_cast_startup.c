/* Execute the real application entry point at typed platform boundaries.
 * Check visible startup and failure paths; this does not render physical pixels. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "app_platform.h"
#include "../main/main.c"

static jmp_buf reached_loop;
static int fail_at, brightness, labels, city_labels;
static bool locked, initialized, rendered, buttons_ready;
static lv_obj_t objects[32];
static char contents[32][512];
static int hidden[32];
static cast_key_t queued[12];
static unsigned queue_count, send_calls, scenario_step, cycles;
static bool exercise_loop, pending_result;
static TickType_t ticks;
#ifdef CONFIG_CAST_WIFI_PORTAL
static unsigned setup_calls,cancel_calls,forget_calls,today_calls;
static cast_net_view_t simulated_network;
void cast_network_view(cast_net_view_t *out) {*out=simulated_network;}
bool cast_network_setup(void) {setup_calls++;simulated_network.scan_generation++;simulated_network.count=1;strcpy(simulated_network.access[0].ssid,"test-wifi");return true;}
bool cast_network_connect(const char *s,const char *p) {assert(!strcmp(s,"test-wifi") && !strcmp(p,""));simulated_network.state=NET_SAVED;simulated_network.state_generation++;simulated_network.connected=true;return true;}
bool cast_network_set_origin(const char *o) {(void)o;return true;}
bool cast_network_origin(char *o,size_t n) {assert(n);o[0]=0;return false;}
void cast_remote_view(cast_remote_view_t *o) {memset(o,0,sizeof(*o));}
bool cast_remote_pair(void) {return true;}
bool cast_text_supported(const char *t) {(void)t;return true;}
bool cast_network_cancel(void) {cancel_calls++;memset(&simulated_network,0,sizeof(simulated_network));return true;}
bool cast_network_forget(void) {forget_calls++;memset(&simulated_network,0,sizeof(simulated_network));return true;}
bool cast_network_today(char out[11]) {(void)out;today_calls++;return false;}
bool cast_sync_profile_request(const cast_profile_key_t *key) {(void)key;return false;}
bool cast_sync_profile_poll(cast_profile_online_t **out) {(void)out;return false;}
bool cast_profile_key_equal(const cast_profile_key_t *a,const cast_profile_key_t *b) {return !memcmp(a,b,sizeof(*a));}
#endif
static void (*button_callback)(bsp_btn_t,bsp_btn_ev_t,void *);
static void next_action(void);
const lv_font_t cast_font_16 = {0};

void test_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "test"; }
esp_err_t bsp_display_init(void) { brightness=0; return fail_at==1?ESP_FAIL:ESP_OK; }
bool bsp_lvgl_init(void) { initialized=fail_at!=2; return initialized; }
bool bsp_lvgl_lock(int timeout) { (void)timeout; assert(!locked); locked=fail_at!=4; return locked; }
void bsp_lvgl_unlock(void) { assert(locked); locked=false; rendered=city_labels>=8; }
void lv_refr_now(void *d) {(void)d;assert(locked && city_labels>=8);}
esp_err_t bsp_display_flush_wait(void) {assert(locked && city_labels>=8);return fail_at==6?ESP_FAIL:ESP_OK;}
void bsp_display_backlight(uint8_t percent) {
    assert(initialized && rendered && !locked);
    assert(percent>0 && percent<=100);
    brightness=percent;
}
esp_err_t bsp_battery_init(void) { return fail_at==5?ESP_FAIL:ESP_OK; }
int bsp_battery_soc(void) { return 50; }
esp_err_t bsp_button_init(void (*cb)(bsp_btn_t,bsp_btn_ev_t,void *),void *arg) {
    (void)arg; assert(brightness>0); buttons_ready=true; button_callback=cb; return ESP_OK;
}
QueueHandle_t xQueueCreate(unsigned count,unsigned size) {
    assert(count>0 && size==sizeof(cast_key_t)); return fail_at==3?NULL:(void *)1;
}
int xQueueSend(QueueHandle_t q,const void *data,unsigned timeout) {
    assert(q && data && timeout==0 && !locked); ++send_calls;
    if (queue_count==12) return pdFALSE;
    queued[queue_count++]=*(const cast_key_t *)data; return pdTRUE;
}
int xQueueReceive(QueueHandle_t q,void *data,unsigned timeout) {
    assert(q && data && timeout==40 && buttons_ready && brightness>0);
    if (!exercise_loop) longjmp(reached_loop,1);
    ticks+=timeout;
    if (!queue_count && pending_result) return pdFALSE;
    if (!queue_count) next_action();
    assert(queue_count);
    *(cast_key_t *)data=queued[0];
    memmove(queued,queued+1,--queue_count*sizeof(*queued));
    return pdTRUE;
}
TickType_t xTaskGetTickCount(void) { return ticks; }
bool cast_sync_init(void) { return exercise_loop; }
cast_snapshot_t *cast_sync_cached(void) { return NULL; }
bool cast_sync_request(const char *date) { assert(!strcmp(date,cast_date)); pending_result=true; return true; }
bool cast_sync_poll(cast_sync_status_t *result,cast_snapshot_t **snapshot) {
    if (!pending_result) return false;
    *result=CAST_NOT_CONFIGURED; *snapshot=NULL; pending_result=false; return true;
}
void cast_date_step(char date[11],int direction) { (void)date;(void)direction; }

uint32_t lv_color_hex(uint32_t color) { assert(locked);return color; }
lv_obj_t *lv_screen_active(void) { assert(locked);return &objects[31]; }
lv_obj_t *lv_label_create(lv_obj_t *parent) { assert(locked && parent && labels<31);return &objects[labels++]; }
void lv_label_set_text(lv_obj_t *object,const char *text) {
    assert(locked && object && text);
    assert(strlen(text)<sizeof(contents[0]));
    strcpy(contents[object-objects],text);
    for (int i=0;i<8;++i) if(strcmp(text,cast_cities[i])==0) ++city_labels;
}
void lv_obj_set_style_text_font(lv_obj_t *o,const lv_font_t *font,int selector) {
    (void)selector; assert(locked && o && font==&cast_font_16);
}
void lv_obj_set_pos(lv_obj_t *o,int a,int b) { (void)a;(void)b;assert(locked && o); }
void lv_obj_set_size(lv_obj_t *o,int a,int b) { (void)a;(void)b;assert(locked && o); }
void lv_obj_set_style_radius(lv_obj_t *o,int v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_border_width(lv_obj_t *o,int v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_text_align(lv_obj_t *o,int v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_text_line_space(lv_obj_t *o,int v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_bg_opa(lv_obj_t *o,int v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_pad_left(lv_obj_t *o,int v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_pad_top(lv_obj_t *o,int v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_bg_color(lv_obj_t *o,uint32_t v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_text_color(lv_obj_t *o,uint32_t v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_set_style_border_color(lv_obj_t *o,uint32_t v,int s) { (void)v;(void)s;assert(locked && o); }
void lv_obj_remove_flag(lv_obj_t *o,int v) { assert(locked && o);if(v==LV_OBJ_FLAG_HIDDEN)hidden[o-objects]=0; }
void lv_obj_add_flag(lv_obj_t *o,int v) { assert(locked && o);if(v==LV_OBJ_FLAG_HIDDEN)hidden[o-objects]=1; }
void lv_label_set_long_mode(lv_obj_t *o,int v) { (void)v;assert(locked && o); }

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; cast_level_t level; size_t city,actor,page; } action_t;
static const action_t actions[]={
    {BSP_BTN_DOWN,BSP_BTN_DOUBLE,CAST_CITY,2,0,0},
    {BSP_BTN_UP,BSP_BTN_DOUBLE,CAST_CITY,0,0,0},
    {BSP_BTN_DOWN,BSP_BTN_LONG,CAST_CITY,2,0,0},
    {BSP_BTN_UP,BSP_BTN_LONG,CAST_CITY,0,0,0},
    {BSP_BTN_OK,BSP_BTN_DOUBLE,CAST_SESSION,0,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_ACTOR,0,0,0},
    {BSP_BTN_DOWN,BSP_BTN_DOUBLE,CAST_ACTOR,0,2,0},
    {BSP_BTN_UP,BSP_BTN_DOUBLE,CAST_ACTOR,0,0,0},
    {BSP_BTN_OK,BSP_BTN_DOUBLE,CAST_READER,0,0,0},
    {BSP_BTN_DOWN,BSP_BTN_DOUBLE,CAST_READER,0,0,1},
    {BSP_BTN_UP,BSP_BTN_DOUBLE,CAST_READER,0,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_ACTOR,0,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_SESSION,0,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_CITY,0,0,0},
    {BSP_BTN_UP,BSP_BTN_CLICK,CAST_CITY,7,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_SESSION,7,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_ACTOR,7,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_ACTOR,7,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_SESSION,7,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_CITY,7,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_SYNC,7,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_SYNC,7,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_CITY,7,0,0},
    {BSP_BTN_DOWN,BSP_BTN_CLICK,CAST_CITY,0,0,0},
#ifdef CONFIG_CAST_WIFI_PORTAL
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_SYNC,0,0,0},
    {BSP_BTN_UP,BSP_BTN_LONG,CAST_SYNC,0,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_SYNC,0,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_SYNC,0,0,0},
    {BSP_BTN_DOWN,BSP_BTN_LONG,CAST_SYNC,0,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_SYNC,0,0,0},
    {BSP_BTN_UP,BSP_BTN_CLICK,CAST_SYNC,0,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_SYNC,0,0,0},
    {BSP_BTN_OK,BSP_BTN_CLICK,CAST_SYNC,0,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_SYNC,0,0,0},
    {BSP_BTN_DOWN,BSP_BTN_LONG,CAST_SYNC,0,0,0},
    {BSP_BTN_OK,BSP_BTN_LONG,CAST_CITY,0,0,0},
#endif
};
static void next_action(void) {
    assert(!locked && labels==14 && brightness>0);
    if (scenario_step) {
        const action_t *previous=&actions[scenario_step-1];
        assert(state.level==previous->level && state.city==previous->city);
        assert(state.actor==previous->actor && state.page==previous->page);
    }
    if (state.level==CAST_CITY) {
        assert(!strcmp(contents[heading-objects],"选择城市"));
        assert(hidden[body-objects]);
        for(unsigned i=0;i<8;++i) assert(!hidden[rows[i]-objects] && !strcmp(contents[rows[i]-objects],cast_cities[i]));
    } else if(state.level==CAST_READER) {
        const cast_actor_t *profile=cast_profile(state.venue,state.actor);
        assert(!hidden[body-objects]);
        assert(!strcmp(contents[body-objects],profile->pages[state.page].text));
        assert(!strcmp(contents[heading-objects],profile->pages[state.page].title));
        for(unsigned i=0;i<8;++i) assert(hidden[rows[i]-objects]);
    } else if(state.level==CAST_ACTOR && state.city==7) {
        assert(!hidden[body-objects] && strstr(contents[body-objects],"阵容待更新"));
        for(unsigned i=0;i<8;++i) assert(hidden[rows[i]-objects]);
    } else if(state.level==CAST_SYNC && scenario_step==22) {
        assert(!syncing && sync_status==CAST_NOT_CONFIGURED);
        assert(strstr(contents[body-objects],"尚未配置服务"));
    }
    if(scenario_step==sizeof(actions)/sizeof(*actions)) {
        scenario_step=0;
        if(++cycles==100) longjmp(reached_loop,1);
    }
#ifdef CONFIG_CAST_WIFI_PORTAL
    if(network_page)assert(!hidden[body-objects]);
    if(network_page && setup_page==SETUP_CLEAR)assert(strstr(contents[body-objects],"清除网络与服务地址"));
#endif
    const action_t *action=&actions[scenario_step++];
    ticks+=400;
    button_callback(action->button,BSP_BTN_PRESS,NULL);
    if(action->event==BSP_BTN_LONG)button_callback(action->button,BSP_BTN_LONG,NULL);
    button_callback(action->button,BSP_BTN_RELEASE,NULL);
    if(action->event==BSP_BTN_DOUBLE){ticks+=20;button_callback(action->button,BSP_BTN_PRESS,NULL);button_callback(action->button,BSP_BTN_RELEASE,NULL);}
}

static void check_callback_boundaries(void) {
    queue_count=send_calls=0;
    on_button(BSP_BTN_UP,BSP_BTN_CLICK,NULL);on_button(BSP_BTN_DOWN,BSP_BTN_DOUBLE,NULL);
    on_button((bsp_btn_t)99,BSP_BTN_RELEASE,NULL);assert(!queue_count);
    for(unsigned i=0;i<13;i++){on_button(BSP_BTN_UP,BSP_BTN_PRESS,NULL);on_button(BSP_BTN_UP,BSP_BTN_RELEASE,NULL);}
    assert(queue_count==12 && send_calls==13);queue_count=0;
    ticks+=400;on_button(BSP_BTN_OK,BSP_BTN_PRESS,NULL);on_button(BSP_BTN_OK,BSP_BTN_RELEASE,NULL);
    on_button(BSP_BTN_OK,BSP_BTN_PRESS,NULL);on_button(BSP_BTN_OK,BSP_BTN_RELEASE,NULL);
    assert(queue_count==1 && queued[0]==CAST_OK);queue_count=0;
    on_button(BSP_BTN_OK,BSP_BTN_PRESS,NULL);on_button(BSP_BTN_OK,BSP_BTN_LONG,NULL);on_button(BSP_BTN_OK,BSP_BTN_RELEASE,NULL);
    assert(queue_count==1 && queued[0]==CAST_BACK);queue_count=0;
}

int main(void) {
    for (fail_at=0;fail_at<=6;++fail_at) {
        brightness=labels=city_labels=0;
        locked=initialized=rendered=buttons_ready=false;
        if (setjmp(reached_loop)==0) {
            app_main();
            assert((fail_at>=1 && fail_at<=4) || fail_at==6);
            assert(brightness==0 && !buttons_ready);
        } else {
            assert(fail_at==0 || fail_at==5);
            assert(brightness>0 && rendered && labels==14);
        }
    }
    check_callback_boundaries();
    fail_at=0; exercise_loop=true;
    brightness=labels=city_labels=0;
    locked=initialized=rendered=buttons_ready=false;
    state=(cast_state_t){0};
    if(setjmp(reached_loop)==0) { app_main(); assert(!"application returned unexpectedly"); }
    assert(cycles==100 && labels==14);
    puts("Cast startup visibility/failure paths: PASS");
    printf("Cast application loop/buttons/view binding: PASS (100 cycles, %zu gestures)\n",100*sizeof(actions)/sizeof(*actions));
#ifdef CONFIG_CAST_WIFI_PORTAL
    assert(setup_calls==100 && cancel_calls==100 && forget_calls==100 && today_calls==100);
    setup_page=SETUP_RESULT;network.state=NET_SAVED;result_generation=network.state_generation;
    setup_key(CAST_OK);assert(setup_page==SETUP_RESULT); /* Previous saved state is not this operation's success. */
    network.state_generation++;network.state=NET_CONNECTING;setup_key(CAST_OK);assert(setup_page==SETUP_RESULT);
    network.state=NET_FAILED;setup_key(CAST_OK);assert(setup_page==SETUP_MENU);
    puts("Wi-Fi UI: PASS (setup entry, explicit clear/cancel, return, clock-unavailable actions; radio boundary mocked)");
#endif
    return 0;
}

