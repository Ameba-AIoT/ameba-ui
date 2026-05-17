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

#include "os_wrapper.h"
#include "ameba_soc.h"

#include "panel_manager.h"
#include "panel_pin_config.h"

#define LOG_TAG "panel_t1720a"

static const uint8_t t1720a_init_cmds[][32] = {
    {0x00, 0x00, 0x00},
};

// T1720A timing parameters（800x480）
static const panel_timing_t t1720a_timing = {
    .width = 800,
    .height = 480,
    .hsync_front_porch = 40,
    .hsync_back_porch = 40,
    .hsync_pulse_width = 4,
    .vsync_front_porch = 6,
    .vsync_back_porch = 4,
    .vsync_pulse_width = 1,
    .clock_frequency = 60,
    .hsync_active_low = true,
    .vsync_active_low = true,
    .de_active_high = true,
    .dclk_falling_edge = true,
    .use_de = true
};

typedef struct {
    bool spi_initialized;
    uint32_t spi_bus;
} t1720a_priv_t;

static bool t1720a_init(void *panel_data) {
    (void) panel_data;
    return true;
}

static bool t1720a_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting T1720A panel\n");

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
    }

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    return true;
}

static bool t1720a_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on T1720A\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool t1720a_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off T1720A\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool t1720a_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting T1720A\n");

    if (panel->desc->gpio_config->reset_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 0);
        rtos_time_delay_ms(10);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(120);
    }

    return true;
}

static bool t1720a_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight enabled, brightness: %d\n", brightness);
    }

    return true;
}

static bool t1720a_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static panel_ops_t t1720a_ops = {
    .init = t1720a_init,
    .exit = t1720a_exit,
    .power_on = t1720a_power_on,
    .power_off = t1720a_power_off,
    .reset = t1720a_reset,
    .enable_backlight = t1720a_enable_backlight,
    .disable_backlight = t1720a_disable_backlight,
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

static t1720a_priv_t t1720a_priv = {
    .spi_initialized = false,
    .spi_bus = 0
};

panel_desc_t t1720a_desc = {
    .name = "t1720a_800x480",
    .manufacturer = "Top Display",
    .model = "T1720A",

    .interface = PANEL_IF_RGB,
    .rgb_format = PANEL_RGB_FORMAT_RGB888,

    .timing = t1720a_timing,
    .gpio_config = NULL,

    .init_cmd_count = sizeof(t1720a_init_cmds) / sizeof(t1720a_init_cmds[0]),
    .init_cmds = t1720a_init_cmds,

    .ops = &t1720a_ops,
    .private_data = &t1720a_priv
};

bool panel_t1720a_register(void) {
    return panel_register(&t1720a_desc);
}