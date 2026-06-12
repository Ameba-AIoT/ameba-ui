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

#define LOG_TAG "panel_st7102_mipi"

/* Must match lcdc_mipi.c controller definitions */
#define REGFLAG_DELAY          0xFC
#define REGFLAG_END_OF_TABLE   0xFD

/*
 * ST7102 MIPI DSI 480x480 init sequence.
 * Converted from SSD2828 bridge format (screen/ST7102_480480_60hz_0109_tp+3ms+.txt):
 *   SSD_SEND(0x01, CMD, PARAM...)  →  {CMD, count, PARAM...}
 */
static const uint8_t st7102_mipi_init_cmds[][64] = {
    /* KEY unlock */
    {0x99, 3, 0x71, 0x02, 0xA2},
    {0x99, 3, 0x71, 0x02, 0xA3},
    {0x99, 3, 0x71, 0x02, 0xA4},

    /* 1-lane internal setting: 0x30 means 1-lane, 0x31 means 2-lane. Hardware wires 1-lane */
    {0xA4, 1, 0x30},

    /* Power - VGH/VGL */
    {0xB0, 7, 0x22, 0x61, 0x1E, 0x61, 0x2F, 0x39, 0x39},
    /* Source */
    {0xB7, 2, 0x46, 0x46},
    /* VCOM */
    {0xBF, 2, 0x43, 0x43},

    /* GIP */
    {0xD7, 6, 0x00, 0x10, 0x8C, 0x08, 0xF0, 0xF0},

    {0xA3, 32,
     0x40, 0x03, 0x8C, 0x40, 0x45, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x1E, 0x01, 0x00, 0x12, 0x00, 0x45,
     0x05, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x01, 0x00,
     0x12, 0x20, 0x52, 0x00, 0x05, 0x00, 0x00, 0xFF},

    {0xA6, 44,
     0x08, 0x00, 0x24, 0x55, 0x35, 0x00, 0x76, 0x40,
     0x4E, 0x4E, 0x00, 0x24, 0x55, 0x00, 0x00, 0x40,
     0x40, 0x4E, 0x4E, 0x02, 0xAC, 0x51, 0x00, 0xCC,
     0x40, 0x40, 0x4E, 0x4E, 0x00, 0xAC, 0x11, 0x00,
     0x00, 0x40, 0x40, 0x4E, 0x4E, 0x00, 0x00, 0x06,
     0x00, 0x00, 0x00, 0x00},

    {0xA7, 48,
     0x19, 0x19, 0x00, 0x64, 0x40, 0x07, 0x16, 0x40,
     0x00, 0x44, 0x43, 0x4E, 0x4E, 0x00, 0x64, 0x40,
     0x25, 0x34, 0x00, 0x00, 0x42, 0x41, 0x4E, 0x4E,
     0x00, 0x64, 0x40, 0x4B, 0x5A, 0x00, 0x00, 0x42,
     0x41, 0x4E, 0x4E, 0x00, 0x24, 0x40, 0x69, 0x78,
     0x00, 0x00, 0x40, 0x40, 0x4E, 0x4E, 0x00, 0x44},

    {0xAC, 37,
     0x00, 0x1C, 0x04, 0x1A, 0x19, 0x1B, 0x1B, 0x18,
     0x06, 0x13, 0x19, 0x11, 0x1B, 0x08, 0x18, 0x0A,
     0x01, 0x1C, 0x04, 0x1A, 0x19, 0x1B, 0x1B, 0x18,
     0x06, 0x12, 0x19, 0x10, 0x1B, 0x09, 0x18, 0x0B,
     0xBF, 0xAA, 0xBF, 0xAA, 0x00},

    {0xAD, 7, 0xCC, 0x40, 0x46, 0x11, 0x04, 0x6F, 0x6F},

    {0xE8, 14,
     0x30, 0x07, 0x05, 0x6A, 0x6A, 0x9C, 0x00, 0xE2,
     0x04, 0x00, 0x00, 0x00, 0x00, 0xEF},

    {0x75, 2, 0x03, 0x04},

    {0xE7, 33,
     0x8B, 0x3C, 0x00, 0x0C, 0xF0, 0x5D, 0x00, 0x5D,
     0x00, 0x5D, 0x00, 0x5D, 0x00, 0xFF, 0x00, 0x08,
     0x7B, 0x00, 0x00, 0xC8, 0x6A, 0x5A, 0x08, 0x1A,
     0x3C, 0x00, 0xA1, 0x01, 0x8C, 0x01, 0x7F, 0xF0,
     0x22},

    {0xE9, 9,
     0x3C, 0x7F, 0x08, 0x10, 0x1A, 0x7A, 0x22, 0x1A,
     0x33},

    /* Gamma positive */
    {0xC8, 37,
     0x00, 0x00, 0x15, 0x26, 0x44, 0x00, 0x78, 0x03,
     0xBE, 0x06, 0x11, 0x1C, 0x09, 0x8A, 0x03, 0x21,
     0xD4, 0x01, 0x11, 0x0F, 0x22, 0x4A, 0x0F, 0x8F,
     0x0A, 0x32, 0xF0, 0x0A, 0x41, 0x0D, 0xF3, 0x80,
     0x0D, 0xAE, 0xC5, 0x03, 0xC4},

    /* Gamma negative */
    {0xC9, 37,
     0x00, 0x00, 0x15, 0x26, 0x44, 0x00, 0x78, 0x03,
     0xBE, 0x06, 0x11, 0x1C, 0x09, 0x8A, 0x03, 0x21,
     0xD4, 0x01, 0x11, 0x0F, 0x22, 0x4A, 0x0F, 0x8F,
     0x0A, 0x32, 0xF0, 0x0A, 0x41, 0x0D, 0xF3, 0x80,
     0x0D, 0xAE, 0xC5, 0x03, 0xC4},

    /* TE on */
    {0x35, 1, 0x00},

    /* Sleep Out */
    {0x11, 0},
    {REGFLAG_DELAY, 250},
    {REGFLAG_DELAY, 250},
    {REGFLAG_DELAY, 250},
    {REGFLAG_DELAY, 50},

    /* Display On */
    {0x29, 0},
    {REGFLAG_DELAY, 150},

    /* End */
    {REGFLAG_END_OF_TABLE, 0x00},
};

