/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
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

#ifndef AMEBA_UI_DISPLAYER_CONTROLLER_LCDC_RGB_H
#define AMEBA_UI_DISPLAYER_CONTROLLER_LCDC_RGB_H

#include <stdint.h>
#include <stdbool.h>

#include "panel_manager.h"

#include "display_controller.h"

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
    uint32_t clock_frequency;
} lcd_timing_t;

bool lcdc_rgb_controller_init(int32_t color_depth, panel_dev_t *panel);
void lcdc_rgb_do_page_flip(uint8_t *buffer);
void lcdc_rgb_register_vblank_callback(display_driver_callback_t *event);

#endif // AMEBA_UI_DISPLAYER_CONTROLLER_LCDC_RGB_H