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

#ifndef AMEBA_UI_LVGL_LV_DRIVERS_PANEL_SPI_INIT_H
#define AMEBA_UI_LVGL_LV_DRIVERS_PANEL_SPI_INIT_H

#include <stdint.h>
#include <stdbool.h>

#include "panel_manager.h"

struct spi_t;

void spi_write_command(spi_t *spi, u16 cmd);
void spi_write_data(spi_t *spi,u16 data);

void spi_driver_init(spi_t *spi, panel_spi_config_t *spi_config);

#endif // AMEBA_UI_LVGL_LV_DRIVERS_PANEL_SPI_INIT_H