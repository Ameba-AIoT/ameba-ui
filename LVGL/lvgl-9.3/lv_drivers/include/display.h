#ifndef UI_LVGL_LV_DRIVERS_DISPLAY
#define UI_LVGL_LV_DRIVERS_DISPLAY

#include <stdint.h>

#include "lvgl.h"

bool display_init(uint16_t bpp, uint32_t width, uint32_t height);
void display_set_lv_display(lv_display_t *display);
void display_get_info(uint32_t *width, uint32_t *height);
uint8_t *display_get_buffer(int buffer_id);
void display_flush_full(lv_display_t * display, const lv_area_t * area, void * px_map);
void display_flush_direct(lv_display_t *display, const lv_area_t *area, void *px_map);

#endif /* UI_LVGL_LV_DRIVERS_DISPLAY */
