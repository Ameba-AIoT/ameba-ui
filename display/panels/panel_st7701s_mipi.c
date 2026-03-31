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

#define LOG_TAG "panel_st7701s_mipi"

#include <stdint.h>

#define REGFLAG_DELAY          0xFE
#define REGFLAG_END_OF_TABLE   0xFF

static const uint8_t st7701s_mipi_init_cmds[][32] = {
    /* 0: Sleep Out */
    {0x11, 0,
     /* padding */},

    /* 1: Delay 120 ms */
    {REGFLAG_DELAY, 120},

    /* 2: Bank0 Setting - Display Control setting */
    {0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10},

    {0xC0, 2, 0x63, 0x00},
    {0xC1, 2, 0x0C, 0x02},
    {0xC2, 2, 0x31, 0x08},
    {0xCC, 1, 0x10},

    /* Gamma Cluster Setting */
    {0xB0, 16,
    0x40, 0x02, 0x87, 0x0E, 0x15, 0x0A, 0x03, 0x0A,
    0x0A, 0x18, 0x08, 0x16, 0x13, 0x07, 0x09, 0x19},

    {0xB1, 16,
    0x40, 0x01, 0x86, 0x0D, 0x13, 0x09, 0x03, 0x0A,
    0x09, 0x1C, 0x09, 0x15, 0x13, 0x91, 0x16, 0x19},

    /* Bank1 Setting - Power Control Registers Initial */
    {0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x11},
    {0xB0, 1, 0x4D},

    /* Vcom Setting */
    {0xB1, 1, 0x64},

    {0xB2, 1, 0x07},
    {0xB3, 1, 0x80},
    {0xB5, 1, 0x47},
    {0xB7, 1, 0x85},
    {0xB8, 1, 0x21},
    {0xB9, 1, 0x10},
    {0xC1, 1, 0x78},
    {0xC2, 1, 0x78},
    {0xD0, 1, 0x88},

    /* Delay 100 ms */
    {REGFLAG_DELAY, 100},

    /* GIP Setting */
    {0xE0, 3, 0x00, 0x84, 0x02},

    {0xE1, 11,
    0x06, 0x00, 0x00, 0x00, 0x05, 0x00,
    0x00, 0x00, 0x00, 0x20, 0x20},

    {0xE2, 13,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

    {0xE3, 4, 0x00, 0x00, 0x33, 0x33},
    {0xE4, 2, 0x44, 0x44},

    {0xE5, 16,
    0x09, 0x31, 0xBE, 0xA0, 0x0B, 0x31, 0xBE, 0xA0,
    0x05, 0x31, 0xBE, 0xA0, 0x07, 0x31, 0xBE, 0xA0},

    {0xE6, 4, 0x00, 0x00, 0x33, 0x33},
    {0xE7, 2, 0x44, 0x44},

    {0xE8, 16,
    0x08, 0x31, 0xBE, 0xA0, 0x0A, 0x31, 0xBE, 0xA0,
    0x04, 0x31, 0xBE, 0xA0, 0x06, 0x31, 0xBE, 0xA0},

    {0xEA, 16,
    0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00,
    0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00},

    {0xEB, 7,
    0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00},

    {0xEC, 2, 0x02, 0x00},

    {0xED, 16,
    0xF5, 0x47, 0x6F, 0x0B, 0x8F, 0x9F, 0xFF, 0xFF,
    0xFF, 0xFF, 0xF9, 0xF8, 0xB0, 0xF6, 0x74, 0x5F},

    {0xEF, 12,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04},

    /* Back to Bank0 */
    {0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00},

    /* Display ON */
    {0x29, 0},

    /* End flag */
    {REGFLAG_END_OF_TABLE, 0x00},
};

static const panel_timing_t st7701s_mipi_timing = {
    .width = 480,
    .height = 800,
    .hsync_front_porch = 30,
    .hsync_back_porch = 30,
    .hsync_pulse_width = 4,
    .vsync_front_porch = 15,
    .vsync_back_porch = 20,
    .vsync_pulse_width = 5,
    .clock_frequency = 30,
    .hsync_active_low = true,
    .vsync_active_low = true,
    .de_active_high = true,
    .dclk_falling_edge = true,
    .use_de = true
};

typedef struct {
    bool spi_initialized;
    uint32_t spi_bus;
} st7701s_mipi_priv_t;

static bool st7701s_mipi_init(void *panel_data) {
    (void) panel_data;
    return true;
}

static bool st7701s_mipi_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting ST7701S_mipi panel\n");

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

static bool st7701s_mipi_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on ST7701S_mipi\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        //wait for power to be stable.
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool st7701s_mipi_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off ST7701S_mipi\n");

    // power off
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool st7701s_mipi_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting ST7701S_mipi\n");

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

static bool st7701s_mipi_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        // only turn on/off here, need hardware to support PWM control.
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
    }

    return true;
}

static bool st7701s_mipi_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static panel_ops_t st7701s_mipi_ops = {
    .init = st7701s_mipi_init,
    .exit = st7701s_mipi_exit,
    .power_on = st7701s_mipi_power_on,
    .power_off = st7701s_mipi_power_off,
    .reset = st7701s_mipi_reset,
    .enable_backlight = st7701s_mipi_enable_backlight,
    .disable_backlight = st7701s_mipi_disable_backlight,
    //RGB interface doesn't need enable display.
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

static st7701s_mipi_priv_t st7701s_mipi_priv = {
    .spi_initialized = false,
    .spi_bus = 0
};

panel_desc_t st7701s_mipi_desc = {
    .name = "st7701s_mipi_480x800",
    .manufacturer = "Sitronix",
    .model = "ST7701S_mipi",

    .interface = PANEL_IF_MIPI_DSI,
    .rgb_format = PANEL_RGB_FORMAT_RGB888,

    .timing = st7701s_mipi_timing,
    .gpio_config = NULL,

    .init_cmd_count = sizeof(st7701s_mipi_init_cmds) / sizeof(st7701s_mipi_init_cmds[0]),
    .init_cmds = st7701s_mipi_init_cmds,

    .ops = &st7701s_mipi_ops,
    .private_data = &st7701s_mipi_priv
};

bool panel_st7701s_mipi_register(void) {
    return panel_register(&st7701s_mipi_desc);
}
