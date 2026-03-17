#include <stdlib.h>
#include <string.h>

#include "ameba_soc.h"
#include "os_wrapper.h"

#include "jpeg_decoder.h"
#include "lv_draw_ppe.h"
#include "lv_fs_romfs.h"

#include "lvgl.h"

#include "demos/lv_demos.h"
#include "display_mode_setting.h"

#include "touch.h"
#include "lv_lcd_demo.h"

#define LOG_TAG "LV-Demos"

static int32_t          g_flush_mode = FLUSH_DIRECT;
static lv_thread_sync_t g_flip_sync;
static bool             g_flip_done = false;
static uint32_t         g_rotation = 0; // 0, 90, 180, 270
static uint8_t          g_flip_index = 0;
static uint8_t*         g_rotation_buffer[2] = { 0 };

static void lv_lcd_open_demo(void) {
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

static void lv_lcd_open_hw_render(void) {
#if RTK_ROMFS_ENABLE
    lv_fs_romfs_init();
#endif

#if RTK_HW_JPEG_DECODE
    lv_ameba_jpeg_init();
#endif

#if RTK_HW_PPE_ENABLE
    lv_draw_ppe_init();
#endif
}

static void lv_lcd_set_touch_if_exist(void) {
    bool exist = touch_init();
    if (exist) {
        lv_indev_t * indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, touch_read);
    }
}

static void demo_vblank_handler(void) {
    if (!g_flip_done) {
        lv_thread_sync_signal(&g_flip_sync);
        g_flip_done = true;
    }
}

void screen_rotate(lv_display_t *display, uint32_t rotation, uint8_t *in_screen, uint8_t *out_screen) {
    uint64_t start, end, time_used;
    start = rtos_time_get_current_system_time_ns();
    lv_color_format_t cf = lv_display_get_color_format(display);
    uint32_t pixel_size = lv_color_format_get_size(cf);
    lv_draw_ppe_header_t src_header = {0};
    lv_draw_ppe_header_t dest_header = {0};
    lv_draw_ppe_configuration_t ppe_draw_conf = {0};
    uint16_t width = lv_display_get_horizontal_resolution(display);
    uint16_t height = lv_display_get_vertical_resolution(display);
    uint16_t src_stride = width * pixel_size;
    uint16_t dest_stride = lv_new_demo_get_width() * pixel_size;
    ppe_draw_conf.src_buf = in_screen;
    ppe_draw_conf.dest_buf = out_screen;
    src_header.cf = cf;
    src_header.w = width;
    src_header.h = height;
    src_header.stride = src_stride;
    src_header.color = 0xFFFFFFFF;
    dest_header.cf = cf;
    dest_header.w = lv_new_demo_get_width();
    dest_header.h = lv_new_demo_get_height();
    dest_header.stride = dest_stride;
    ppe_draw_conf.src_header = &src_header;
    ppe_draw_conf.dest_header = &dest_header;
    ppe_draw_conf.scale_x = 1.0f;
    ppe_draw_conf.scale_y = 1.0f;
    ppe_draw_conf.angle = rotation;
    ppe_draw_conf.opa = LV_OPA_COVER;
    lv_draw_ppe_configure_and_start_transfer(&ppe_draw_conf);
    end = rtos_time_get_current_system_time_ns();
    time_used = end - start;
    RTK_LOGD(LOG_TAG, "rotation Time:%8lld, w:%d, h:%d\n", time_used, (int)lv_new_demo_get_width(), (int)lv_new_demo_get_height());
}

static void display_flush_direct(lv_display_t *display, const lv_area_t *area, void *px_map) {
    bool last_flush = lv_display_flush_is_last(display);

    static int flush_count = 0;
    RTK_LOGD(LOG_TAG, "Flush #%d %s: Buffer=%p, Area=(%d,%d)-(%d,%d)\n",
             flush_count++, last_flush ? "last" : "int",
             px_map, area->x1, area->y1, area->x2, area->y2);

    if (last_flush) {
        // send new rendering buffer to lcdc
        uint8_t *screen_buffer = px_map;
        if (g_rotation) {
            screen_buffer = g_rotation_buffer[g_flip_index];
            screen_rotate(display, g_rotation, px_map, screen_buffer);
        }
        g_flip_index = !g_flip_index;
        display_mode_flip_buffer(screen_buffer);
        RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "Page flip\n");

        // wait flip done
        g_flip_done = false;
        lv_thread_sync_wait(&g_flip_sync);
        g_flip_done = true;
    }

    lv_display_flush_ready(display);
}

