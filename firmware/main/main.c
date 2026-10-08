/* Offline cast handbook. The application task alone owns state and UI. */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "lvgl.h"
#include "cast_state.h"
#include "cast_sync.h"
#include "sdkconfig.h"
#ifdef CONFIG_CAST_WIFI_PORTAL
#include "cast_network.h"
#include "cast_input.h"
#include "cast_remote.h"
static cast_net_view_t network;
static bool network_page;
static cast_remote_view_t remote;
typedef enum { SETUP_MENU,SETUP_SCAN,SETUP_LIST,SETUP_SSID,SETUP_PASSWORD,SETUP_ORIGIN,SETUP_PAIR,SETUP_CLEAR,SETUP_RESULT } setup_page_t;
static setup_page_t setup_page;
static unsigned setup_selection;
static uint32_t scan_generation,result_generation;
static bool result_service;
static cast_input_t input;
static char selected_ssid[33];
static const char *setup_message="";
static cast_profile_online_t *profile;
static bool profile_busy;
#endif
#include <stdlib.h>
#include <string.h>
static cast_sync_status_t sync_status=CAST_OFFLINE;
static cast_snapshot_t *shown;
static bool syncing, sync_ready;
static char request_date[11];
static const char *status_text(void) {
    switch(sync_status) {
    case CAST_UNPUBLISHED:return "未发布";
    case CAST_LATEST:return "已验证更新";
    case CAST_INVALID:return "校验失败";
    case CAST_SAVE_FAILED:return "保存失败";
    case CAST_BUSY:return "正在同步";
    case CAST_NOT_CONFIGURED:return "尚未配置服务";
    case CAST_CLOCK_WAIT:return "等待网络校时";
    case CAST_CACHED:return "已打开离线名单";
    default:return "离线";
    }
}

