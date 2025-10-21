/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * All rights reserved.
 *
 * Licensed under the Realtek License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License from Realtek
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AMEBA_UI_LVGL_LV_DRIVERS_LCDC
#define AMEBA_UI_LVGL_LV_DRIVERS_LCDC

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LCD_FORMAT_RGB565,
    LCD_FORMAT_RGB888,
    LCD_FORMAT_ARGB8888
} lcd_format_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t hsync_front_porch;
    uint32_t hsync_back_porch;
    uint32_t hsync_pulse_width;
    uint32_t vsync_front_porch;
    uint32_t vsync_back_porch;
    uint32_t vsync_pulse_width;
    uint32_t clock_frequency;  // Hz
} lcd_timing_t;

typedef struct {
    void (*vblank_handler)(void *user_data);
    void (*page_flip_handler)(void *user_data);
} lcdc_event_t;

bool lcdc_init(lcd_format_t format, const lcd_timing_t *timing);
bool lcdc_init_default(lcd_format_t format, uint32_t width, uint32_t height);
void lcdc_deinit(void);

void lcdc_get_resolution(uint32_t *width, uint32_t *height);
bool lcdc_set_timing(const lcd_timing_t *timing);

void lcdc_page_flip(uint8_t *buffer);
void lcdc_register_event_callback(lcdc_event_t *callback, void *user_data);

#endif // AMEBA_UI_LVGL_LV_DRIVERS_LCDC
