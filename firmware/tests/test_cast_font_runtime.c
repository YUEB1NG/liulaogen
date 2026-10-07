/* Use the actual LVGL glyph lookup API with the exact shipped font data. */
#include "lvgl.h"
#include "cast_supported.h"
#include "cast_data.h"
#include "cast_input.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
LV_FONT_DECLARE(cast_font_16);
/* Host memory/draw-buffer boundary only; glyph lookup and bitmap decoder below
 * are the unmodified LVGL 9.5 sources from the locked managed component. */
void lv_memset(void *p,uint8_t value,size_t n) { memset(p,value,n); }
void *lv_memcpy(void *d,const void *s,size_t n) { return memcpy(d,s,n); }
int lv_strcmp(const char *a,const char *b) { return strcmp(a,b); }
void *lv_malloc_zeroed(size_t n) { return calloc(1,n); }
void lv_free(void *p) { free(p); }
void *lv_malloc(size_t n) { return malloc(n); }
void *lv_realloc(void *p,size_t n) { return realloc(p,n); }
size_t lv_strlen(const char *s) { return strlen(s); }
int lv_vsnprintf(char *b,size_t n,const char *fmt,va_list args) { return vsnprintf(b,n,fmt,args); }
void *lv_utils_bsearch(const void *key,const void *base,size_t n,size_t size,int (*cmp)(const void *,const void *)) { return bsearch(key,base,n,size,cmp); }
uint32_t lv_draw_buf_width_to_stride(uint32_t w,lv_color_format_t format) { assert(format==LV_COLOR_FORMAT_A8);return w; }
void lv_draw_buf_flush_cache(const lv_draw_buf_t *buf,const lv_area_t *area) { (void)buf;(void)area; }
int main(void) {
    for(size_t i=0;i<sizeof(cast_supported)/sizeof(*cast_supported);i++) {
        lv_font_glyph_dsc_t dsc;
        assert(lv_font_get_glyph_dsc(&cast_font_16,&dsc,cast_supported[i],0));
        assert(!dsc.is_placeholder && dsc.resolved_font==&cast_font_16);
        if(dsc.box_w && dsc.box_h) {
            dsc.req_raw_bitmap=1;
            const uint8_t *raw=cast_font_16.get_glyph_bitmap(&dsc,NULL);
            assert(raw);
            uint8_t pixels[1024];memset(pixels,0x55,sizeof(pixels));
            size_t size=(size_t)dsc.box_w*dsc.box_h;
            assert(size<sizeof(pixels));
            lv_draw_buf_t draw={.data=pixels};
            dsc.req_raw_bitmap=0;
            assert(cast_font_16.get_glyph_bitmap(&dsc,&draw)==&draw);
            for(size_t k=0;k<size;k++)
                assert(pixels[k]==17*((k%2)?(raw[k/2]&15):(raw[k/2]>>4)));
            assert(pixels[size]==0x55);
        }
    }
    lv_font_glyph_dsc_t missing;
    bool found=lv_font_get_glyph_dsc(&cast_font_16,&missing,0x9f98,0);
    assert(!found || missing.is_placeholder);
    size_t pages=0;
    int max_width=0,max_height=0;
    for(size_t actor=0;actor<28;++actor) {
        const cast_actor_t *a=&cast_actors[actor];
        for(size_t page=0;page<a->page_count;++page) {
            lv_point_t size;
            lv_text_get_size(&size,a->pages[page].text,&cast_font_16,0,0,204,LV_TEXT_FLAG_EXPAND);
            assert(size.x<=204 && size.y<=183); /* body starts at y=85; status at y=268 */
            if(size.x>max_width)max_width=size.x;
            if(size.y>max_height)max_height=size.y;
            lv_text_get_size(&size,a->pages[page].title,&cast_font_16,0,0,140,LV_TEXT_FLAG_EXPAND);
            assert(size.x<=140 && size.y<=27);
            ++pages;
        }
    }
    assert(pages==113 && cast_font_16.line_height+4+2<=34);
    cast_input_t input;cast_input_begin(&input,"WWWWWWWWWWWWWWWWWWWW",160);
    for(unsigned group=0;group<4;group++) {
        size_t first=input.cursor;
        do {
            char text[384];lv_point_t size;cast_input_description(&input,false,text,sizeof(text));
            lv_text_get_size(&size,text,&cast_font_16,0,0,204,LV_TEXT_FLAG_EXPAND);
            assert(size.x<=204 && size.y<=183);
            cast_input_key(&input,CAST_DOWN);
        }while(input.cursor!=first);
        cast_input_key(&input,CAST_LONG_UP);
    }
    puts("On-device text editor layout: PASS (all character groups and widest preview, within 204x183)");
    lv_point_t phone_size;
    lv_text_get_size(&phone_size,"热点名 Passport-FFFF\n密码 ffffffffffffffff\n手机打开最大兼容性\n热点断开，正在重连\nOK 连接手机热点\n长按上键网页配网",&cast_font_16,0,0,204,LV_TEXT_FLAG_EXPAND);
    assert(phone_size.x<=204 && phone_size.y<=183);
    printf("Phone hotspot setup text metrics: PASS (%dx%d within 204x183)\n",phone_size.x,phone_size.y);
    printf("LVGL text metrics: PASS (%zu pages; max body %dx%d within 204x183)\n",pages,max_width,max_height);
    printf("LVGL 9.5 runtime glyph lookup/bitmap decode: PASS (%zu glyphs and missing-glyph control)\n", sizeof(cast_supported)/sizeof(*cast_supported));
    return 0;
}
