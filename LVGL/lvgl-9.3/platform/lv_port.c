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

#include <stdlib.h>
#include <string.h>

#include "ameba_soc.h"
#include "os_wrapper.h"

#include "lvgl.h"
#include "lv_ameba_hal.h"
#include "lv_port.h"

#include "display_mode_setting.h"

#ifdef CONFIG_AMEBASMART
#include "lv_port_touch.h"
#endif

#define LOG_TAG "LV-Port"

typedef struct {
    lv_display_t *disp;
    uint8_t *buf1;
    uint8_t *buf2;
    uint8_t *rot_buf[2];
    uint8_t color_depth;
    uint16_t rotation;
    uint16_t phys_width;
    uint16_t phys_height;
    uint16_t disp_width;
    uint16_t disp_height;
    uint8_t flip_index;
    bool is_running;

    /* Legacy path */
    lv_thread_sync_t    flip_sync;
    volatile bool       flip_done;

} lvgl_ctx_t;

static lvgl_ctx_t *s_ctx = NULL;
static lv_port_demo_fn_t s_demo_fn = NULL;
static uint16_t s_demo_rotation = 0;

static inline uint32_t ameba_get_tick(void) {
    return rtos_time_get_current_system_time_ms();
}

static void display_vblank_handler(void) {
    if (!s_ctx->flip_done) {
        lv_thread_sync_signal_isr(&s_ctx->flip_sync);
        s_ctx->flip_done = true;
    }
}

static void display_init(void) {
    display_mode_init(LV_COLOR_DEPTH);

    static display_mode_callback_t s_callback = {
        .vblank_handler = display_vblank_handler
    };
    display_mode_set_callback(&s_callback);
}

static void gfx_init(void) {
    lv_ameba_hal_init();
}

static void fs_init(void) {
#ifdef RTK_ROMFS_ENABLE
    lv_fs_romfs_init();
#endif
}

static size_t get_buf_size(void) {
    return s_ctx->phys_width * s_ctx->phys_height * (s_ctx->color_depth / 8);
}

static void get_disp_size(void) {
    if (s_ctx->rotation == 90 || s_ctx->rotation == 270) {
        s_ctx->disp_width = s_ctx->phys_height;
        s_ctx->disp_height = s_ctx->phys_width;
    } else {
        s_ctx->disp_width = s_ctx->phys_width;
        s_ctx->disp_height = s_ctx->phys_height;
    }
}

static void rotate_90(uint8_t *src, uint8_t *dst,
                      uint16_t src_w, uint16_t src_h, uint8_t bpp) {
    for (uint16_t y = 0; y < src_h; y++) {
        for (uint16_t x = 0; x < src_w; x++) {
            uint32_t src_idx = (y * src_w + x) * bpp;
            uint32_t dst_idx = (x * src_h + (src_h - y - 1)) * bpp;
            memcpy(dst + dst_idx, src + src_idx, bpp);
        }
    }
}

static void rotate_180(uint8_t *src, uint8_t *dst,
                       uint16_t src_w, uint16_t src_h, uint8_t bpp) {
    uint32_t total = src_w * src_h;
    for (uint32_t i = 0; i < total; i++) {
        memcpy(dst + (total - i - 1) * bpp, src + i * bpp, bpp);
    }
}

static void rotate_270(uint8_t *src, uint8_t *dst,
                       uint16_t src_w, uint16_t src_h, uint8_t bpp) {
    for (uint16_t y = 0; y < src_h; y++) {
        for (uint16_t x = 0; x < src_w; x++) {
            uint32_t src_idx = (y * src_w + x) * bpp;
            uint32_t dst_idx = ((src_w - x - 1) * src_h + y) * bpp;
            memcpy(dst + dst_idx, src + src_idx, bpp);
        }
    }
}

static void screen_rotate(uint8_t *src, uint8_t *dst) {
    uint8_t bpp = s_ctx->color_depth / 8;
    uint16_t w = s_ctx->phys_width;
    uint16_t h = s_ctx->phys_height;

    switch (s_ctx->rotation) {
        case 90:
            rotate_90(src, dst, w, h, bpp);
            break;
        case 180:
            rotate_180(src, dst, w, h, bpp);
            break;
        case 270:
            rotate_270(src, dst, w, h, bpp);
            break;
        default:
            memcpy(dst, src, get_buf_size());
            break;
    }
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    (void)area;

    if (!lv_display_flush_is_last(disp)) {
        lv_display_flush_ready(disp);
        return;
    }

    uint8_t *out_buffer = px_map;

    if (s_ctx->rotation != 0) {
        out_buffer = s_ctx->rot_buf[s_ctx->flip_index];
        screen_rotate(px_map, out_buffer);
    }

    s_ctx->flip_index = !s_ctx->flip_index;

    display_mode_flip_buffer(out_buffer);
    s_ctx->flip_done = false;
    lv_thread_sync_wait(&s_ctx->flip_sync);
    s_ctx->flip_done = true;

    lv_display_flush_ready(disp);
}

