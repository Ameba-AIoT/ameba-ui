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

#define LOG_TAG "panel_gh7002"

/* Must match lcdc_mipi.c controller definitions */
#define REGFLAG_DELAY          0xFC
#define REGFLAG_END_OF_TABLE   0xFD

/*
 * GH7002 MIPI-DSI 1024x600 panel (Driver IC: gh7002-01, Screen: hj7001-02).
 * Converted from Linux driver drivers/rtkdrivers/drm/panel/panel-gh7002.c
 * gh7002_initialization[] table:
 *   {MIPI_DSI_DCS_SHORT_WRITE_PARAM, 2, {reg, val}} -> {reg, 1, {val}}
 *   {MIPI_DSI_DCS_SHORT_WRITE, 1, {cmd}}             -> {cmd, 0, {}}
 */
static const uint8_t gh7002_mipi_init_cmds[][64] = {
    /* PAGE1 */
    {0xee, 1, 0x01},
    {0xea, 1, 0x07},
    {0xeb, 1, 0x12},
    {0x0a, 1, 0x55},
    {0x0c, 1, 0x70},
    {0x13, 1, 0x14},
    {0x15, 1, 0x58},
    {0x17, 1, 0x32},
    {0x1d, 1, 0x33},
    {0x21, 1, 0x01},
    {0x28, 1, 0x23},
    {0x29, 1, 0x23},
    {0x2a, 1, 0x03},
    {0x2f, 1, 0xf3},

    /* PAGE2: gamma */
    {0xee, 1, 0x02},
    {0x39, 1, 0xb0},

    {0x00, 1, 0x00},
    {0x01, 1, 0x11},
    {0x02, 1, 0x18},
    {0x03, 1, 0x0D},
    {0x04, 1, 0x15},
    {0x05, 1, 0x35},
    {0x06, 1, 0x0e},
    {0x07, 1, 0x10},
    {0x08, 1, 0x11},
    {0x09, 1, 0x0e},
    {0x0a, 1, 0x12},
    {0x0b, 1, 0x55},
    {0x0c, 1, 0x12},
    {0x0d, 1, 0x15},
    {0x0e, 1, 0x3a},
    {0x0f, 1, 0x3d},
    {0x10, 1, 0x3f},

    {0x20, 1, 0x00},
    {0x21, 1, 0x11},
    {0x22, 1, 0x18},
    {0x23, 1, 0x0d},
    {0x24, 1, 0x15},
    {0x25, 1, 0x35},
    {0x26, 1, 0x0e},
    {0x27, 1, 0x10},
    {0x28, 1, 0x11},
    {0x29, 1, 0x0e},
    {0x2a, 1, 0x12},
    {0x2b, 1, 0x55},
    {0x2c, 1, 0x12},
    {0x2d, 1, 0x15},
    {0x2e, 1, 0x3a},
    {0x2f, 1, 0x3d},
    {0x30, 1, 0x3f},

    /* PAGE3 */
    {0xee, 1, 0x03},
    {0x0f, 1, 0xb9},

    /* PAGE4: source/gate driver control */
    {0xee, 1, 0x04},
    {0x00, 1, 0x05},
    {0x01, 1, 0x01},
    {0x02, 1, 0x2C},
    {0x03, 1, 0x04},
    {0x04, 1, 0x00},
    {0x06, 1, 0x06},
    {0x07, 1, 0x05},
    {0x08, 1, 0x15},
    {0x09, 1, 0x20},
    {0x0a, 1, 0x0a},
    {0x0b, 1, 0x07},
    {0x0f, 1, 0x0a},
    {0x19, 1, 0xcc},
    {0x1a, 1, 0xcc},
    {0x20, 1, 0x40},
    {0x24, 1, 0x08},
    {0x25, 1, 0x02},
    {0x29, 1, 0x00},
    {0x30, 1, 0x1d},
    {0x31, 1, 0x1d},
    {0x37, 1, 0x22},
    {0x40, 1, 0x80},
    {0x41, 1, 0x55},

    /* PAGE5: gate timing */
    {0xee, 1, 0x05},
    {0x00, 1, 0x01},
    {0x01, 1, 0x05},
    {0x02, 1, 0x45},
    {0x03, 1, 0x05},
    {0x07, 1, 0xBD},
    {0x08, 1, 0xC1},
    {0x09, 1, 0x44},
    {0x10, 1, 0x03},
    {0x11, 1, 0x07},
    {0x12, 1, 0x45},
    {0x13, 1, 0x05},
    {0x19, 1, 0xBB},
    {0x1a, 1, 0x74},
    {0x30, 1, 0x01},
    {0x31, 1, 0x01},
    {0x32, 1, 0x00},
    {0x33, 1, 0x14},
    {0x34, 1, 0x14},
    {0x35, 1, 0x78},
    {0x36, 1, 0x01},
    {0x37, 1, 0x01},
    {0x38, 1, 0x00},
    {0x39, 1, 0x14},
    {0x3A, 1, 0x14},
    {0x40, 1, 0xEE},
    {0x41, 1, 0x44},
    {0x43, 1, 0x13},
    {0x44, 1, 0x01},
    {0x45, 1, 0x81},
    {0x46, 1, 0x06},
    {0x47, 1, 0x00},

    /* PAGE6: GIP back */
    {0xee, 1, 0x06},
    {0x00, 1, 0x01},
    {0x02, 1, 0x45},
    {0x06, 1, 0xcd},
    {0x08, 1, 0x67},
    {0x09, 1, 0x45},
    {0x0a, 1, 0x23},
    {0x0b, 1, 0x01},

    /* PAGE7: GIP left/right pins */
    {0xee, 1, 0x07},
    {0x00, 1, 0x01},
    {0x01, 1, 0x05},
    {0x02, 1, 0x0C},
    {0x03, 1, 0x0D},
    {0x04, 1, 0x3c},
    {0x05, 1, 0x21},
    {0x06, 1, 0x20},
    {0x07, 1, 0x12},
    {0x08, 1, 0x10},
    {0x09, 1, 0x16},
    {0x0A, 1, 0x14},
    {0x0b, 1, 0x3C},
    {0x0c, 1, 0x3C},
    {0x0d, 1, 0x3C},
    {0x0e, 1, 0x3C},
    {0x0f, 1, 0x3C},
    {0x10, 1, 0x3c},
    {0x11, 1, 0x3c},
    {0x12, 1, 0x3c},
    {0x13, 1, 0x3c},
    {0x14, 1, 0x3c},
    {0x15, 1, 0x3c},

    {0x20, 1, 0x00},
    {0x21, 1, 0x04},
    {0x22, 1, 0x0C},
    {0x23, 1, 0x0D},
    {0x24, 1, 0x3c},
    {0x25, 1, 0x21},
    {0x26, 1, 0x20},
    {0x27, 1, 0x13},
    {0x28, 1, 0x11},
    {0x29, 1, 0x17},
    {0x2A, 1, 0x15},
    {0x2b, 1, 0x3C},
    {0x2c, 1, 0x3C},
    {0x2d, 1, 0x3C},
    {0x2e, 1, 0x3C},
    {0x2f, 1, 0x3C},
    {0x30, 1, 0x3c},
    {0x31, 1, 0x3c},
    {0x32, 1, 0x3c},
    {0x33, 1, 0x3c},
    {0x34, 1, 0x3c},
    {0x35, 1, 0x3c},

    /* PAGE8: power */
    {0xee, 1, 0x08},
    {0x10, 1, 0x00},
    {0x12, 1, 0xda},
    {0x13, 1, 0x1c},
    {0x18, 1, 0x10},
    {0x20, 1, 0x80},

    /* PAGEf: dual-gate enable */
    {0xee, 1, 0x0f},
    {0x00, 1, 0x01},
    {0x03, 1, 0x95},

    /* PAGE0: finalise */
    {0xee, 1, 0x00},
    {0xea, 1, 0x00},
    {0xeb, 1, 0x00},
    {0x36, 1, 0x00},

    /* Sleep Out */
    {0x11, 0},
    {REGFLAG_DELAY, 120},

    /* Display On */
    {0x29, 0},
    {REGFLAG_DELAY, 20},

    /* End */
    {REGFLAG_END_OF_TABLE, 0x00},
};

