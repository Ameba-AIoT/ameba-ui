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

#define LOG_TAG "PanelSpiInit"

void spi_write_command(spi_t *spi, u16 cmd) {
    spi_master_write(spi, cmd);
}

void spi_write_data(spi_t *spi,u16 data) {
    spi_master_write(spi, data|BIT8);
}

void spi_driver_init(spi_t *spi, panel_spi_config_t *spi_config) {
    if (spi_config->spi_index > 1) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "unsupported spi index:%d\n",
                                        spi_config->spi_index);
    }

    spi->spi_idx = spi_config->spi_index == 0 ? MBED_SPI0 : MBED_SPI1;

    spi_init(spi, spi_config->mosi_pin, spi_config->miso_pin,
                       spi_config->sclk_pin, spi_config->cs_pin);
    spi_frequency(spi, 5000000);
    spi_format(spi, 9, 3, 0);
}