int lv_port_init(uint16_t rotation) {
    lv_init();
    lv_tick_set_cb(ameba_get_tick);

    s_ctx = calloc(1, sizeof(lvgl_ctx_t));
    if (!s_ctx) {
        return -1;
    }
    s_ctx->color_depth = LV_COLOR_DEPTH;
    s_ctx->rotation    = rotation;
    s_ctx->flip_index  = 0;

    display_init();  /* legacy: sets display up, registers vblank callback */
    s_ctx->flip_done = true;
    lv_thread_sync_init(&s_ctx->flip_sync);
    s_ctx->phys_width  = display_mode_get_width();
    s_ctx->phys_height = display_mode_get_height();

    gfx_init();
    fs_init();

    get_disp_size();

    s_ctx->disp = lv_display_create(s_ctx->disp_width, s_ctx->disp_height);
    if (!s_ctx->disp) {
        free(s_ctx);
        s_ctx = NULL;
        return -2;
    }

    size_t buf_size = get_buf_size();
    s_ctx->buf1 = malloc(buf_size);
    s_ctx->buf2 = malloc(buf_size);
    if (!s_ctx->buf1 || !s_ctx->buf2) {
        free(s_ctx->buf1);
        free(s_ctx->buf2);
        free(s_ctx);
        s_ctx = NULL;
        return -3;
    }

    lv_display_set_buffers(s_ctx->disp, s_ctx->buf1, s_ctx->buf2,
                           buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_ctx->disp, flush_cb);

    if (s_ctx->rotation != 0) {
        s_ctx->rot_buf[0] = malloc(buf_size);
        s_ctx->rot_buf[1] = malloc(buf_size);
        if (!s_ctx->rot_buf[0] || !s_ctx->rot_buf[1]) {
            free(s_ctx->rot_buf[0]);
            free(s_ctx->rot_buf[1]);
            free(s_ctx->buf1);
            free(s_ctx->buf2);
            free(s_ctx);
            s_ctx = NULL;
            return -4;
        }
    }

    RTK_LOGI(LOG_TAG, "===== LVGL Init OK (rot=%u, %ux%u) =====\n",
             rotation, s_ctx->disp_width, s_ctx->disp_height);
    s_ctx->is_running = true;
    return 0;
}

void lv_port_run(void) {
    if (!s_ctx) {
        return;
    }

    while (s_ctx->is_running) {
        uint32_t time_till_next = lv_task_handler();

        if(time_till_next == LV_NO_TIMER_READY)
            time_till_next = LV_DEF_REFR_PERIOD;

        rtos_time_delay_ms(time_till_next);
    }
}

void lv_port_deinit(void) {
    if (!s_ctx) {
        return;
    }

    s_ctx->is_running = false;

    /* Unregister vblank ISR callback before freeing s_ctx to prevent use-after-free */
    display_mode_callback_t empty = {0};
    display_mode_set_callback(&empty);

    lv_deinit();
    lv_thread_sync_delete(&s_ctx->flip_sync);

    free(s_ctx->buf1);
    free(s_ctx->buf2);
    free(s_ctx->rot_buf[0]);
    free(s_ctx->rot_buf[1]);
    free(s_ctx);
    s_ctx = NULL;

    s_demo_fn = NULL;
    s_demo_rotation = 0;
}

static void lvgl_task(void *param) {
    UNUSED(param);
    RTK_LOGI(LOG_TAG, "lvgl_task start\n");

    if (lv_port_init(s_demo_rotation) != 0) {
        RTK_LOGE(LOG_TAG, "lv_port_init failed\n");
        rtos_task_delete(NULL);
        return;
    }
    RTK_LOGI(LOG_TAG, "lv_port_init returned OK\n");

#ifdef CONFIG_AMEBASMART
    RTK_LOGI(LOG_TAG, "calling lv_port_touch_init\n");
    lv_port_touch_init();
    RTK_LOGI(LOG_TAG, "lv_port_touch_init done\n");
    RTK_LOGI(LOG_TAG, "calling lv_port_touch_register\n");
    if (lv_port_touch_register() != 0) {
        RTK_LOGW(LOG_TAG, "Touch register failed, continuing without touch\n");
    }
    RTK_LOGI(LOG_TAG, "lv_port_touch_register done\n");
#endif

    RTK_LOGI(LOG_TAG, "calling demo fn %p\n", s_demo_fn);
    if (s_demo_fn) {
        s_demo_fn();
    }
    RTK_LOGI(LOG_TAG, "demo fn returned\n");

    RTK_LOGI(LOG_TAG, "lv_port_run enter\n");
    lv_port_run();
    RTK_LOGI(LOG_TAG, "lv_port_run exit\n");
    lv_port_deinit();

    RTK_LOGI(LOG_TAG, "lvgl_task end\n");
    rtos_task_delete(NULL);
}

void lv_port_run_demo(lv_port_demo_fn_t demo_fn, uint16_t rotation) {
    s_demo_fn = demo_fn;
    s_demo_rotation = rotation;
    rtos_task_create(NULL, "lvgl_task", lvgl_task, NULL, 1024 * 32, 1);
}