static const panel_timing_t gh7002_mipi_timing = {
    .width = 1024,
    .height = 600,
    .hsync_front_porch = 120,
    .hsync_back_porch = 60,
    .hsync_pulse_width = 10,
    .vsync_front_porch = 12,
    .vsync_back_porch = 10,
    .vsync_pulse_width = 10,
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
} gh7002_priv_t;

static bool gh7002_init(void *panel_data) {
    (void) panel_data;
    return true;
}

static bool gh7002_exit(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Exiting GH7002 panel\n");

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

static bool gh7002_power_on(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering on GH7002\n");

    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 1);
        //wait for power to be stable.
        rtos_time_delay_ms(10);
    }

    panel->powered_on = true;
    return true;
}

static bool gh7002_power_off(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Powering off GH7002\n");

    // power off
    if (panel->desc->gpio_config->power_en_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->power_en_pin, 0);
    }

    panel->powered_on = false;
    return true;
}

static bool gh7002_reset(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Resetting GH7002\n");

    /*
     * Linux dsi_gpio_reset(): to prevent electric leakage,
     * high 10ms -> low 15ms -> high, then wait 120ms.
     */
    if (panel->desc->gpio_config->reset_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(10);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 0);
        rtos_time_delay_ms(15);
        GPIO_WriteBit(panel->desc->gpio_config->reset_pin, 1);
        rtos_time_delay_ms(120);
    }

    return true;
}

