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

#include "os_wrapper.h"

#include "lvgl.h"
#include "display.h"
#include "jpeg_decoder.h"

#include "lv_ameba_hal.h"

#define RTK_HW_JPEG_DECODE 0

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

uint32_t ameba_tick_get(void) {
    return rtos_time_get_current_system_time_ms();
}

void lv_ameba_hal_init(void) {
#if RTK_HW_JPEG_DECODE
    lv_ameba_jpeg_init();
#endif

    lv_tick_set_cb(ameba_tick_get);

    display_init(LV_COLOR_DEPTH, SCREEN_WIDTH, SCREEN_HEIGHT);

    uint8_t *buf1 = display_get_buffer(0);
    uint8_t *buf2 = display_get_buffer(1);

    lv_display_t *display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    display_set_lv_display(display);

    lv_display_set_buffers(display, buf1, buf2, SCREEN_WIDTH * SCREEN_HEIGHT * LV_COLOR_DEPTH, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, display_flush_direct);
    //lv_display_set_buffers(display, buf1, buf2, SCREEN_WIDTH * SCREEN_HEIGHT * LV_COLOR_DEPTH, LV_DISPLAY_RENDER_MODE_FULL);
    //lv_display_set_flush_cb(display, display_flush_full);
}
