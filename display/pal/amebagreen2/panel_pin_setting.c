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

void panel_configure_pinmux(const char *panel_name) {

    uint32_t pin_configs_cnt = default_pin_configs_num;

    for (uint32_t i = 0; i < pin_configs_cnt; i++) {

        if (strcmp(default_pin_configs[i].panel_name, panel_name) == 0) {

            if (default_pin_configs[i].rgb_data_config.status) {
                for (int j = 0; j < 24; j++) {
                    if (default_pin_configs[i].rgb_data_config.data_pins[j] != 0xFFFFFFFF) {
                        Pinmux_Config(default_pin_configs[i].rgb_data_config.data_pins[j], (PINMUX_FUNCTION_LCD_D0 + j));
                    }
                }
            }

            if (default_pin_configs[i].rgb_ctrl_config.status) {
                Pinmux_Config(default_pin_configs[i].rgb_ctrl_config.hsync_pin, PINMUX_FUNCTION_LCD_RGB_HSYNC);
                Pinmux_Config(default_pin_configs[i].rgb_ctrl_config.vsync_pin, PINMUX_FUNCTION_LCD_RGB_VSYNC);
                Pinmux_Config(default_pin_configs[i].rgb_ctrl_config.dclk_pin, PINMUX_FUNCTION_LCD_RGB_DCLK);
                Pinmux_Config(default_pin_configs[i].rgb_ctrl_config.de_pin, PINMUX_FUNCTION_LCD_RGB_DE);
            }

            if (default_pin_configs[i].gpio_config.status) {
                if (default_pin_configs[i].gpio_config.reset_pin != 0xFFFFFFFF) {
                    GPIO_InitTypeDef gpio_init = {0};
                    gpio_init.GPIO_Pin = default_pin_configs[i].gpio_config.reset_pin;
                    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
                    GPIO_Init(&gpio_init);
                }

                if (default_pin_configs[i].gpio_config.bl_pin != 0xFFFFFFFF) {
                    GPIO_InitTypeDef gpio_init = {0};
                    gpio_init.GPIO_Pin = default_pin_configs[i].gpio_config.bl_pin;
                    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
                    GPIO_Init(&gpio_init);
                    GPIO_WriteBit(default_pin_configs[i].gpio_config.bl_pin, 0);
                }

                if (default_pin_configs[i].gpio_config.power_en_pin != 0xFFFFFFFF) {
                    GPIO_InitTypeDef gpio_init = {0};
                    gpio_init.GPIO_Pin = default_pin_configs[i].gpio_config.power_en_pin;
                    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
                    GPIO_Init(&gpio_init);
                    GPIO_WriteBit(default_pin_configs[i].gpio_config.power_en_pin, 0);
                }
            }

            if (default_pin_configs[i].spi_config.status) {
                if (default_pin_configs[i].spi_config.cs_pin != 0xFFFFFFFF) {
                    Pinmux_Config(default_pin_configs[i].spi_config.cs_pin, PINMUX_FUNCTION_SPI1_CS);
                }
                if (default_pin_configs[i].spi_config.sclk_pin != 0xFFFFFFFF) {
                    Pinmux_Config(default_pin_configs[i].spi_config.sclk_pin, PINMUX_FUNCTION_SPI1_CLK);
                }
                if (default_pin_configs[i].spi_config.mosi_pin != 0xFFFFFFFF) {
                    Pinmux_Config(default_pin_configs[i].spi_config.mosi_pin, PINMUX_FUNCTION_SPI1_MOSI);
                }
                if (default_pin_configs[i].spi_config.miso_pin != 0xFFFFFFFF) {
                    Pinmux_Config(default_pin_configs[i].spi_config.miso_pin, PINMUX_FUNCTION_SPI1_MISO);
                }
            }

            return;
        }

    }
}