LV_FONT_DECLARE(cast_font_16);
static QueueHandle_t keys;
static cast_state_t state;
static lv_obj_t *heading, *context, *battery, *rows[8], *body, *footer, *status;
static const char *TAG = "cast";
#ifdef CONFIG_CAST_WIFI_PORTAL
static void setup_scan(void) {
    scan_generation=network.scan_generation;setup_selection=0;
    if(cast_network_setup())setup_page=SETUP_SCAN;else setup_message="设备忙，请稍后重试";
}
static void setup_key(cast_key_t key) {
    if(setup_page==SETUP_PASSWORD || setup_page==SETUP_SSID || setup_page==SETUP_ORIGIN) {
        cast_input_result_t result=cast_input_key(&input,key);
        if(result==INPUT_CANCELLED) {setup_page=SETUP_MENU;setup_selection=0;setup_message="";return;}
        if(result!=INPUT_DONE)return;
        if(setup_page==SETUP_SSID) {
            if(!input.length || input.length>32) {setup_message="名称不能为空";return;}
            strcpy(selected_ssid,input.value);cast_input_begin(&input,"",63);setup_page=SETUP_PASSWORD;setup_message="";
        } else if(setup_page==SETUP_PASSWORD) {
            cast_network_view(&network);result_generation=network.state_generation;result_service=false;
            if(!cast_network_connect(selected_ssid,input.value)) {setup_message="密码需八至六十三位";return;}
            memset(&input,0,sizeof(input));setup_page=SETUP_RESULT;setup_message="正在连接，请稍候";
        } else {
            cast_network_view(&network);result_generation=network.state_generation;result_service=true;
            if(!cast_network_set_origin(input.value)) {setup_message="服务地址格式无效";return;}
            memset(&input,0,sizeof(input));setup_page=SETUP_RESULT;setup_message="正在保存服务地址";
        }
        return;
    }
    if(key==CAST_BACK) {
        if(setup_page==SETUP_MENU) {if(cast_network_cancel())network_page=false;}
        else {cast_network_cancel();setup_page=SETUP_MENU;setup_selection=0;setup_message="";}
        return;
    }
    if(setup_page==SETUP_MENU) {
        if(key==CAST_UP)setup_selection=(setup_selection+4)%5;
        if(key==CAST_DOWN)setup_selection=(setup_selection+1)%5;
        if(key==CAST_OK)switch(setup_selection) {
            case 0:setup_scan();break;
            case 1:cast_input_begin(&input,"",32);setup_page=SETUP_SSID;setup_message="";break;
            case 2:{char origin[161]="";cast_network_origin(origin,sizeof(origin));cast_input_begin(&input,origin,160);setup_page=SETUP_ORIGIN;setup_message="";break;}
            case 3:setup_page=SETUP_PAIR;cast_remote_pair();break;
            case 4:setup_page=SETUP_CLEAR;break;
        }
    } else if(setup_page==SETUP_LIST) {
        size_t n=network.count+1;
        if(key==CAST_UP)setup_selection=(unsigned)((setup_selection+n-1)%n);
        if(key==CAST_DOWN)setup_selection=(unsigned)((setup_selection+1)%n);
        if(key==CAST_OK) {
            if(setup_selection==network.count)setup_scan();
            else {strcpy(selected_ssid,network.access[setup_selection].ssid);cast_input_begin(&input,"",63);setup_page=SETUP_PASSWORD;setup_message="";}
        }
    } else if(setup_page==SETUP_CLEAR && key==CAST_OK) {
        if(cast_network_forget()) {setup_page=SETUP_MENU;setup_selection=0;setup_message="已提交清除请求";}
    } else if(setup_page==SETUP_PAIR && key==CAST_OK)cast_remote_pair();
    else if(setup_page==SETUP_RESULT && key==CAST_OK) {
        if(network.state_generation==result_generation || network.state==NET_CONNECTING)return;
        setup_page=SETUP_MENU;setup_selection=0;setup_message="";
    }
}
static const char *safe_ssid(const char *ssid) {return cast_text_supported(ssid)?ssid:"名称含字库外字符";}
static void setup_render(void) {
    char b[384]="",detail[100]="";
    const char *title="网络设置";
    if(setup_page==SETUP_MENU) {
        const char *items[]={"选择 Wi-Fi","手动输入名称","更新服务地址","网站配对","清除网络配置"};
        for(unsigned i=0;i<5;i++)snprintf(b+strlen(b),sizeof(b)-strlen(b),"%s %s\n",setup_selection==i?">":" ",items[i]);
    } else if(setup_page==SETUP_SCAN)strcpy(b,"正在扫描 Wi-Fi\n请稍候\n\n长按确定取消");
    else if(setup_page==SETUP_LIST) {
        title="选择 Wi-Fi";
        if(setup_selection<network.count)snprintf(detail,sizeof(detail),"%s",safe_ssid(network.access[setup_selection].ssid));
        size_t start=setup_selection/4*4;
        for(size_t i=start;i<network.count+1 && i<start+4;i++) {
            char name[40];const char *s=i==network.count?"重新扫描":safe_ssid(network.access[i].ssid);
            /* Truncate only at UTF-8 boundaries; full selected name scrolls above. */
            size_t end=0,chars=0;while(s[end] && chars<10) {end++;while(((unsigned char)s[end]&0xc0)==0x80)end++;chars++;}
            memcpy(name,s,end);name[end]=0;
            snprintf(b+strlen(b),sizeof(b)-strlen(b),"%s %s\n",setup_selection==i?">":" ",name);
        }
        if(!network.count)snprintf(detail,sizeof(detail),"%s",network.scan_failed?"扫描失败，请重试":"没有发现网络");
        else if(setup_selection<network.count)snprintf(b+strlen(b),sizeof(b)-strlen(b),"\n信号 %d dBm %s",network.access[setup_selection].rssi,network.access[setup_selection].secure?"加密":"开放");
    } else if(setup_page==SETUP_PASSWORD || setup_page==SETUP_SSID || setup_page==SETUP_ORIGIN) {
        title=setup_page==SETUP_PASSWORD?"Wi-Fi 密码":setup_page==SETUP_SSID?"网络名称":"服务地址";
        cast_input_description(&input,setup_page==SETUP_PASSWORD,b,sizeof(b));
        snprintf(detail,sizeof(detail),"%s",setup_page==SETUP_PASSWORD?safe_ssid(selected_ssid):setup_page==SETUP_ORIGIN?"服务根地址，公网 HTTPS":"隐藏网络支持英文名称");
    } else if(setup_page==SETUP_PAIR) {
        title="网站配对";
        if(remote.state==REMOTE_PAIRING)snprintf(b,sizeof(b),"配对码 %s\n\n手机网站打开设备管理\n输入配对码添加设备\n配对码五分钟内有效",remote.code);
        else if(remote.state==REMOTE_PAIRED)strcpy(b,"网站已识别本机\n在手机网页选择名单\n点击上传到设备\n收到并保存后显示成功");
        else strcpy(b,remote.state==REMOTE_UNCONFIGURED?"请先填写服务地址":remote.state==REMOTE_WAIT_CLOCK?"等待网络校时":remote.state==REMOTE_OFFLINE?"请先连接 Wi-Fi":remote.state==REMOTE_READY?"OK 获取网站配对码":remote.state==REMOTE_ERROR?"网站连接失败\nOK 重试配对":"正在联系网站\nOK 重新申请配对");
    } else if(setup_page==SETUP_CLEAR)strcpy(b,"清除网络与服务地址？\n\nOK 确认清除\n长按确定取消\n\n保留已存名单");
    else strcpy(b,network.state_generation==result_generation?setup_message:network.state==NET_SAVED?(result_service?"服务地址已保存":network.connected?"连接成功，已保存":"配置已保存"):network.state==NET_FAILED?"连接失败，请重试":network.state==NET_STORAGE_FAILED?"保存失败，请重试":setup_message);
    for(unsigned i=0;i<8;i++)lv_obj_add_flag(rows[i],LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(body,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(heading,title);lv_label_set_text(context,detail);lv_label_set_text(body,b);
    lv_label_set_text(status,*setup_message && setup_page!=SETUP_RESULT?setup_message:network.connected?"Wi-Fi 已连接":"Wi-Fi 未连接");
    lv_label_set_text(footer,"OK 选择 长按确定返回");
}
#endif

/* Runs on shared esp_timer task: strictly bounded, no LVGL or I2C. */
static void on_button(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    static bool held[3],long_sent[3],ok_recent;
    static TickType_t last_ok;
    if((unsigned)button>=3)return;
    if(event==BSP_BTN_PRESS){held[button]=true;long_sent[button]=false;return;}
    if(event==BSP_BTN_LONG){long_sent[button]=true;}
    else if(event==BSP_BTN_RELEASE){
        if(!held[button])return;
        held[button]=false;
        if(long_sent[button])return;
        if(button==BSP_BTN_OK){
            TickType_t now=xTaskGetTickCount();
            if(ok_recent && (TickType_t)(now-last_ok)<pdMS_TO_TICKS(180))return;
            last_ok=now;ok_recent=true;
        }
    }else return;
    cast_key_t key;
    if (event == BSP_BTN_LONG && button == BSP_BTN_OK) key = CAST_BACK;
    else if(event==BSP_BTN_LONG && button==BSP_BTN_UP) key=CAST_LONG_UP;
    else if(event==BSP_BTN_LONG && button==BSP_BTN_DOWN) key=CAST_LONG_DOWN;
    else if (event == BSP_BTN_RELEASE) {
        if (button == BSP_BTN_UP) key = CAST_UP;
        else if (button == BSP_BTN_DOWN) key = CAST_DOWN;
        else if (button == BSP_BTN_OK) key = CAST_OK;
        else return;
    } else return;
    /* Each release navigates immediately; a long press never adds a click. */
    xQueueSend(keys,&key,0);
}
static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_text_font(o, &cast_font_16, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(0xEEF0F3), 0);
    lv_obj_set_style_text_line_space(o, 0, 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_label_set_text(o, "");
    return o;
}
static void create_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x202122), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    heading = label(screen, 18, 16, 140, 27);
    battery = label(screen, 167, 16, 55, 27);
    lv_obj_set_style_text_align(battery, LV_TEXT_ALIGN_RIGHT, 0);
    context = label(screen, 18, 45, 204, 32);
    lv_label_set_long_mode(context, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(context, lv_color_hex(0xA9B2C0), 0);
    for (int i = 0; i < 8; ++i) {
        rows[i] = label(screen, 12, 80 + i * 38, 216, 34);
        lv_obj_set_style_bg_opa(rows[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(rows[i], 6, 0);
        lv_obj_set_style_pad_left(rows[i], 8, 0);
        lv_obj_set_style_pad_top(rows[i], 4, 0);
        lv_obj_set_style_border_width(rows[i], 1, 0);
        lv_obj_set_style_border_color(rows[i], lv_color_hex(0x343A43), 0);
    }
    body = label(screen, 12, 85, 216, 183);
    status = label(screen,18,268,204,24);
    footer = label(screen,18,292,204,24);
    lv_obj_set_style_text_color(footer, lv_color_hex(0xA9B2C0), 0);
}
static void render(void)
{
#ifdef CONFIG_CAST_WIFI_PORTAL
    if(network_page && state.level==CAST_SYNC) {setup_render();return;}
#endif
    char text[256]; size_t count=0, selected=0;
    bool city=state.level==CAST_CITY, reader=state.level==CAST_READER, sync=state.level==CAST_SYNC;
    const char *title=city?"选择城市":state.level==CAST_SESSION?"选择场次":"演出阵容";
    if(city) {count=cast_city_count();selected=state.city;}
    else if(state.level==CAST_SESSION) {
        count=cast_session_count(state.city);
        for(size_t i=0;i<count;i++) if(cast_session_index(state.city,i)==state.venue) selected=i;
    } else if(state.level==CAST_ACTOR) {count=cast_group_count(state.venue);selected=state.actor;}
    if(reader) {
        const cast_actor_t *a=cast_profile(state.venue,state.actor);
        title=a->pages[state.page].title;lv_label_set_text(body,a->pages[state.page].text);
        snprintf(text,sizeof(text),"%s",cast_group_name(state.venue,state.actor));
    } else if(sync) {
        title="在线更新";
        snprintf(text,sizeof(text),"%s",request_date);
        char b[256];snprintf(b,sizeof(b),"%s\n上下调整日期\nOK 立即检查更新\n\n发布时段 18:00-20:00\n无自动轮询",status_text());lv_label_set_text(body,b);
#ifdef CONFIG_CAST_WIFI_PORTAL
        snprintf(b,sizeof(b),"%s\n%s\n上下调整日期\nOK 立即检查更新\n长按上键配置网络\n长按下键使用今日",status_text(),network.connected?"网络已连接":"网络未连接");
        lv_label_set_text(body,b);
#endif
    } else if(city) snprintf(text,sizeof(text),"%s",cast_display_date());
    else snprintf(text,sizeof(text),"%s %s",cast_city_name(state.city),state.level==CAST_ACTOR?cast_session_label(state.venue):cast_display_date());
    lv_label_set_text(heading,title);lv_label_set_text(context,text);
    bool empty=!city && !sync && !reader && !count;
    if(reader || sync || empty) lv_obj_remove_flag(body,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(body,LV_OBJ_FLAG_HIDDEN);
    if(empty) lv_label_set_text(body,"阵容待更新\n该城市名单尚未提供");
    size_t start=city?selected/8*8:selected/5*5;
    for(size_t i=0;i<8;i++) {
        size_t index=start+i;
        if(reader || sync || index>=count || (!city && i>=5)) {lv_obj_add_flag(rows[i],LV_OBJ_FLAG_HIDDEN);continue;}
        lv_obj_remove_flag(rows[i],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(rows[i],city?12+(i%2)*110:12,city?80+(i/2)*45:80+i*36);
        lv_obj_set_size(rows[i],city?106:216,city?39:34);
        const char *name;
        if(city) name=cast_city_name(index);
        else if(state.level==CAST_SESSION) {
            name=cast_session_label(cast_session_index(state.city,index));if(!*name)name="阵容待更新";
        } else name=cast_group_name(state.venue,index);
        lv_label_set_text(rows[i],name);
        lv_label_set_long_mode(rows[i],index==selected?LV_LABEL_LONG_SCROLL_CIRCULAR:LV_LABEL_LONG_CLIP);
        lv_obj_set_style_bg_color(rows[i],lv_color_hex(index==selected?0x1D3556:0x2B2D30),0);
        lv_obj_set_style_border_color(rows[i],lv_color_hex(index==selected?0x397FFC:0x383A3E),0);
    }
    if(sync) snprintf(text,sizeof(text),"名单日期 %s",request_date);
    else if(reader) snprintf(text,sizeof(text),"%u/%u 仅供历史参考",(unsigned)state.page+1,(unsigned)cast_profile(state.venue,state.actor)->page_count);
    else if(state.level==CAST_ACTOR) snprintf(text,sizeof(text),"%u/%u组 %s",(unsigned)(count?selected+1:0),(unsigned)count,shown?"已存本机":"历史名单");
    else snprintf(text,sizeof(text),"%s %s",shown?"已存本机":"历史名单",cast_display_date());
#ifdef CONFIG_CAST_WIFI_PORTAL
    if(reader) {
        cast_profile_key_t key;
        bool downloaded=profile && cast_current_profile_key(state.venue,state.actor,&key) && cast_profile_key_equal(&key,&profile->key);
        snprintf(text,sizeof(text),"%u/%u %s",(unsigned)state.page+1,(unsigned)cast_profile(state.venue,state.actor)->page_count,
                 downloaded?"已存在线资料":profile_busy?"资料更新中":"历史资料或待补充");
    }
#endif
    lv_label_set_text(status,text);
    lv_label_set_text(footer,city?"OK进入 长按确定更新":reader?"上/下翻页 长按返回":"OK进入 长按确定返回");
}
void app_main(void)
{
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed"); return;
    }
    bool battery_ok = bsp_battery_init() == ESP_OK;
    keys = xQueueCreate(12, sizeof(cast_key_t));
    if (!keys) { ESP_LOGE(TAG, "Input queue allocation failed"); return; }
    sync_ready=cast_sync_init();
    shown=cast_sync_cached();
    if(shown)cast_use_snapshot(shown);
    snprintf(request_date,sizeof(request_date),"%s",cast_display_date());
    /* app_main remains the sole consumer; no extra application task/stack. */
    if (!bsp_lvgl_lock(-1)) return;
    create_ui(); render();
    lv_refr_now(NULL);
    esp_err_t first_frame=bsp_display_flush_wait();
    bsp_lvgl_unlock();
    if(first_frame!=ESP_OK){ESP_LOGE(TAG,"Initial frame transfer failed");return;}
    /* The BSP initializes PWM at zero; make the prepared page visible. */
    bsp_display_backlight(100);
    ESP_LOGI(TAG, "Cast UI ready; backlight enabled");
    esp_err_t err = bsp_button_init(on_button, NULL);
    if (err != ESP_OK) ESP_LOGE(TAG, "Button initialization failed: %s", esp_err_to_name(err));
    TickType_t last_battery = xTaskGetTickCount() - pdMS_TO_TICKS(30000);
    int soc = -1;
    for (;;) {
        cast_key_t key;
        bool input = xQueueReceive(keys, &key, pdMS_TO_TICKS(40)) == pdTRUE;
        bool battery_due = (TickType_t)(xTaskGetTickCount() - last_battery) >= pdMS_TO_TICKS(30000);
        bool refresh=battery_due;
#ifdef CONFIG_CAST_WIFI_PORTAL
        cast_net_view_t current;cast_network_view(&current);
        if(memcmp(&network,&current,sizeof(current))) {network=current;refresh=true;}
        if(network_page && setup_page==SETUP_SCAN && network.scan_generation!=scan_generation) {setup_page=SETUP_LIST;setup_selection=0;refresh=true;}
        cast_remote_view_t remote_current;cast_remote_view(&remote_current);
        if(memcmp(&remote,&remote_current,sizeof(remote))) {remote=remote_current;refresh=true;}
        cast_profile_online_t *arrived=NULL;
        if(cast_sync_profile_poll(&arrived)) {
            profile_busy=false;free(profile);profile=arrived;cast_use_profile(profile);
            if(state.level==CAST_READER && state.page>=cast_profile(state.venue,state.actor)->page_count)state.page=0;
            refresh=true;
        }
#endif
        if (battery_due) { soc = battery_ok ? bsp_battery_soc() : -1; last_battery = xTaskGetTickCount(); }
        cast_sync_status_t result;cast_snapshot_t *next=NULL;
        if(cast_sync_poll(&result,&next)) {
            bool boot_restore=result==CAST_CACHED && !syncing && state.level==CAST_CITY;
            syncing=false;sync_status=result;refresh=true;
            if(next) {
#ifdef CONFIG_CAST_WIFI_PORTAL
                /* A different service can reuse dates/revisions/member IDs. */
                cast_use_profile(NULL);free(profile);profile=NULL;
#endif
                free(shown);shown=next;cast_use_snapshot(shown);state=(cast_state_t){.level=boot_restore?CAST_CITY:CAST_SYNC};
                if(boot_restore)snprintf(request_date,sizeof(request_date),"%s",shown->date);
            }
        }
        if(input) {
#ifdef CONFIG_CAST_WIFI_PORTAL
            if(state.level==CAST_ACTOR && key==CAST_OK && !profile_busy) {
                cast_profile_key_t key;
                if(cast_current_profile_key(state.venue,state.actor,&key))profile_busy=cast_sync_profile_request(&key);
            }
            if(state.level==CAST_SYNC && network_page) {
                setup_key(key);
            } else if(state.level==CAST_SYNC && key==CAST_LONG_UP && !syncing && !profile_busy) {network_page=true;setup_page=SETUP_MENU;setup_selection=0;setup_message="";}
            else if(state.level==CAST_SYNC && key==CAST_LONG_DOWN && !syncing) {if(!cast_network_today(request_date)) sync_status=CAST_CLOCK_WAIT;}
            else
#endif
            if(state.level==CAST_SYNC && key==CAST_OK && !syncing) {
                syncing=sync_ready && cast_sync_request(request_date);
                sync_status=syncing?CAST_BUSY:CAST_OFFLINE;
            } else if(state.level==CAST_SYNC && !syncing && (key==CAST_UP || key==CAST_DOWN)) {
                cast_date_step(request_date,key==CAST_UP?-1:1);
            } else cast_navigate(&state,key);
        }
        if ((input || refresh) && bsp_lvgl_lock(-1)) {
            char value[16];
            if (soc < 0) snprintf(value, sizeof(value), "--%%");
            else snprintf(value, sizeof(value), "%d%%", soc);
            lv_label_set_text(battery, value);
            render();
            if (err != ESP_OK) lv_label_set_text(footer, "按键初始化失败");
            bsp_lvgl_unlock();
        }
    }
}