static void display_flush_full(lv_display_t *display, const lv_area_t *area, void *px_map) {

    bool last_flush = lv_display_flush_is_last(display);

    static int flush_count = 0;
    RTK_LOGD(LOG_TAG, "Flush #%d %s: Buffer=%p, Area=(%d,%d)-(%d,%d)\n",
             flush_count++, last_flush ? "last" : "int",
             px_map, area->x1, area->y1, area->x2, area->y2);

    // send new rendering buffer to lcdc
    display_mode_flip_buffer(px_map);

    // wait flip done
    g_flip_done = false;
    lv_thread_sync_wait(&g_flip_sync);
    g_flip_done = true;

    lv_display_flush_ready(display);
}

void lv_lcd_demos_task(void *param) {
    UNUSED(param);

    lv_init();

    display_mode_init(LV_COLOR_DEPTH);

    lv_lcd_open_hw_render();

    lv_tick_set_cb(ameba_tick_get);

    // vsync semphore
    lv_thread_sync_init(&g_flip_sync);

    display_mode_callback_t callback;
    callback.vblank_handler = demo_vblank_handler;
    display_mode_set_callback(&callback);

    int32_t screen_width = lv_new_demo_get_width();
    int32_t screen_height = lv_new_demo_get_height();

    size_t buffer_size = screen_width * screen_height * LV_COLOR_DEPTH / 8;
    uint8_t *buf1 = malloc(buffer_size);
    uint8_t *buf2 = malloc(buffer_size);

    if (g_rotation == 90 || g_rotation == 270) {
        screen_width = lv_new_demo_get_height();
        screen_height = lv_new_demo_get_width();
    }

    lv_display_t *display = lv_display_create(screen_width, screen_height);

    if (g_flush_mode == FLUSH_DIRECT) {
        lv_display_set_buffers(display, buf1, buf2, screen_width * screen_height * LV_COLOR_DEPTH, LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(display, (lv_display_flush_cb_t)display_flush_direct);
    } else {
        lv_display_set_buffers(display, buf1, buf2, screen_width * screen_height * LV_COLOR_DEPTH, LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display, (lv_display_flush_cb_t)display_flush_full);
    }

    lv_lcd_set_touch_if_exist();

    if (g_rotation) {
        g_rotation_buffer[0] = malloc(buffer_size);
        g_rotation_buffer[1] = malloc(buffer_size);
    }

    lv_lcd_open_demo();

    while(1) {
        /* Periodically call the lv_task handler.
         * It could be done in a timer interrupt or an OS task too.*/
        uint32_t time_till_next = lv_task_handler();

        /* handle LV_NO_TIMER_READY. Another option is to `sleep` for longer */
        if(time_till_next == LV_NO_TIMER_READY)
            time_till_next = LV_DEF_REFR_PERIOD;

        /* delay to avoid unnecessary polling */
        rtos_time_delay_ms(time_till_next);
    }

    lv_deinit();

    lv_thread_sync_delete(&g_flip_sync);

    free(buf1);
    free(buf2);
    if (g_rotation_buffer[0]) {
        free(g_rotation_buffer[0]);
    }
    if (g_rotation_buffer[1]) {
        free(g_rotation_buffer[2]);
    }

    rtos_task_delete(NULL);
}

long unsigned int lv_lcd_demos(short unsigned int argc, unsigned char *argv[]) {
    UNUSED(argc);
    while (*argv) {
        if (strcmp((const char *)(*argv), "-m") == 0) {
            argv++;
            if (*argv) {
                g_flush_mode = atof((const char *)(*argv));
            }
        }
        if (*argv) {
            argv++;
        }
    }

    rtos_task_create(NULL, "lv_demos_task", lv_lcd_demos_task, NULL, 1024 * 32, 1);
    return TRUE;
}

CMD_TABLE_DATA_SECTION
const COMMAND_TABLE cmd_table_lv_demos[] = {
    {(const char *)"lv_lcd_demos", lv_lcd_demos},
};
