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

#include "ameba.h"
#include "basic_types.h"
#include "os_wrapper.h"

#include "display_controller.h"

#include "panel_manager.h"

#include "panel_pin_config.h"
#include "display_mode_setting.h"

#define LOG_TAG "DisplayModeSetting"

#define DEBUG_PURE 0

#if defined(CONFIG_ST7701S) && CONFIG_ST7701S
#define SELECTED_PANEL_NAME "st7701s_rgb_800x480"
#elif defined(CONFIG_ST7701S_RGB565) && CONFIG_ST7701S_RGB565
#define SELECTED_PANEL_NAME "st7701s_rgb565_480x480"
#elif defined(CONFIG_ST7701P_RGB) && CONFIG_ST7701P_RGB
#define SELECTED_PANEL_NAME "st7701p_rgb_480x480"
#elif defined(CONFIG_HJ3508_12) && CONFIG_HJ3508_12
#define SELECTED_PANEL_NAME "hj3508_12_rgb_320x480"
#elif defined(CONFIG_B1620A) && CONFIG_B1620A
#define SELECTED_PANEL_NAME "b1620a_720x720"
#elif defined(CONFIG_T1720A) && CONFIG_T1720A
#define SELECTED_PANEL_NAME "t1720a_800x480"
#elif defined(CONFIG_JD9165BA) && CONFIG_JD9165BA
#define SELECTED_PANEL_NAME "jd9165ba_1024x600"
#else
#define SELECTED_PANEL_NAME "unknown"
#endif

typedef struct {
    panel_dev_t *panel;
    display_mode_callback_t *callback;
    display_driver_callback_t lcdc_callback;
} display_mode_t;

static display_mode_t display_mode = {0};

static void display_vblank_handler(void *user_data) {
    (void) user_data;
    if (display_mode.callback) {
        display_mode.callback->vblank_handler();
    }
}

void display_mode_set_callback(display_mode_callback_t *callback) {
    display_mode.callback = callback;
}

void display_mode_flip_buffer(uint8_t *buffer) {
    controller_do_page_flip(buffer);
}

void fillPureBlueBuffer(uint32_t* buffer, int total_pixels) {
    uint32_t pureBlue = 0xFF0000FF; // ARGB: A=FF, R=00, G=00, B=FF

    for (int i = 0; i < total_pixels / 2; ++i) {
        buffer[i] = pureBlue;
    }
    for (int i = total_pixels / 2; i < total_pixels; ++i) {
        buffer[i] = 0xFFFFFFFF;
    }
}

//pure color for debugging driver without app rendering.
void display_mode_pure_color(int32_t color_depth) {
    size_t buffer_size = 0;
    size_t total_pixels = 0;
    if (!strcmp(SELECTED_PANEL_NAME, "st7701s_rgb_800x480")) {
        total_pixels = 800 * 480;
    } else if (!strcmp(SELECTED_PANEL_NAME, "st7701s_rgb565_480x480")) {
        total_pixels = 480 * 480;
    } else if (!strcmp(SELECTED_PANEL_NAME, "st7701p_rgb_480x480")) {
        total_pixels = 480 * 480;
    } else if (!strcmp(SELECTED_PANEL_NAME, "hj3508_12_rgb_320x480")) {
        total_pixels = 320 * 480;
    } else if (!strcmp(SELECTED_PANEL_NAME, "b1620a_720x720")) {
        total_pixels = 720 * 720;
    } else if (!strcmp(SELECTED_PANEL_NAME, "t1720a_800x480")) {
        total_pixels = 800 * 480;
    } else if (!strcmp(SELECTED_PANEL_NAME, "jd9165ba_1024x600")) {
        total_pixels = 1024 * 600;
    } else {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "unsupported panel\n");
        return;
    }

    buffer_size = total_pixels * color_depth / 8;

    uint8_t *buf1 = malloc(buffer_size);
    uint8_t *buf2 = malloc(buffer_size);

    // memset(buf1, 0xFF, buffer_size);
    fillPureBlueBuffer((uint32_t* )buf1, total_pixels);
    memset(buf2, 0x0, buffer_size);

    while(1) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "do flip\n");
        controller_do_page_flip(buf1);
        rtos_time_delay_ms(2000);
        controller_do_page_flip(buf2);
        rtos_time_delay_ms(2000);
    }
    while(1) {
        rtos_time_delay_ms(2000);
    }
    free(buf1);
    free(buf2);
}

// lcdc init flow
bool display_mode_init(int32_t color_depth) {
    // 1. manage panel pinmux.
    panel_configure_pinmux(SELECTED_PANEL_NAME);

    // 2. register all panels available.
    extern bool panel_st7701s_register(void);
    panel_st7701s_register();

    extern bool panel_st7701s_rgb565_register(void);
    panel_st7701s_rgb565_register();

    extern bool panel_st7701p_rgb_register(void);
    panel_st7701p_rgb_register();

    extern bool panel_hj3508_12_register(void);
    panel_hj3508_12_register();

    extern bool panel_b1620a_register(void);
    panel_b1620a_register();

    extern bool panel_t1720a_register(void);
    panel_t1720a_register();

    extern bool panel_jd9165ba_register(void);
    panel_jd9165ba_register();

    // 3. list all registered panels.
    panel_list_all();

    // 4. choose panel from user setting.
    panel_desc_t *desc = panel_find_by_name(SELECTED_PANEL_NAME);
    if (!desc) {
        // choose panel from format.
        desc = panel_find_by_resolution(800, 480);
        if (!desc) {
            RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "No suitable panel found\n");
            return false;
        }
    }

    desc->gpio_config = panel_get_gpio_config(desc->name);
    desc->spi_config = panel_get_spi_config(desc->name);

    // 5. create panel device.
    panel_dev_t *panel = panel_device_create(desc);
    if (!panel) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to create panel device\n");
        return false;
    }

    if (!panel_reset(panel)) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to reset panel\n");
        panel_device_exit(panel);
        panel_device_destroy(panel);
        return false;
    }

    // 6. init panel.
    if (!panel_device_init(panel)) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to initialize panel device\n");
        panel_device_destroy(panel);
        return false;
    }

    // 7. power on panel.
    if (!panel_power_on(panel)) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to power on panel\n");
        panel_device_exit(panel);
        panel_device_destroy(panel);
        return false;
    }

    // 8. init lcdc controller with panel.
    if (!controller_init_with_panel(color_depth, panel)) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to initialize LCDC\n");
        panel_power_off(panel);
        panel_device_exit(panel);
        panel_device_destroy(panel);
        return false;
    }

    // 9. turn on back light.
    panel_set_backlight(panel, 150);

    // 10. enable display.
    panel_enable_display(panel);

    // 11. register vblank
    display_mode.lcdc_callback.vblank_handler = display_vblank_handler;
    controller_register_vblank_callback(&display_mode.lcdc_callback);

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "LCD system initialized successfully with panel: %s\n", 
             desc->name);

    display_mode.panel = panel;

#if DEBUG_PURE
    display_mode_pure_color(color_depth);
#endif

    return true;
}
