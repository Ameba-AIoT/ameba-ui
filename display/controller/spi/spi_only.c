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
#include "spi_ex_api.h"

#include "panel_manager.h"
#include "spi_only.h"

#define LOG_TAG "ControllerSpiOnly"

/* init_cmds table format, same as the MIPI panels: {cmd, param_count, params...} */
#define REGFLAG_DELAY                               0xFC
#define REGFLAG_END_OF_TABLE                        0xFD
/* {REGFLAG_FRAME_CMD, 1, cmd}: cmd is not sent at init, but before the pixels of every frame. */
#define REGFLAG_FRAME_CMD                           0xFE
/* {REGFLAG_SPI_CLOCK, 1, mhz}: SPI SCLK of the panel, read before the bus is set up. */
#define REGFLAG_SPI_CLOCK                           0xFB

typedef struct {
    panel_dev_t *panel;
    spi_t spi;
    uint32_t dc_pin;
    uint32_t buffer_size;
    uint8_t frame_cmd;
    bool has_frame_cmd;
    display_driver_callback_t *callback;
    bool initialized;
} spi_only_context_t;

static spi_only_context_t spi_only_context = {0};

static void spi_only_set_frame_size(uint32_t bits) {
    SPI_TypeDef *spi_dev = SPI_DEV_TABLE[spi_only_context.spi.spi_idx & 0x01].SPIx;

    // SSI_GetDataFrameSize() returns bits, SSI_SetDataFrameSize() takes bits - 1.
    if (SSI_GetDataFrameSize(spi_dev) != bits) {
        SSI_SetDataFrameSize(spi_dev, bits - 1);
    }
}

static void spi_only_wait_idle(void) {
    SPI_TypeDef *spi_dev = SPI_DEV_TABLE[spi_only_context.spi.spi_idx & 0x01].SPIx;

    // wait for the last pixel DMA and TX FIFO to drain.
    while (*(volatile uint32_t *)&spi_only_context.spi.state & SPI_STATE_TX_BUSY);
    while (!(SSI_GetStatus(spi_dev) & SPI_BIT_TFE) || SSI_Busy(spi_dev));

    // drop the dummy rx data left by the pixel transfer.
    spi_flush_rx_fifo(&spi_only_context.spi);
}

static void spi_only_write_cmd(uint8_t cmd) {
    spi_only_wait_idle();
    spi_only_set_frame_size(8);

    GPIO_WriteBit(spi_only_context.dc_pin, 0);
    spi_master_write(&spi_only_context.spi, cmd);
    GPIO_WriteBit(spi_only_context.dc_pin, 1);
}

static void spi_only_write_data(uint8_t data) {
    spi_master_write(&spi_only_context.spi, data);
}

static void spi_only_send_init_cmds(panel_desc_t *desc) {
    for (uint32_t i = 0; i < desc->init_cmd_count; i++) {
        const uint8_t *row = desc->init_cmds[i];
        uint8_t cmd = row[0];
        uint8_t count = row[1];

        if (cmd == REGFLAG_DELAY) {
            rtos_time_delay_ms(count);
            continue;
        }

        if (cmd == REGFLAG_SPI_CLOCK) {
            continue;
        }

        if (cmd == REGFLAG_FRAME_CMD) {
            spi_only_context.frame_cmd = row[2];
            spi_only_context.has_frame_cmd = true;
            continue;
        }

        if (cmd == REGFLAG_END_OF_TABLE) {
            break;
        }

        spi_only_write_cmd(cmd);
        for (uint8_t j = 0; j < count; j++) {
            spi_only_write_data(row[2 + j]);
        }
    }
}

static int32_t spi_only_panel_bpp(panel_rgb_format_t format) {
    switch (format) {
        case PANEL_RGB_FORMAT_RGB565:
            return 16;
        case PANEL_RGB_FORMAT_RGB888:
            return 24;
        default:
            return 0;
    }
}

static void spi_only_tx_done_handler(uint32_t id, SpiIrq event) {
    (void) id;

    if (event == SpiTxIrq && spi_only_context.callback && spi_only_context.callback->vblank_handler) {
        spi_only_context.callback->vblank_handler(NULL);
    }
}

static uint32_t spi_only_get_clock(panel_desc_t *desc) {
    for (uint32_t i = 0; i < desc->init_cmd_count; i++) {
        const uint8_t *row = desc->init_cmds[i];

        if (row[0] == REGFLAG_END_OF_TABLE) {
            break;
        }

        if (row[0] == REGFLAG_SPI_CLOCK) {
            return row[2] * 1000000;
        }
    }

    return 0;
}

static void spi_only_bus_init(panel_spi_config_t *spi_config, uint32_t dc_pin, uint32_t hz) {
    spi_t *spi = &spi_only_context.spi;

    spi->spi_idx = spi_config->spi_index == 0 ? MBED_SPI0 : MBED_SPI1;
    spi_init(spi, spi_config->mosi_pin, spi_config->miso_pin, spi_config->sclk_pin, spi_config->cs_pin);
    spi_format(spi, 8, 3, 0);
    spi_frequency(spi, hz);
    spi_irq_hook(spi, (spi_irq_handler) spi_only_tx_done_handler, (uint32_t) spi);

    spi_only_context.dc_pin = dc_pin;
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.GPIO_Pin = dc_pin;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&gpio_init);
    GPIO_WriteBit(dc_pin, 1);
}

bool spi_only_controller_init(int32_t color_depth, panel_dev_t *panel) {
    if (!panel || !panel->desc) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Invalid panel device\n");
        return false;
    }

    panel_desc_t *desc = panel->desc;

    // no format conversion on the SPI path, the frame buffer must already be in the panel format.
    if (color_depth != spi_only_panel_bpp(desc->rgb_format)) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "color depth %d does not match panel format %d\n", color_depth, desc->rgb_format);
        return false;
    }

    // pixels go out as 16-bit SPI frames.
    if (color_depth != 16) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "unsupported bpp:%d\n", color_depth);
        return false;
    }

    if (!desc->spi_config || !desc->spi_config->status || !desc->gpio_config ||
        desc->gpio_config->dc_pin == 0xFFFFFFFF) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "missing spi/dc pin config: %s\n", desc->name);
        return false;
    }

    uint32_t hz = spi_only_get_clock(desc);
    if (!hz) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "missing REGFLAG_SPI_CLOCK in init cmds: %s\n", desc->name);
        return false;
    }

    spi_only_bus_init(desc->spi_config, desc->gpio_config->dc_pin, hz);
    spi_only_send_init_cmds(desc);

    spi_only_context.panel = panel;
    spi_only_context.buffer_size = desc->timing.width * desc->timing.height * color_depth / 8;
    spi_only_context.initialized = true;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "spi controller initialized with panel: %s\n", desc->name);

    return true;
}

void spi_only_do_page_flip(uint8_t *buffer) {
    if (!spi_only_context.initialized) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "spi controller not initialized\n");
        return;
    }

    if (spi_only_context.has_frame_cmd) {
        spi_only_write_cmd(spi_only_context.frame_cmd);
    }

    // 16-bit frames shift out MSB first, so little-endian RGB565 needs no byte swap.
    spi_only_set_frame_size(16);
    if (spi_master_write_stream_dma(&spi_only_context.spi, (char *) buffer, spi_only_context.buffer_size) != HAL_OK) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "pixel dma start failed\n");
    }
}

void spi_only_register_vblank_callback(display_driver_callback_t *event) {
    spi_only_context.callback = event;
}
