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

#include "spi_api.h"

#include "panel_spi_init.h"

#include "panel_manager.h"
#include "panel_pin_config.h"

#define LOG_TAG "panel_hj3508_12"

static const uint8_t hj3508_12_init_cmds[][32] = {
    {0x00, 0x00, 0x00},
};

static const panel_timing_t hj3508_12_timing = {
    .width = 320,
    .height = 480,
    .hsync_front_porch = 3,
    .hsync_back_porch = 3,
    .hsync_pulse_width = 3,//need check
    .vsync_front_porch = 2,
    .vsync_back_porch = 2,
    .vsync_pulse_width = 1,//need check
    .clock_frequency = 60,
    .hsync_active_low = true,
    .vsync_active_low = true,
    .de_active_high = true,
    .dclk_falling_edge = true,
    .use_de = true
};

typedef struct {
    bool spi_initialized;
    spi_t spi_lcd;
} hj3508_12_priv_t;

static bool hj3508_12_init(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    hj3508_12_priv_t *priv_data = panel->desc->private_data;
    spi_t *spi = &priv_data->spi_lcd;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Initializing HJ3508_12 panel\n");

    spi_driver_init(spi, panel->desc->spi_config);

    #include "panel_hj3508_12_spi.inc"

    priv_data->spi_initialized = true;

    return true;
}

static bool hj3508_12_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting HJ3508_12 panel\n");

    // turn off backlight.
    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
    }

    // close power.
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    return true;
}

static bool hj3508_12_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on HJ3508_12\n");

    // enable power.
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        //wait for power to be stable.
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool hj3508_12_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off HJ3508_12\n");

    // power off
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool hj3508_12_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting HJ3508_12\n");

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

static bool hj3508_12_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        // only turn on/off here, need hardware to support PWM control.
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
    }

    return true;
}

static bool hj3508_12_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static panel_ops_t hj3508_12_ops = {
    .init = hj3508_12_init,
    .exit = hj3508_12_exit,
    .power_on = hj3508_12_power_on,
    .power_off = hj3508_12_power_off,
    .reset = hj3508_12_reset,
    .enable_backlight = hj3508_12_enable_backlight,
    .disable_backlight = hj3508_12_disable_backlight,
    //RGB interface doesn't need enable display.
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

static hj3508_12_priv_t hj3508_12_priv = {
    .spi_initialized = false,
};

panel_desc_t hj3508_12_desc = {
    .name = "hj3508_12_rgb_320x480",
    .manufacturer = "HongJia",
    .model = "HJ3508_12",

    .interface = PANEL_IF_RGB,
    .rgb_format = PANEL_RGB_FORMAT_RGB888,

    .timing = hj3508_12_timing,
    .gpio_config = NULL,
    .spi_config = NULL,

    .init_cmd_count = sizeof(hj3508_12_init_cmds) / sizeof(hj3508_12_init_cmds[0]),
    .init_cmds = hj3508_12_init_cmds,

    .ops = &hj3508_12_ops,
    .private_data = &hj3508_12_priv
};

bool panel_hj3508_12_register(void) {
    return panel_register(&hj3508_12_desc);
}