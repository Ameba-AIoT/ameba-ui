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
#include "lcdc_mipi.h"

#define LOG_TAG "LcdcMipi"

#define Mhz                                         1000000UL
#define T_LPX                                       5
#define T_HS_PREP                                   6
#define T_HS_TRAIL                                  8
#define T_HS_EXIT                                   7
#define T_HS_ZERO                                   10

#define MIPI_DSI_DCS_SHORT_WRITE                    0x05
#define MIPI_DSI_DCS_SHORT_WRITE_PARAM              0x15
#define MIPI_DSI_DCS_LONG_WRITE                     0x39

#define REGFLAG_DELAY                               0xFC
#define REGFLAG_END_OF_TABLE                        0xFD

#define MIPI_DSI_RTNI                               2//4

typedef struct {
    const uint8_t (*table)[32];
    uint32_t table_size;
    uint32_t index;
    volatile uint8_t init_done;
    volatile uint8_t send_busy;
} panel_cmd_ctx_t;

typedef struct {
    // panel device
    panel_dev_t *panel;

    // controller state
    lcd_format_t in_format;
    lcd_format_t out_format;
    lcd_timing_t timing;
    panel_cmd_ctx_t panel_ctx;
    bool initialized;
    bool lcdc_enabled;

    MIPI_InitTypeDef mipi_init_struct;
    LCDC_InitTypeDef lcdc_init_struct;

    display_driver_callback_t *callback;
    void *user_data;

    uint32_t buffer_size;
} lcdc_context_t;

static lcdc_context_t lcdc_context = {0};

static struct {
    uint32_t irq_num;
    uint32_t irq_priority;
} lcdc_irq_info = {
    .irq_num = LCDC_IRQ,
    .irq_priority = INT_PRI_MIDDLE
};

static void lcdc_mipi_enable_clk(void) {
    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "RCC Enable\n");
    RCC_PeriphClockCmd(APBPeriph_NULL, APBPeriph_HPERI_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_LCDC, APBPeriph_LCDCMIPI_CLOCK, ENABLE);
}

static void mipi_dsi_isr(void)
{
    uint32_t ints = MIPI_DSI_INTS_Get(MIPI);
    MIPI_DSI_INTS_Clr(MIPI, ints);

    uint32_t ints_acpu = MIPI_DSI_INTS_ACPU_Get(MIPI);
    MIPI_DSI_INTS_ACPU_Clr(MIPI, ints_acpu);

    if (ints & MIPI_BIT_CMD_TXDONE) {
        lcdc_context.panel_ctx.send_busy = 0;
        ints &= ~MIPI_BIT_CMD_TXDONE;
    }

    if (ints_acpu & MIPI_BIT_VID_DONE) {
        ints_acpu &= ~MIPI_BIT_VID_DONE;
    }

    if (ints & MIPI_BIT_ERROR) {
        uint32_t dphy_err = MIPI->MIPI_DPHY_ERR;
        MIPI->MIPI_DPHY_ERR = dphy_err;

        DiagPrintf("DSI: LPTX/DPHY error ints=0x%08lx, dphy=0x%08lx\n",
                (unsigned long)ints, (unsigned long)dphy_err);

        if (MIPI->MIPI_CONTENTION_DETECTOR_AND_STOPSTATE_DT & MIPI_MASK_DETECT_ENABLE) {
            MIPI->MIPI_CONTENTION_DETECTOR_AND_STOPSTATE_DT &= ~MIPI_MASK_DETECT_ENABLE;

            MIPI->MIPI_DPHY_ERR = dphy_err;
            MIPI_DSI_INTS_Clr(MIPI, MIPI_BIT_ERROR);

            DiagPrintf("DSI: contention cleared, ints=0x%08lx, dphy=0x%08lx\n",
                    (unsigned long)MIPI->MIPI_INTS,
                    (unsigned long)MIPI->MIPI_DPHY_ERR);
        }

        if (MIPI->MIPI_DPHY_ERR == dphy_err) {
            DiagPrintf("DSI: error still present, disable DSI INT\n");
            MIPI_DSI_INT_Config(MIPI, ENABLE, DISABLE, FALSE);
        }

        ints &= ~MIPI_BIT_ERROR;
    }

    if (ints) {
        DiagPrintf("DSI: other INTS=0x%08lx\n", (unsigned long)ints);
    }
    if (ints_acpu) {
        DiagPrintf("DSI: other ACPU_INTS=0x%08lx\n", (unsigned long)ints_acpu);
    }
}