static const panel_timing_t st7102_mipi_timing = {
    .width = 480,
    .height = 480,
    .hsync_front_porch = 40,
    .hsync_back_porch = 40,
    .hsync_pulse_width = 2,
    .vsync_front_porch = 125,
    .vsync_back_porch = 14,
    .vsync_pulse_width = 2,
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
} st7102_mipi_priv_t;

static bool st7102_mipi_init(void *panel_data) {
    (void) panel_data;
    return true;
}

static bool st7102_mipi_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting ST7102_mipi panel\n");

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

static bool st7102_mipi_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on ST7102_mipi\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        //wait for power to be stable.
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool st7102_mipi_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off ST7102_mipi\n");

    // power off
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool st7102_mipi_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting ST7102_mipi\n");

    if (panel->desc->gpio_config->reset_pin != 0xFFFFFFFF) {
        /* Reset sequence: PA14 low >= 10ms, then high >= 120ms */
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 0);
        rtos_time_delay_ms(10);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(120);
    }

    return true;
}

static bool st7102_mipi_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        // only turn on/off here, need hardware to support PWM control.
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
    }

    return true;
}

static bool st7102_mipi_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static panel_ops_t st7102_mipi_ops = {
    .init = st7102_mipi_init,
    .exit = st7102_mipi_exit,
    .power_on = st7102_mipi_power_on,
    .power_off = st7102_mipi_power_off,
    .reset = st7102_mipi_reset,
    .enable_backlight = st7102_mipi_enable_backlight,
    .disable_backlight = st7102_mipi_disable_backlight,
    //RGB interface doesn't need enable display.
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

static st7102_mipi_priv_t st7102_mipi_priv = {
    .spi_initialized = false,
    .spi_bus = 0
};

panel_desc_t st7102_mipi_desc = {
    .name = "st7102_mipi_480x480",
    .manufacturer = "Sitronix",
    .model = "ST7102_mipi",

    .interface = PANEL_IF_MIPI_DSI,
    .rgb_format = PANEL_RGB_FORMAT_RGB888,

    .timing = st7102_mipi_timing,

    .lane_count = 1,
    .gpio_config = NULL,

    .init_cmd_count = sizeof(st7102_mipi_init_cmds) / sizeof(st7102_mipi_init_cmds[0]),
    .init_cmds = st7102_mipi_init_cmds,

    .ops = &st7102_mipi_ops,
    .private_data = &st7102_mipi_priv
};

bool panel_st7102_mipi_register(void) {
    return panel_register(&st7102_mipi_desc);
}
