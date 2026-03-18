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

#define LOG_TAG "panel_b1620a"

static const uint8_t b1620a_init_cmds[][3] = {
    {0x00, 0x00, 0x00},
};

static const panel_timing_t b1620a_timing = {
    .width = 720,
    .height = 720,
    .hsync_front_porch = 46,
    .hsync_back_porch = 44,
    .hsync_pulse_width = 2,
    .vsync_front_porch = 15,
    .vsync_back_porch = 15,
    .vsync_pulse_width = 5,
    .clock_frequency = 60,
    .hsync_active_low = true,
    .vsync_active_low = true,
    .de_active_high = true,
    .dclk_falling_edge = false,
    .use_de = true
};

typedef struct {
    bool spi_initialized;
    spi_t spi_lcd;
} b1620a_priv_t;

static bool b1620a_init(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    b1620a_priv_t *priv_data = panel->desc->private_data;
    spi_t *spi = &priv_data->spi_lcd;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Initializing B1620A panel\n");

    spi_driver_init(spi, panel->desc->spi_config);

    #include "panel_b1620a_spi.inc"

    priv_data->spi_initialized = true;

    return true;

}

static bool b1620a_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting B1620A panel\n");

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

static bool b1620a_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on B1620A\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        //wait for power to be stable.
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool b1620a_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off B1620A\n");

    // power off
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool b1620a_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting B1620A\n");

    if (panel->desc->gpio_config->reset_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(1);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 0);
        rtos_time_delay_ms(10);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(120);
    }

    return true;
}

static bool b1620a_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        // only turn on/off here, need hardware to support PWM control.
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
    }

    return true;
}

static bool b1620a_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static panel_ops_t b1620a_ops = {
    .init = b1620a_init,
    .exit = b1620a_exit,
    .power_on = b1620a_power_on,
    .power_off = b1620a_power_off,
    .reset = b1620a_reset,
    .enable_backlight = b1620a_enable_backlight,
    .disable_backlight = b1620a_disable_backlight,
    //RGB interface doesn't need enable display.
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

static b1620a_priv_t b1620a_priv = {
    .spi_initialized = false,
};

panel_desc_t b1620a_desc = {
    .name = "b1620a_720x720",
    .manufacturer = "TDO",
    .model = "B1620A",

    .interface = PANEL_IF_RGB,
    .rgb_format = PANEL_RGB_FORMAT_RGB888,

    .timing = b1620a_timing,
    .gpio_config = NULL,
    .spi_config = NULL,

    .init_cmd_count = sizeof(b1620a_init_cmds) / sizeof(b1620a_init_cmds[0]),
    .init_cmds = b1620a_init_cmds,

    .ops = &b1620a_ops,
    .private_data = &b1620a_priv
};

bool panel_b1620a_register(void) {
    return panel_register(&b1620a_desc);
}