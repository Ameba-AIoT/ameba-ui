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

#include "ameba_soc.h"

#include "panel_manager.h"
#include "panel_pin_config.h"

#define LOG_TAG "PanelPinConfig"

// demo board default pinmux
pin_config_entry_t default_pin_configs[] = {
    {
        .panel_name = "hj3508_12_rgb_320x480",
        .gpio_config = {
            .status = true,
            .reset_pin = _PA_15,
            .cs_pin = 0xFFFFFFFF,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = 0xFFFFFFFF,
            .power_en_pin = 0xFFFFFFFF
        },
        .spi_config = {
            .status = true,
            .spi_index = 1,
            .cs_pin = _PB_4,
            .sclk_pin = _PB_7,
            .mosi_pin = _PB_5,
            .miso_pin = 0xFFFFFFFF
        },
        .rgb_data_config = {
            .status = true,
            .data_pins = {
                0xFFFFFFFF, // D0
                0xFFFFFFFF, // D1
                _PB_9,      // D2-B0
                _PB_11,     // D3
                _PB_10,     // D4
                _PB_16,     // D5
                _PB_22,     // D6
                _PB_23,     // D7
                0xFFFFFFFF, // D8
                0xFFFFFFFF, // D9
                _PB_14,     // D10-G0
                _PB_12,     // D11
                _PB_15,     // D12
                _PB_17,     // D13
                _PB_21,     // D14
                _PB_18,     // D15
                0xFFFFFFFF, // D16
                0xFFFFFFFF, // D17
                _PA_6,      // D18-R0
                _PA_8,      // D19
                _PA_7,      // D20
                _PA_10,     // D21
                _PA_11,     // D22
                _PA_9,      // D23
            },
        },
        .rgb_ctrl_config = {
            .status = true,
            .hsync_pin = _PA_14,
            .vsync_pin = _PB_13,
            .dclk_pin = _PA_16,
            .de_pin = _PA_13,
        },
    },
    {
        .panel_name = "st7701s_rgb_800x480",
        .gpio_config = {
            .status = true,
            .reset_pin = 0xFFFFFFFF,
            .cs_pin = 0xFFFFFFFF,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = _PB_3,
            .power_en_pin = _PA_17
        },
        .spi_config = {
            .status = false,
        },
        .rgb_data_config = {
            .status = true,
            .data_pins = {
                _PB_15,  // D0
                _PB_17,  // D1
                _PB_21,  // D2
                _PB_18,  // D3
                _PA_6,   // D4
                _PA_8,   // D5
                _PA_7,   // D6
                _PA_10,  // D7
                _PB_9,   // D8
                _PB_11,  // D9
                _PB_10,  // D10
                _PB_16,  // D11
                _PB_22,  // D12
                _PB_23,  // D13
                _PB_14,  // D14
                _PB_12,  // D15
                _PA_22,  // D16
                _PA_25,  // D17
                _PA_29,  // D18
                _PB_4,   // D19
                _PB_5,   // D20
                _PB_6,   // D21
                _PB_7,   // D22
                _PB_8    // D23
            },
        },
        .rgb_ctrl_config = {
            .status = true,
            .hsync_pin = _PA_16,
            .vsync_pin = _PA_13,
            .dclk_pin = _PA_9,
            .de_pin = _PA_14,
        },
    },
    {
        .panel_name = "b1620a_720x720",
        .gpio_config = {
            .status = true,
            .reset_pin = _PA_22,
            .cs_pin = _PA_19,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = _PA_25,
            .power_en_pin = 0xFFFFFFFF
        },
        .spi_config = {
            .status = true,
            .spi_index = 1,
            .cs_pin = _PA_19,
            .sclk_pin = _PA_18,
            .mosi_pin = _PA_17,
            .miso_pin = 0xFFFFFFFF
        },
        .rgb_data_config = {
            .status = true,
            .data_pins = {
                0xFFFFFFFF,     // D0
                0xFFFFFFFF,     // D1
                _PA_12,     // D2-B0
                _PC_1,      // D3-B1
                _PC_0,      // D4-B2
                _PB_31,     // D5-B3
                _PB_30,     // D6-B4
                _PB_29,     // D7-B5
                0xFFFFFFFF,     // D8
                0xFFFFFFFF,     // D9
                _PB_28,     // D10-G0
                _PB_27,     // D11-G1
                _PB_26,     // D12-G2
                _PB_25,     // D13-G3
                _PB_24,     // D14-G4
                _PB_23,     // D15-G5
                0xFFFFFFFF,     // D16
                0xFFFFFFFF,     // D17
                _PB_22,     // D18-R0
                _PB_21,     // D19-R1
                _PB_19,     // D20-R2
                _PB_18,     // D21-R3
                _PB_17,     // D22-R4
                _PB_16,     // D23-R5
            },
        },
        .rgb_ctrl_config = {
            .status = true,
            .hsync_pin = _PA_13,
            .vsync_pin = _PA_14,
            .dclk_pin = _PA_16,
            .de_pin = _PA_15,
        },
    },
    {
        .panel_name = "t1720a_800x480",
        .gpio_config = {
            .status = true,
            .reset_pin = 0xFFFFFFFF,
            .cs_pin = 0xFFFFFFFF,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = _PA_25,
            .power_en_pin = _PA_23
        },
        .spi_config = {
            .status = false,
        },
        .rgb_data_config = {
            .status = true,
            .data_pins = {
                _PB_31,  // D0
                _PC_0,  // D1
                _PC_1,  // D2
                _PA_12,  // D3
                _PA_13,   // D4
                _PA_14,   // D5
                _PA_15,   // D6
                _PA_16,  // D7
                _PB_23,   // D8
                _PB_24,  // D9
                _PB_25,  // D10
                _PB_26,  // D11
                _PB_27,  // D12
                _PB_28,  // D13
                _PB_29,  // D14
                _PB_30,  // D15
                _PB_14,  // D16
                _PB_15,  // D17
                _PB_16,  // D18
                _PB_17,   // D19
                _PB_18,   // D20
                _PB_19,   // D21
                _PB_21,   // D22
                _PB_22    // D23
            },
        },
        .rgb_ctrl_config = {
            .status = true,
            .hsync_pin = 0xFFFFFFFF,
            .vsync_pin = 0xFFFFFFFF,
            .dclk_pin = _PB_13,
            .de_pin = _PA_17,
        },
    },
    {
        .panel_name = "st7701s_rgb565_480x480",
        .gpio_config = {
            .status = true,
            .reset_pin = _PA_4,
            .cs_pin = 0xFFFFFFFF,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = _PC_0,
            .power_en_pin = 0xFFFFFFFF
        },
        .spi_config = {
            .status = true,
            .spi_index = 1,
            .cs_pin = _PB_27,
            .sclk_pin = _PB_28,
            .mosi_pin = _PB_29,
            .miso_pin = 0xFFFFFFFF
        },
        .rgb_data_config = {
            .status = true,
            .data_pins = {
                _PA_22,  // D0
                _PA_21,  // D1
                _PA_20,  // D2
                _PA_19,  // D3
                _PA_16,  // D4
                _PA_15,  // D5
                _PA_14,  // D6
                _PA_13,  // D7
                _PA_12,  // D8
                _PA_11,  // D9
                _PA_10,  // D10
                _PA_9,   // D11
                _PA_8,   // D12
                _PA_7,   // D13
                _PA_6,   // D14
                _PA_5,   // D15
            },
        },
        .rgb_ctrl_config = {
            .status = true,
            .hsync_pin = _PA_25,
            .vsync_pin = _PA_26,
            .dclk_pin = _PA_24,
            .de_pin = _PA_23,
        },
    },
    {
        .panel_name = "st7701p_rgb_480x480",
        .gpio_config = {
            .status = true,
            .reset_pin = _PA_23,
            .cs_pin = 0xFFFFFFFF,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = _PA_25,
            .power_en_pin = 0xFFFFFFFF
        },
        .spi_config = {
            .status = true,
            .spi_index = 1,
            .cs_pin = _PA_20,
            .sclk_pin = _PA_21,
            .mosi_pin = _PA_22,
            .miso_pin = 0xFFFFFFFF
        },
        .rgb_data_config = {
            .status = true,
            .data_pins = {
                _PA_16,  // D0
                _PA_15,  // D1
                _PA_14,  // D2
                _PA_13,  // D3
                _PA_12,  // D4
                _PC_1,  // D5
                _PC_0,  // D6
                _PB_31,  // D7
                _PB_30,  // D8
                _PB_29,  // D9
                _PB_28,  // D10
                _PB_27,   // D11
                _PB_26,   // D12
                _PB_25,   // D13
                _PB_24,   // D14
                _PB_23,   // D15
                _PB_22,  // D16
                _PB_21,  // D17
                _PB_19,  // D18
                _PB_18,   // D19
                _PB_17,   // D20
                _PB_16,   // D21
                _PB_15,   // D22
                _PB_14    // D23
            },
        },
        .rgb_ctrl_config = {
            .status = true,
            .hsync_pin = _PA_19,
            .vsync_pin = _PA_18,
            .dclk_pin = _PB_13,
            .de_pin = _PA_17,
        },
    },
    {
        .panel_name = "jd9165ba_1024x600",
        .gpio_config = {
            .status = true,
            .reset_pin = _PA_23,
            .cs_pin = 0xFFFFFFFF,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = _PA_25,
            .power_en_pin = 0xFFFFFFFF
        },
        .spi_config = {
            .status = false,
        },
        .rgb_data_config = {
            .status = true,
            .data_pins = {
                _PB_31,  // D0-B0
                _PC_0,  // D1
                _PC_1,  // D2
                _PA_12,  // D3
                _PA_13,  // D4
                _PA_14,  // D5
                _PA_15,  // D6
                _PA_16,  // D7
                _PB_23,  // D8-G0
                _PB_24,  // D9
                _PB_25,  // D10
                _PB_26,   // D11
                _PB_27,   // D12
                _PB_28,   // D13
                _PB_29,   // D14
                _PB_30,   // D15
                _PB_14,  // D16-R0
                _PB_15,  // D17
                _PB_16,  // D18
                _PB_17,   // D19
                _PB_18,   // D20
                _PB_19,   // D21
                _PB_21,   // D22
                _PB_22    // D23
            },
        },
        .rgb_ctrl_config = {
            .status = true,
            .hsync_pin = _PA_19,
            .vsync_pin = _PA_18,
            .dclk_pin = _PB_13,
            .de_pin = _PA_17,
        },
    },
        {
        .panel_name = "st7701s_mipi_480x800",
        .gpio_config = {
            .status = true,
            .reset_pin = _PA_14,
            .cs_pin = 0xFFFFFFFF,
            .dc_pin = 0xFFFFFFFF,
            .bl_pin = 0xFFFFFFFF,
            .power_en_pin = 0xFFFFFFFF
        },
        .spi_config = {
            .status = false,
        },
        .rgb_data_config = {
            .status = false,
        },
        .rgb_ctrl_config = {
            .status = false,
        },
    },
};

int32_t default_pin_configs_num = sizeof(default_pin_configs) / sizeof(default_pin_configs[0]);

panel_gpio_config_t *panel_get_gpio_config(const char *panel_name) {
    uint32_t pin_configs_cnt = sizeof(default_pin_configs) / sizeof(default_pin_configs[0]);

    for (uint32_t i = 0; i < pin_configs_cnt; i++) {
        if (strcmp(default_pin_configs[i].panel_name, panel_name) == 0) {
            return &default_pin_configs[i].gpio_config;
        }
    }

    return NULL;
}

panel_spi_config_t *panel_get_spi_config(const char *panel_name) {
    uint32_t pin_configs_cnt = sizeof(default_pin_configs) / sizeof(default_pin_configs[0]);

    for (uint32_t i = 0; i < pin_configs_cnt; i++) {
        if (strcmp(default_pin_configs[i].panel_name, panel_name) == 0) {
            return &default_pin_configs[i].spi_config;
        }
    }

    return NULL;
}