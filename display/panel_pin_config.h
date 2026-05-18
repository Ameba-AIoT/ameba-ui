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

#ifndef AMEBA_UI_DISPLAY_PANEL_PIN_CONFIG_H
#define AMEBA_UI_DISPLAY_PANEL_PIN_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *panel_name;
    panel_gpio_config_t gpio_config;
    panel_spi_config_t spi_config;
    panel_rgb_data_config_t rgb_data_config;
    panel_rgb_ctrl_config_t rgb_ctrl_config;
} pin_config_entry_t;

void panel_configure_pinmux(const char *panel_name);
panel_gpio_config_t *panel_get_gpio_config(const char *panel_name);
panel_spi_config_t *panel_get_spi_config(const char *panel_name);

extern pin_config_entry_t default_pin_configs[];
extern int32_t default_pin_configs_num;

#endif // AMEBA_UI_DISPLAY_PANEL_PIN_CONFIG_H
