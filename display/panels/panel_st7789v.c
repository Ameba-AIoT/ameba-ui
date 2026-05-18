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

#include "panel_manager.h"

#define LOG_TAG "panel_st7789v"

static const uint8_t st7789v_init_cmds[][32] = {
    {0x00, 0x00, 0x00},
};

// ST7789V GPIO config (SPI interface)
static panel_gpio_config_t st7789v_gpio_config = {
    .reset_pin = _PA_18,
    .cs_pin = _PB_14,     // SPI CS
    .dc_pin = _PB_15,     // SPI DC
    .bl_pin = _PB_3,
    .power_en_pin = _PA_17
};

// ST7789V timing parameters（240x320）
static panel_timing_t st7789v_timing = {
    .width = 240,
    .height = 320,
    .clock_frequency = 60,
    // SPI interface doesn't need HSYNC/VSYNC
    .use_de = false
};

typedef struct {
    spi_t spi;
    uint32_t spi_speed;
    bool spi_initialized;
} st7789v_priv_t;

// SPI send
static void st7789v_spi_write_cmd(uint8_t cmd) {
    // SPI implement
}

static void st7789v_spi_write_data(uint8_t data) {
    // SPI implement
}

// ST7789V init
static bool st7789v_init(void *panel_data) {
    // SPI init
    return true;
}

static panel_ops_t st7789v_ops = {
    .init = st7789v_init,
    // ... other functions.
};

panel_desc_t st7789v_desc = {
    .name = "st7789v_spi_240x320",
    .manufacturer = "Sitronix",
    .model = "ST7789V",

    .interface = PANEL_IF_SPI,
    .rgb_format = PANEL_RGB_FORMAT_RGB565,

    .timing = st7789v_timing,
    .gpio_config = st7789v_gpio_config,

    .init_cmd_count = sizeof(st7789v_init_cmds) / sizeof(st7789v_init_cmds[0]),
    .init_cmds = st7789v_init_cmds,

    .ops = &st7789v_ops,
    .private_data = NULL
};

bool panel_st7789v_register(void) {
    return panel_register(&st7789v_desc);
}