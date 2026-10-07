/* These lifecycle tests do not render pages. Resolve their unused rendering
 * references on PE/COFF linkers, which check them before section collection.
 * Any accidental execution fails instead of pretending to render a screen. */
#include "demo_test_stubs.h"
#include "ui_pixel.h"
#include <stdlib.h>

#define UNUSED_UI() abort()
uint32_t lv_color_hex(uint32_t c) { (void)c; UNUSED_UI(); }
lv_obj_t *lv_obj_create(lv_obj_t *p) { (void)p; UNUSED_UI(); }
lv_obj_t *lv_label_create(lv_obj_t *p) { (void)p; UNUSED_UI(); }
void lv_obj_set_size(lv_obj_t *o,int w,int h) { (void)o;(void)w;(void)h;UNUSED_UI(); }
void lv_obj_set_width(lv_obj_t *o,int w) { (void)o;(void)w;UNUSED_UI(); }
void lv_obj_set_style_radius(lv_obj_t *o,int v,int s) { (void)o;(void)v;(void)s;UNUSED_UI(); }
void lv_obj_set_style_bg_color(lv_obj_t *o,uint32_t v,int s) { (void)o;(void)v;(void)s;UNUSED_UI(); }
void lv_obj_set_style_border_width(lv_obj_t *o,int v,int s) { (void)o;(void)v;(void)s;UNUSED_UI(); }
void lv_obj_set_style_text_color(lv_obj_t *o,uint32_t v,int s) { (void)o;(void)v;(void)s;UNUSED_UI(); }
void lv_obj_set_style_text_align(lv_obj_t *o,int v,int s) { (void)o;(void)v;(void)s;UNUSED_UI(); }
void lv_obj_set_style_text_font(lv_obj_t *o,const lv_font_t *v,int s) { (void)o;(void)v;(void)s;UNUSED_UI(); }
void lv_obj_center(lv_obj_t *o) { (void)o;UNUSED_UI(); }
void lv_obj_align(lv_obj_t *o,int a,int x,int y) { (void)o;(void)a;(void)x;(void)y;UNUSED_UI(); }
void lv_obj_delete(lv_obj_t *o) { (void)o;UNUSED_UI(); }
void lv_label_set_text_fmt(lv_obj_t *o,const char *f,...) { (void)o;(void)f;UNUSED_UI(); }
void lv_label_set_long_mode(lv_obj_t *o,int m) { (void)o;(void)m;UNUSED_UI(); }
void lv_screen_load(lv_obj_t *o) { (void)o;UNUSED_UI(); }
lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *),unsigned p,void *d) { (void)cb;(void)p;(void)d;UNUSED_UI(); }
void lv_timer_delete(lv_timer_t *t) { (void)t;UNUSED_UI(); }
lv_obj_t *ui_pixel_screen_create(const char *t) { (void)t;UNUSED_UI(); }
lv_obj_t *ui_pixel_panel_create(lv_obj_t *p,int x,int y,int w,int h,uint32_t c) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)c;UNUSED_UI(); }
lv_obj_t *ui_pixel_label(lv_obj_t *p,const char *t,const lv_font_t *f,uint32_t c) { (void)p;(void)t;(void)f;(void)c;UNUSED_UI(); }
lv_obj_t *ui_pixel_mascot_create(lv_obj_t *p,int x,int y) { (void)p;(void)x;(void)y;UNUSED_UI(); }
int esp_sleep_get_wakeup_cause(void) { UNUSED_UI(); }
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *n) { (void)n;UNUSED_UI(); }
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *n,wifi_ap_record_t *r) { (void)n;(void)r;UNUSED_UI(); }
