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

#define LOG_TAG "panel_st7789v"

#define REGFLAG_DELAY          0xFC
#define REGFLAG_END_OF_TABLE   0xFD
/* sent by the spi_only controller before the pixels of every frame */
#define REGFLAG_FRAME_CMD      0xFE
/* SPI SCLK in MHz, read by the spi_only controller before the bus is set up */
#define REGFLAG_SPI_CLOCK      0xFB

/* MIPI DCS commands to write the panel GRAM. */
#define DCS_SET_COLUMN_ADDRESS 0x2A
#define DCS_SET_PAGE_ADDRESS   0x2B
#define DCS_WRITE_MEMORY_START 0x2C

#define ST7789V_WIDTH          240
#define ST7789V_HEIGHT         320

/* 16-bit window end address as the 2 DCS parameter bytes, MSB first. */
#define ST7789V_END_BYTES(n)   (((n) - 1) >> 8), (((n) - 1) & 0xFF)

/* Sent over 4-wire SPI by the spi_only controller: {cmd, param_count, params...} */
static const uint8_t st7789v_init_cmds[][64] = {
    /* SCLK 30MHz: IpClk 120MHz / 4, the divider must be even and 60MHz flickers on long wires */
    {REGFLAG_SPI_CLOCK, 1, 30},

    /* Sleep Out */
    {0x11, 0},
    {REGFLAG_DELAY, 120},

    /* MADCTL / COLMOD: RGB565 */
    {0x36, 1, 0x00},
    {0x3A, 1, 0x05},

    /* Porch / Gate / VCOM / Power */
    {0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33},
    {0xB7, 1, 0x71},
    {0xBB, 1, 0x3B},
    {0xC0, 1, 0x2C},
    {0xC2, 1, 0x01},
    {0xC3, 1, 0x13},
    {0xC4, 1, 0x20},
    {0xC6, 1, 0x0F},
    {0xD0, 2, 0xA4, 0xA1},
    {0xD6, 1, 0xA1},

    /* Positive / Negative Gamma */
    {0xE0, 14, 0xD0, 0x08, 0x0A, 0x0D, 0x0B, 0x07, 0x21, 0x33, 0x39, 0x39, 0x16, 0x16, 0x1F, 0x3C},
    {0xE1, 14, 0xD0, 0x00, 0x03, 0x01, 0x00, 0x10, 0x21, 0x32, 0x38, 0x16, 0x14, 0x14, 0x20, 0x3D},

    /* Display Inversion On */
    {0x21, 0},
    {REGFLAG_DELAY, 120},

    /* Display On */
    {0x29, 0},
    {REGFLAG_DELAY, 120},

    /* Full screen window, GRAM is written as a whole frame every flip */
    {DCS_SET_COLUMN_ADDRESS, 4, 0x00, 0x00, ST7789V_END_BYTES(ST7789V_WIDTH)},
    {DCS_SET_PAGE_ADDRESS, 4, 0x00, 0x00, ST7789V_END_BYTES(ST7789V_HEIGHT)},

    /* RAMWR before every frame, it resets the GRAM pointer to the window start */
    {REGFLAG_FRAME_CMD, 1, DCS_WRITE_MEMORY_START},

    {REGFLAG_END_OF_TABLE, 0x00},
};

// ST7789V timing parameters
static const panel_timing_t st7789v_timing = {
    .width = ST7789V_WIDTH,
    .height = ST7789V_HEIGHT,
    .clock_frequency = 60,
    // SPI interface doesn't need HSYNC/VSYNC
    .use_de = false
};

static bool st7789v_init(void *panel_data) {
    (void) panel_data;

    // init cmds are sent by the spi_only controller.
    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Initializing ST7789V panel\n");

    return true;
}

static bool st7789v_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting ST7789V panel\n");

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

static bool st7789v_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on ST7789V\n");

    // enable power.
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        //wait for power to be stable.
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool st7789v_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off ST7789V\n");

    // power off
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool st7789v_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting ST7789V\n");

    if (panel->desc->gpio_config->reset_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(100);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 0);
        rtos_time_delay_ms(100);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(120);
    }

    return true;
}

static bool st7789v_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        // only turn on/off here, need hardware to support PWM control.
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
    }

    return true;
}

static bool st7789v_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static bool st7789v_enable_display(void *panel_data) {
    (void) panel_data;

    // Display On is already in init cmds.
    return true;
}

static panel_ops_t st7789v_ops = {
    .init = st7789v_init,
    .exit = st7789v_exit,
    .power_on = st7789v_power_on,
    .power_off = st7789v_power_off,
    .reset = st7789v_reset,
    .enable_backlight = st7789v_enable_backlight,
    .disable_backlight = st7789v_disable_backlight,
    .enable_display = st7789v_enable_display,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

panel_desc_t st7789v_desc = {
    .name = "st7789v_spi_240x320",
    .manufacturer = "Sitronix",
    .model = "ST7789V",

    .interface = PANEL_IF_SPI,
    .rgb_format = PANEL_RGB_FORMAT_RGB565,
    .lane_count = 1,

    .timing = st7789v_timing,
    .gpio_config = NULL,
    .spi_config = NULL,

    .init_cmd_count = sizeof(st7789v_init_cmds) / sizeof(st7789v_init_cmds[0]),
    .init_cmds = st7789v_init_cmds,

    .ops = &st7789v_ops,
    .private_data = NULL
};

bool panel_st7789v_register(void) {
    return panel_register(&st7789v_desc);
}
