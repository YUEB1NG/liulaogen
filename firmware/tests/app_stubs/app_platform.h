#pragma once
#include "../demo_stubs/demo_test_stubs.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
int xQueueSend(QueueHandle_t q, const void *data, unsigned ticks);
int xQueueReceive(QueueHandle_t q, void *data, unsigned ticks);
TickType_t xTaskGetTickCount(void);
esp_err_t bsp_display_init(void);
bool bsp_lvgl_init(void);
esp_err_t bsp_battery_init(void);
int bsp_battery_soc(void);
esp_err_t bsp_button_init(void (*callback)(bsp_btn_t, bsp_btn_ev_t, void *), void *user);
#define LV_FONT_DECLARE(name) extern const lv_font_t name
#define LV_LABEL_LONG_CLIP 1
#define LV_LABEL_LONG_SCROLL_CIRCULAR 2
#define LV_TEXT_ALIGN_RIGHT 1
#define LV_OPA_COVER 255
#define LV_OBJ_FLAG_SCROLLABLE 1
#define LV_OBJ_FLAG_HIDDEN 2
lv_obj_t *lv_screen_active(void);
void lv_obj_set_pos(lv_obj_t *, int, int);
void lv_obj_set_style_text_line_space(lv_obj_t *, int, int);
void lv_obj_set_style_bg_opa(lv_obj_t *, int, int);
void lv_obj_set_style_pad_left(lv_obj_t *, int, int);
void lv_obj_set_style_pad_top(lv_obj_t *, int, int);
void lv_obj_set_style_border_color(lv_obj_t *, uint32_t, int);
void lv_obj_remove_flag(lv_obj_t *, int);
void lv_obj_add_flag(lv_obj_t *, int);

