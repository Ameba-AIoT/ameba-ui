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

#define LOG_TAG "panel_jd9165ba"

static const uint8_t jd9165ba_init_cmds[][3] = {
    {0x00, 0x00, 0x00},
};

// JD9165BA timing parameters（1024x600）
static const panel_timing_t jd9165ba_timing = {
    .width = 1024,
    .height = 600,
    .hsync_front_porch = 160,
    .hsync_back_porch = 160,
    .hsync_pulse_width = 24,
    .vsync_front_porch = 12,
    .vsync_back_porch = 23,
    .vsync_pulse_width = 2,
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
} jd9165ba_priv_t;

static bool jd9165ba_init(void *panel_data) {
    (void) panel_data;
    return true;
}

static bool jd9165ba_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting JD9165BA panel\n");

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
    }

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    return true;
}

static bool jd9165ba_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on JD9165BA\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool jd9165ba_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off JD9165BA\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool jd9165ba_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting JD9165BA\n");

    if (panel->desc->gpio_config->reset_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(10);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 0);
        rtos_time_delay_ms(10);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(120);
    }

    return true;
}

static bool jd9165ba_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight enabled, brightness: %d\n", brightness);
    }

    return true;
}

static bool jd9165ba_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static panel_ops_t jd9165ba_ops = {
    .init = jd9165ba_init,
    .exit = jd9165ba_exit,
    .power_on = jd9165ba_power_on,
    .power_off = jd9165ba_power_off,
    .reset = jd9165ba_reset,
    .enable_backlight = jd9165ba_enable_backlight,
    .disable_backlight = jd9165ba_disable_backlight,
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

static jd9165ba_priv_t jd9165ba_priv = {
    .spi_initialized = false,
    .spi_bus = 0
};

panel_desc_t jd9165ba_desc = {
    .name = "jd9165ba_1024x600",
    .manufacturer = "Top Display",
    .model = "JD9165BA",

    .interface = PANEL_IF_RGB,
    .rgb_format = PANEL_RGB_FORMAT_RGB888,

    .timing = jd9165ba_timing,
    .gpio_config = NULL,

    .init_cmd_count = sizeof(jd9165ba_init_cmds) / sizeof(jd9165ba_init_cmds[0]),
    .init_cmds = jd9165ba_init_cmds,

    .ops = &jd9165ba_ops,
    .private_data = &jd9165ba_priv
};

bool panel_jd9165ba_register(void) {
    return panel_register(&jd9165ba_desc);
}