static void dsi_send_dcs(uint8_t cmd,
                        uint8_t payload_len,
                        const uint8_t *para_list)
{
    uint32_t word0, word1, addr, idx;
    uint8_t  buf[128];

    if (payload_len == 0) {
        MIPI_DSI_CMD_Send(MIPI, MIPI_DSI_DCS_SHORT_WRITE, cmd, 0);
        return;
    } else if (payload_len == 1) {
        MIPI_DSI_CMD_Send(MIPI, MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd, para_list[0]);
        return;
    }

    size_t max_len = sizeof(buf) - 1;

    if ((size_t)payload_len > max_len) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "DSI: payload too long (%u), truncated to %zu\n",
                payload_len, max_len);
        payload_len = (uint8_t)max_len;
    }

    buf[0] = cmd;
    for (idx = 0; idx < payload_len; idx++) {
        buf[idx + 1] = para_list[idx];
    }

    payload_len = payload_len + 1;

    uint32_t group = (payload_len + 7U) / 8U;
    for (addr = 0; addr < group; addr++) {
        idx = addr * 8U;

        word0 =  (uint32_t)buf[idx + 0]
                | ((uint32_t)buf[idx + 1] << 8)
                | ((uint32_t)buf[idx + 2] << 16)
                | ((uint32_t)buf[idx + 3] << 24);

        word1 =  (uint32_t)buf[idx + 4]
                | ((uint32_t)buf[idx + 5] << 8)
                | ((uint32_t)buf[idx + 6] << 16)
                | ((uint32_t)buf[idx + 7] << 24);

        MIPI_DSI_CMD_LongPkt_MemQWordRW(MIPI, addr, &word0, &word1, FALSE);
    }

    MIPI_DSI_CMD_Send(MIPI, MIPI_DSI_DCS_LONG_WRITE, payload_len, 0);
}

static uint8_t panel_send_next_cmd(void)
{
    panel_cmd_ctx_t *ctx = &lcdc_context.panel_ctx;

    while (ctx->index < ctx->table_size) {
        const uint8_t *row  = ctx->table[ctx->index];
        uint8_t cmd         = row[0];
        uint8_t count       = row[1];
        const uint8_t *para = &row[2];

        switch (cmd) {
        case REGFLAG_DELAY:
            DelayMs(count);
            ctx->index++;
            break;

        case REGFLAG_END_OF_TABLE:
            ctx->index     = 0;
            ctx->init_done = 1;
            RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "DSI: command table finished\n");
            return 1;

        default:
            if (ctx->send_busy) {
                return 0;
            }

            ctx->send_busy = 1;
            dsi_send_dcs(cmd, count, para);
            ctx->index++;

            return 0;
        }
    }

    ctx->init_done = 1;
    return 1;
}




static void dsi_panel_push_table(uint32_t table_size, const uint8_t table[][32])
{
    MIPI_DSI_TO1_Set(MIPI, DISABLE, 0);
    MIPI_DSI_TO2_Set(MIPI, ENABLE, 0x7FFFFFFF);
    MIPI_DSI_TO3_Set(MIPI, DISABLE, 0);

    InterruptDis(MIPI_DSI_IRQ);
    InterruptUnRegister(MIPI_DSI_IRQ);
    InterruptRegister((IRQ_FUN)mipi_dsi_isr, MIPI_DSI_IRQ, (uint32_t)MIPI, 10);
    InterruptEn(MIPI_DSI_IRQ, 10);

    MIPI_DSI_INT_Config(MIPI, DISABLE, ENABLE, FALSE);

    MIPI_DSI_init(MIPI, &lcdc_context.mipi_init_struct);

    panel_cmd_ctx_t *ctx = &lcdc_context.panel_ctx;
    ctx->table      = table;
    ctx->table_size = table_size;
    ctx->index      = 0;
    ctx->init_done  = 0;
    ctx->send_busy  = 0;

    while (!ctx->init_done) {
        if (!ctx->send_busy) {
            uint8_t done = panel_send_next_cmd();
            if (done) {
                break;
            }
        }

        rtos_time_delay_ms(1);
    }
}

