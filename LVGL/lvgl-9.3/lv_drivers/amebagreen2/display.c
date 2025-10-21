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

#include <stdlib.h>
#include <string.h>

#include "lcdc.h"
#include "display.h"

#define LOG_TAG "Display"

#define CHECK_FLIP_BUFFER 0

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t *buffers[3];
    lv_display_t *lv_disp;
    int active_buffer_id;
    int rendering_buffer_id;
    lv_thread_sync_t flip_sync;
    volatile bool flip_done;
    lcdc_event_t lcdc_callback;
    bool initialized;
} display_context_t;

static display_context_t display_ctx = {0};

static void display_vblank_handler(void *user_data);

bool display_init(uint16_t bpp, uint32_t width, uint32_t height) {
    if (display_ctx.initialized) {
        RTK_LOGW(LOG_TAG, "Already initialized\n");
        return true;
    }

    // map lvgl format to lcdc format
    lcd_format_t format;
    switch (bpp) {
        case 16:  format = LCD_FORMAT_RGB565; break;
        case 24:  format = LCD_FORMAT_RGB888; break;
        case 32:  format = LCD_FORMAT_ARGB8888; break;
        default:
            RTK_LOGE(LOG_TAG, "Unsupported color depth: %d\n", bpp);
            return false;
    }

    // save resolution
    display_ctx.width = width;
    display_ctx.height = height;

    // allocate double framebuffer
    size_t buffer_size = display_ctx.width * display_ctx.height * bpp / 8;
    display_ctx.buffers[0] = malloc(buffer_size);
    display_ctx.buffers[1] = malloc(buffer_size);
    if (!display_ctx.buffers[0] || !display_ctx.buffers[1]) {
        RTK_LOGE(LOG_TAG, "Alloc display buffer fail\n");
        goto err_finish;
    }

    memset(display_ctx.buffers[0], 0, buffer_size);
    memset(display_ctx.buffers[1], 0, buffer_size);

    RTK_LOGI(LOG_TAG, "buf1: 0x%x, buf2: 0x%x\n",
             (int)display_ctx.buffers[0], (int)display_ctx.buffers[1]);

    // vsync semphore
    lv_thread_sync_init(&display_ctx.flip_sync);

    // init lcdc with default resolution
    if (!lcdc_init_default(format, width, height)) {
        RTK_LOGE(LOG_TAG, "Failed to initialize LCD controller\n");
        goto err_finish;
    }

    // set vsync callback
    display_ctx.lcdc_callback.vblank_handler = display_vblank_handler;
    lcdc_register_event_callback(&display_ctx.lcdc_callback, &display_ctx);

    // drawing buffer is 0
    display_ctx.active_buffer_id = 1;
    display_ctx.rendering_buffer_id = 0;
    display_ctx.flip_done = true;
    display_ctx.initialized = true;

    RTK_LOGI(LOG_TAG, "Display manager initialized: %dx%d, buffer size: %zu\n", 
             display_ctx.width, display_ctx.height, buffer_size);

    return true;

err_finish:
    if (display_ctx.buffers[0])
        free(display_ctx.buffers[0]);

    if (display_ctx.buffers[1])
        free(display_ctx.buffers[1]);

    lv_thread_sync_delete(&display_ctx.flip_sync);

    memset(&display_ctx, 0, sizeof(display_context_t));

    return false;
}

void display_set_lv_display(lv_display_t *display) {
    display_ctx.lv_disp = display;
}

void display_get_info(uint32_t *width, uint32_t *height) {
    if (width) *width = display_ctx.width;
    if (height) *height = display_ctx.height;
}

uint8_t *display_get_buffer(int buffer_id) {
    if (buffer_id >= 0 && buffer_id < 2) {
        return display_ctx.buffers[buffer_id];
    }
    return NULL;
}

static void display_vblank_handler(void *user_data) {
    display_context_t *disp = (display_context_t *)user_data;
    if (!disp->flip_done) {
        lv_thread_sync_signal(&display_ctx.flip_sync);
        disp->flip_done = true;
    }
}

void display_flush_direct(lv_display_t *display, const lv_area_t *area, void *px_map) {
    bool last_flush = lv_display_flush_is_last(display);

    static int flush_count = 0;
    RTK_LOGD(LOG_TAG, "Flush #%d %s: Buffer=%p, Area=(%d,%d)-(%d,%d)\n",
             flush_count++, last_flush ? "last" : "int",
             px_map, area->x1, area->y1, area->x2, area->y2);

    if (last_flush) {
        // send new rendering buffer to lcdc
        lcdc_page_flip(px_map);

        // wait flip done
        display_ctx.flip_done = false;
        lv_thread_sync_wait(&display_ctx.flip_sync);
        display_ctx.flip_done = true;
    }

    lv_display_flush_ready(display);
}

void display_flush_full(lv_display_t *display, const lv_area_t *area, void *px_map) {
#if CHECK_FLIP_BUFFER
    // check if px_map is same as our drawing buffer
    if (px_map != display_ctx.buffers[display_ctx.rendering_buffer_id]) {
        RTK_LOGW(LOG_TAG, "Warning: px_map doesn't match active buffer\n");
        return;
    }
#endif

    bool last_flush = lv_display_flush_is_last(display);

    static int flush_count = 0;
    RTK_LOGD(LOG_TAG, "Flush #%d %s: Buffer=%p, Area=(%d,%d)-(%d,%d)\n",
             flush_count++, last_flush ? "last" : "int",
             px_map, area->x1, area->y1, area->x2, area->y2);

    // send new rendering buffer to lcdc
    lcdc_page_flip(px_map);

    // wait flip done
    display_ctx.flip_done = false;
    lv_thread_sync_wait(&display_ctx.flip_sync);
    display_ctx.flip_done = true;

#if CHECK_FLIP_BUFFER
    // swap framebuffer index
    int old_active = display_ctx.active_buffer_id;
    display_ctx.active_buffer_id = display_ctx.rendering_buffer_id;
    display_ctx.rendering_buffer_id = old_active;
#endif

    lv_display_flush_ready(display);
}
