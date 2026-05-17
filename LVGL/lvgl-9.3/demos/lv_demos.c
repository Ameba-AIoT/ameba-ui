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
#include "demos/lv_demos.h"
#include "display_mode_setting.h"

#define LOG_TAG "LV-Demos"
#define ENABLE_TOUCH_DEMO 0

#if ENABLE_TOUCH_DEMO
#include "touch.h"
#endif

typedef struct {
    lv_display_t *disp;
    uint8_t *buf1;
    uint8_t *buf2;
    uint8_t *rot_buf[2];
    uint8_t color_depth;       /* Color depth: 16/24/32 */
    uint16_t rotation;         /* Rotation: 0/90/180/270 */
    uint16_t phys_width;       /* Physical screen width (from display_mode) */
    uint16_t phys_height;      /* Physical screen height (from display_mode) */
    uint16_t disp_width;       /* Display width after rotation */
    uint16_t disp_height;      /* Display height after rotation */
    lv_thread_sync_t flip_sync;
    volatile bool flip_done;
    uint8_t flip_index;
    bool is_running;
} lvgl_ctx_t;

static lvgl_ctx_t *s_ctx = NULL;

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

    display_mode_callback_t callback = {
        .vblank_handler = display_vblank_handler
    };
    display_mode_set_callback(&callback);
}

static void gfx_init(void) {
#ifdef RTK_HW_JPEG_DECODE
    lv_ameba_jpeg_init();
#endif
#ifdef RTK_HW_PPE_ENABLE
    lv_draw_ppe_init();
#endif
}

static void fs_init(void) {
#ifdef RTK_ROMFS_ENABLE
    lv_fs_romfs_init();
#endif
}

/* Helper functions */
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

/* Software rotation */
static void rotate_90(uint8_t *src, uint8_t *dst,
                        uint16_t src_w, uint16_t src_h, uint8_t bytes_per_pixel) {
    for (uint16_t y = 0; y < src_h; y++) {
        for (uint16_t x = 0; x < src_w; x++) {
            uint32_t src_idx = (y * src_w + x) * bytes_per_pixel;
            uint32_t dst_idx = (x * src_h + (src_h - y - 1)) * bytes_per_pixel;
            memcpy(dst + dst_idx, src + src_idx, bytes_per_pixel);
        }
    }
}

static void rotate_180(uint8_t *src, uint8_t *dst,
                        uint16_t src_w, uint16_t src_h, uint8_t bytes_per_pixel) {
    uint32_t total_pixels = src_w * src_h;
    for (uint32_t i = 0; i < total_pixels; i++) {
        uint32_t dst_idx = (total_pixels - i - 1) * bytes_per_pixel;
        memcpy(dst + dst_idx, src + i * bytes_per_pixel, bytes_per_pixel);
    }
}