static void lcdc_display_init(const panel_timing_t *panel_timing)
{
    LCDC_StructInit(&lcdc_context.lcdc_init_struct);
    lcdc_context.lcdc_init_struct.LCDC_ImageWidth = panel_timing->width;
    lcdc_context.lcdc_init_struct.LCDC_ImageHeight = panel_timing->height;

    lcdc_context.lcdc_init_struct.LCDC_BgColorRed = 0;
    lcdc_context.lcdc_init_struct.LCDC_BgColorGreen = 0;
    lcdc_context.lcdc_init_struct.LCDC_BgColorBlue = 0;

    lcdc_context.lcdc_init_struct.layerx[0].LCDC_LayerEn = ENABLE;

    int image_format = LCDC_LAYER_IMG_FORMAT_ARGB8888;
    switch (lcdc_context.in_format)
    {
    case LCD_FORMAT_RGB565:
        image_format = LCDC_LAYER_IMG_FORMAT_RGB565;
        lcdc_context.buffer_size = panel_timing->width * panel_timing->height * 2;
        break;
    case LCD_FORMAT_RGB888:
        image_format = LCDC_LAYER_IMG_FORMAT_RGB888;
        lcdc_context.buffer_size = panel_timing->width * panel_timing->height * 3;
        break;
    case LCD_FORMAT_ARGB8888:
        image_format = LCDC_LAYER_IMG_FORMAT_ARGB8888;
        lcdc_context.buffer_size = panel_timing->width * panel_timing->height * 4;
        break;
    default:
        break;
    }

    lcdc_context.lcdc_init_struct.layerx[0].LCDC_LayerImgFormat = image_format;
    lcdc_context.lcdc_init_struct.layerx[0].LCDC_LayerHorizontalStart = 1;/*1-based*/
    lcdc_context.lcdc_init_struct.layerx[0].LCDC_LayerHorizontalStop = panel_timing->width;
    lcdc_context.lcdc_init_struct.layerx[0].LCDC_LayerVerticalStart = 1;/*1-based*/
    lcdc_context.lcdc_init_struct.layerx[0].LCDC_LayerVerticalStop = panel_timing->height;

    LCDC_Init(LCDC, &lcdc_context.lcdc_init_struct);

    LCDC_DMAModeConfig(LCDC, LCDC_LAYER_BURSTSIZE_4X64BYTES);
    LCDC_DMADebugConfig(LCDC, LCDC_DMA_OUT_DISABLE, NULL);

    LCDC_Cmd(LCDC, ENABLE);
    while (!LCDC_CheckLCDCReady(LCDC));

    MIPI_DSI_Mode_Switch(MIPI, ENABLE);
}