static bool gh7002_enable_backlight(void *panel_data, uint8_t brightness) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        // only turn on/off here, need hardware to support PWM control.
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, brightness > 0 ? 1 : 0);
        panel->backlight_on = (brightness > 0);
        panel->brightness = brightness;
    }

    return true;
}

static bool gh7002_disable_backlight(void *panel_data) {
    panel_dev_t *panel = (panel_dev_t *)panel_data;

    if (panel->desc->gpio_config->bl_pin != 0xFFFFFFFF) {
        GPIO_WriteBit(panel->desc->gpio_config->bl_pin, 0);
        panel->backlight_on = false;
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "Backlight disabled\n");
    }

    return true;
}

static panel_ops_t gh7002_ops = {
    .init = gh7002_init,
    .exit = gh7002_exit,
    .power_on = gh7002_power_on,
    .power_off = gh7002_power_off,
    .reset = gh7002_reset,
    .enable_backlight = gh7002_enable_backlight,
    .disable_backlight = gh7002_disable_backlight,
    //MIPI-DSI panel doesn't need enable display.
    .enable_display = NULL,
    .disable_display = NULL,
    .set_sleep_mode = NULL,
    .set_display_mode = NULL
};

static gh7002_priv_t gh7002_priv = {
    .spi_initialized = false,
    .spi_bus = 0
};

panel_desc_t gh7002_desc = {
    .name = "gh7002_1024x600",
    .manufacturer = "unknown",
    .model = "GH7002",

    .interface = PANEL_IF_MIPI_DSI,
    .rgb_format = PANEL_RGB_FORMAT_RGB888,

    .timing = gh7002_mipi_timing,

    .lane_count = 2,
    .gpio_config = NULL,

    .init_cmd_count = sizeof(gh7002_mipi_init_cmds) / sizeof(gh7002_mipi_init_cmds[0]),
    .init_cmds = gh7002_mipi_init_cmds,

    .ops = &gh7002_ops,
    .private_data = &gh7002_priv
};

bool panel_gh7002_register(void) {
    return panel_register(&gh7002_desc);
}