static void rotate_270(uint8_t *src, uint8_t *dst,
                        uint16_t src_w, uint16_t src_h, uint8_t bytes_per_pixel) {
    for (uint16_t y = 0; y < src_h; y++) {
        for (uint16_t x = 0; x < src_w; x++) {
            uint32_t src_idx = (y * src_w + x) * bytes_per_pixel;
            uint32_t dst_idx = ((src_w - x - 1) * src_h + y) * bytes_per_pixel;
            memcpy(dst + dst_idx, src + src_idx, bytes_per_pixel);
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
    bool last_flush = lv_display_flush_is_last(disp);

    if (last_flush) {
        uint8_t *out_buffer = (uint8_t *)px_map;

        if (s_ctx->rotation != 0) {
            out_buffer = s_ctx->rot_buf[s_ctx->flip_index];
            screen_rotate(px_map, out_buffer);
        }

        s_ctx->flip_index = !s_ctx->flip_index;
        display_mode_flip_buffer(out_buffer);

        s_ctx->flip_done = false;
    }
}

static void lvgl_demo(void) {
#if defined(CONFIG_LV_DEMO_WIDGETS)
    lv_demo_widgets();
#endif
#if defined(CONFIG_LV_DEMO_STRESS)
    lv_demo_stress();
#endif
#if defined(CONFIG_LV_DEMO_MUSIC)
    lv_demo_music();
#endif
#if defined(CONFIG_LV_DEMO_BENCHMARK)
    lv_demo_benchmark();
#endif

}

static void display_flush_wait(lv_display_t * display) {
    (void)display;
    if (s_ctx->flip_done == false) {
        lv_thread_sync_wait(&s_ctx->flip_sync);
    }
}

static int lvgl_init(uint16_t rotation) {
    /* Platform initialization */
    lv_init();
    lv_tick_set_cb(ameba_get_tick);

    display_init();
    gfx_init();
    fs_init();

    s_ctx = calloc(1, sizeof(lvgl_ctx_t));
    if (!s_ctx) return -2;

    s_ctx->color_depth = LV_COLOR_DEPTH;
    s_ctx->rotation = rotation;
    s_ctx->flip_done = true;
    s_ctx->flip_index = 0;
    lv_thread_sync_init(&s_ctx->flip_sync);

    /* Get physical screen dimensions from display_mode */
    s_ctx->phys_width = display_mode_get_width();
    s_ctx->phys_height = display_mode_get_height();
    get_disp_size();  /* Calculate display size after rotation */

    s_ctx->disp = lv_display_create(s_ctx->disp_width, s_ctx->disp_height);
    if (!s_ctx->disp) {
        free(s_ctx);
        return -3;
    }

    size_t buf_size = get_buf_size();
    s_ctx->buf1 = malloc(buf_size);
    s_ctx->buf2 = malloc(buf_size);

    if (!s_ctx->buf1 || !s_ctx->buf2) {
        free(s_ctx->buf1);
        free(s_ctx->buf2);
        free(s_ctx);
        return -4;
    }

    lv_display_set_buffers(s_ctx->disp, s_ctx->buf1, s_ctx->buf2,
                            buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_wait_cb(s_ctx->disp, display_flush_wait);
    lv_display_set_flush_cb(s_ctx->disp, flush_cb);

    if (s_ctx->rotation != 0) {
        s_ctx->rot_buf[0] = malloc(buf_size);
        s_ctx->rot_buf[1] = malloc(buf_size);
        if (!s_ctx->rot_buf[0] || !s_ctx->rot_buf[1]) {
            // TODO
            return -5;
        }
    }

    RTK_LOGI(LOG_TAG, "Display initialized\n");

#if ENABLE_TOUCH_DEMO
    touch_init();
    if (touch_register_to_lvgl() != 0) {
        RTK_LOGE(LOG_TAG, "Failed to register touch!\n");
        return -6;
    }
#endif

    RTK_LOGI(LOG_TAG, "===== LVGL Initialization Complete =====\n");
    s_ctx->is_running = true;

    return 0;
}

static void lvgl_run(void) {
    if (!s_ctx) return;

    while (s_ctx->is_running) {
        /* Periodically call the lv_task handler.
         * It could be done in a timer interrupt or an OS task too.*/
        uint32_t time_till_next = lv_task_handler();

        /* handle LV_NO_TIMER_READY. Another option is to `sleep` for longer */
        if(time_till_next == LV_NO_TIMER_READY)
            time_till_next = LV_DEF_REFR_PERIOD;

        /* delay to avoid unnecessary polling */
        rtos_time_delay_ms(time_till_next);
    }
}

static void lvgl_deinit(void) {
    if (!s_ctx) return;

    lv_deinit();

    free(s_ctx->buf1);
    free(s_ctx->buf2);
    free(s_ctx->rot_buf[0]);
    free(s_ctx->rot_buf[1]);
    free(s_ctx);

    s_ctx = NULL;
}

void lvgl_task(void *param) {
    UNUSED(param);

    RTK_LOGI(LOG_TAG, "lvgl_task start...\n");

    if (lvgl_init(0) == 0) {
        lvgl_demo();
        lvgl_run();
    }

    lvgl_deinit();

    RTK_LOGI(LOG_TAG, "lvgl_task end\n");

    rtos_task_delete(NULL);
}

u32 lv_demos(u16 argc, u8  *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    rtos_task_create(NULL, "lvgl_task", lvgl_task, NULL, 1024 * 32, 1);
    return TRUE;
}

CMD_TABLE_DATA_SECTION
const COMMAND_TABLE cmd_table_lv_demos[] = {
    {"lv_demos", lv_demos},
};