static bool mipi_init(const panel_timing_t *panel_timing) {
    MIPI_StructInit(&lcdc_context.mipi_init_struct);

    u32 bit_per_pixel;
    switch (lcdc_context.mipi_init_struct.MIPI_VideoDataFormat)
    {
    case MIPI_VIDEO_DATA_FORMAT_RGB565:
        bit_per_pixel = 16;
        break;
    case MIPI_VIDEO_DATA_FORMAT_RGB666_PACKED:
        bit_per_pixel = 18;
        break;
    case MIPI_VIDEO_DATA_FORMAT_RGB666_LOOSELY:
    case MIPI_VIDEO_DATA_FORMAT_RGB888:
    default:
        bit_per_pixel = 24;
        break;
    }

    lcdc_context.mipi_init_struct.MIPI_LaneNum = 2;
    lcdc_context.mipi_init_struct.MIPI_FrameRate = panel_timing->clock_frequency;

    lcdc_context.mipi_init_struct.MIPI_HSA = panel_timing->hsync_pulse_width * bit_per_pixel / 8 ;//- 10; /* here the unit is pixel but not us */
    if (lcdc_context.mipi_init_struct.MIPI_VideoModeInterface == MIPI_VIDEO_NON_BURST_MODE_WITH_SYNC_PULSES) {
        lcdc_context.mipi_init_struct.MIPI_HBP = panel_timing->hsync_back_porch * bit_per_pixel / 8 ;//- 10;
    } else {
        lcdc_context.mipi_init_struct.MIPI_HBP = (panel_timing->hsync_pulse_width + panel_timing->hsync_back_porch) * bit_per_pixel / 8 ;//-10 ;
    }

    lcdc_context.mipi_init_struct.MIPI_HACT = panel_timing->width;
    lcdc_context.mipi_init_struct.MIPI_HFP = panel_timing->hsync_front_porch * bit_per_pixel / 8 ;//-12;

    lcdc_context.mipi_init_struct.MIPI_VSA = panel_timing->vsync_pulse_width;
    lcdc_context.mipi_init_struct.MIPI_VBP = panel_timing->vsync_back_porch;
    lcdc_context.mipi_init_struct.MIPI_VACT = panel_timing->height;
    lcdc_context.mipi_init_struct.MIPI_VFP = panel_timing->vsync_front_porch;

    /*DataLaneFreq * LaneNum = FrameRate * (VSA+VBP+VACT+VFP) * (HSA+HBP+HACT+HFP) * PixelFromat*/
    u32 vtotal = lcdc_context.mipi_init_struct.MIPI_VSA + lcdc_context.mipi_init_struct.MIPI_VBP + lcdc_context.mipi_init_struct.MIPI_VACT + lcdc_context.mipi_init_struct.MIPI_VFP;
    u32 htotal_bits = (panel_timing->hsync_pulse_width + panel_timing->hsync_back_porch + lcdc_context.mipi_init_struct.MIPI_HACT + panel_timing->hsync_front_porch) * bit_per_pixel;
    u32 overhead_cycles = T_LPX + T_HS_PREP + T_HS_ZERO + T_HS_TRAIL + T_HS_EXIT;
    u32 overhead_bits = overhead_cycles * lcdc_context.mipi_init_struct.MIPI_LaneNum * 8;
    u32 total_bits = htotal_bits + overhead_bits;

    lcdc_context.mipi_init_struct.MIPI_VideDataLaneFreq = lcdc_context.mipi_init_struct.MIPI_FrameRate * total_bits * vtotal / lcdc_context.mipi_init_struct.MIPI_LaneNum / Mhz + 20;

    lcdc_context.mipi_init_struct.MIPI_LineTime = (lcdc_context.mipi_init_struct.MIPI_VideDataLaneFreq * Mhz) / 8 / lcdc_context.mipi_init_struct.MIPI_FrameRate / vtotal;
    lcdc_context.mipi_init_struct.MIPI_BllpLen = lcdc_context.mipi_init_struct.MIPI_LineTime / 2;

    if (panel_timing->hsync_pulse_width + panel_timing->hsync_back_porch + panel_timing->width + panel_timing->hsync_front_porch < (512 + MIPI_DSI_RTNI * 16)) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "!!ERROR!!, LCM NOT SUPPORT\n");
    }

    if (lcdc_context.mipi_init_struct.MIPI_LineTime * lcdc_context.mipi_init_struct.MIPI_LaneNum < total_bits / 8) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR,"!!ERROR!!, LINE TIME TOO SHORT!\n");
    }

    MIPI_Init(MIPI, &lcdc_context.mipi_init_struct);

    return true;
}

static void lcdc_irq_handler(void) {
    volatile uint32_t int_status = LCDC_GetINTStatus(LCDC);
    LCDC_ClearINT(LCDC, int_status);

    RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "irq 0x%x\n", int_status);

    if (int_status & LCDC_BIT_LCD_FRD_INTS) {
        RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "irq: frame done\n");
        if (lcdc_context.callback) {
            lcdc_context.callback->vblank_handler(lcdc_context.user_data);
        }
    }

    if (int_status & LCDC_BIT_LCD_LIN_INTS) {
        RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "irq: line hit\n");
    }

    if (int_status & LCDC_BIT_DMA_UN_INTS) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "irq: dma underflow\n");
    }
}

static void lcdc_init_irq(lcd_timing_t *timing) {
    // register interrupt
    InterruptRegister((IRQ_FUN)lcdc_irq_handler, lcdc_irq_info.irq_num,
                      (uint32_t)LCDC, lcdc_irq_info.irq_priority);
    InterruptEn(lcdc_irq_info.irq_num, lcdc_irq_info.irq_priority);

    // enable interrupt
    int line_intr_position = timing->height * 5 / 6;  // line interrupt position
    LCDC_LineINTPosConfig(LCDC, line_intr_position);
    LCDC_INTConfig(LCDC, LCDC_BIT_LCD_FRD_INTEN | LCDC_BIT_DMA_UN_INTEN |
                   LCDC_BIT_LCD_LIN_INTEN, ENABLE);
}

static bool mipi_controller_init(const panel_timing_t *panel_timing,
                uint32_t table_size, const uint8_t table[][32]) {
    mipi_init(panel_timing);

    dsi_panel_push_table(table_size, table);

    lcdc_display_init(panel_timing);

    return true;
}



bool lcdc_mipi_controller_init(int32_t color_depth, panel_dev_t *panel) {

    if (!panel || !panel->desc) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Invalid panel device\n");
        return false;
    }

    if (lcdc_context.initialized) {
        RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "LCDC already initialized\n");
        return true;
    }


    lcdc_context.panel = panel;

    switch (color_depth)
    {
    case 16:
        lcdc_context.in_format = LCD_FORMAT_RGB565;
        break;
    case 24:
        lcdc_context.in_format = LCD_FORMAT_RGB888;
        break;
    case 32:
        lcdc_context.in_format = LCD_FORMAT_ARGB8888;
        break;
    default:
        break;
    }

    switch (panel->desc->rgb_format) {
        case PANEL_RGB_FORMAT_RGB565:
            lcdc_context.out_format = LCD_FORMAT_RGB565;
            break;
        case PANEL_RGB_FORMAT_RGB888:
        default:
            lcdc_context.out_format = LCD_FORMAT_RGB888;
            break;
    }

    panel_timing_t *panel_timing = &panel->desc->timing;
    lcdc_context.timing.width = panel_timing->width;
    lcdc_context.timing.height = panel_timing->height;
    lcdc_context.timing.hsync_front_porch = panel_timing->hsync_front_porch;
    lcdc_context.timing.hsync_back_porch = panel_timing->hsync_back_porch;
    lcdc_context.timing.hsync_pulse_width = panel_timing->hsync_pulse_width;
    lcdc_context.timing.vsync_front_porch = panel_timing->vsync_front_porch;
    lcdc_context.timing.vsync_back_porch = panel_timing->vsync_back_porch;
    lcdc_context.timing.vsync_pulse_width = panel_timing->vsync_pulse_width;
    lcdc_context.timing.clock_frequency = panel_timing->clock_frequency;

    lcdc_mipi_enable_clk();

    uint32_t table_size = panel->desc->init_cmd_count;
    const uint8_t (*table)[32] = panel->desc->init_cmds;

    if (!mipi_controller_init(panel_timing, table_size, table)) {
        return false;
    }

    lcdc_init_irq(&lcdc_context.timing);
    lcdc_context.initialized = true;
    lcdc_context.lcdc_enabled = false;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "LCDC initialized with panel: %s\n",
             panel->desc->name);

    return true;
}

void lcdc_mipi_do_page_flip(uint8_t *buffer) {
    if (!lcdc_context.initialized || !lcdc_context.panel) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "lcdc not initialized or no panel\n");
        return;
    }

    DCache_CleanInvalidate(0xFFFFFFFF, 0xFFFFFFFF);
    lcdc_context.lcdc_init_struct.layerx[0].LCDC_LayerImgBaseAddr = (u32)buffer;
    LCDC_LayerConfig(LCDC, LCDC_LAYER_LAYER1, &lcdc_context.lcdc_init_struct.layerx[LCDC_LAYER_LAYER1]);
    LCDC_TrigerSHWReload(LCDC);

    if (!lcdc_context.lcdc_enabled) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "lcdc enable\n");
        LCDC_Cmd(LCDC, ENABLE);
        lcdc_context.lcdc_enabled = true;
    }
}

void lcdc_mipi_register_vblank_callback(display_driver_callback_t *event) {
    lcdc_context.callback = event;
}