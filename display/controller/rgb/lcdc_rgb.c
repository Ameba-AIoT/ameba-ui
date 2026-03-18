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
#include "lcdc_rgb.h"

#define LOG_TAG "LcdcRgb"

typedef struct {
    // panel device
    panel_dev_t *panel;

    // controller state
    lcd_format_t in_format;
    lcd_format_t out_format;
    lcd_timing_t timing;
    bool initialized;
    bool lcdc_enabled;

    display_driver_callback_t *callback;
    void *user_data;

} lcdc_context_t;

static lcdc_context_t lcdc_context = {0};

static struct {
    uint32_t irq_num;
    uint32_t irq_priority;
} lcdc_irq_info = {
    .irq_num = LCDC_IRQ,
    .irq_priority = INT_PRI_MIDDLE
};

static void lcdc_enable_clk(void) {
    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "RCC Enable\n");
    LCDC_RccEnable();
}

static void lcdc_irq_handler(void) {
    volatile uint32_t int_status = LCDC_GetINTStatus(LCDC);
    LCDC_ClearINT(LCDC, int_status);

    RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "irq 0x%x\n", int_status);

    if (int_status & LCDC_BIT_LCD_FRD_INTS) {
        RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "irq: frame done\n");
    }

    if (int_status & LCDC_BIT_LCD_LIN_INTS) {
        RTK_LOGS(LOG_TAG, RTK_LOG_DEBUG, "irq: line hit\n");

        if (lcdc_context.callback) {
            lcdc_context.callback->vblank_handler(lcdc_context.user_data);
        }
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

static bool rgb_controller_init(const panel_timing_t *panel_timing) {
    LCDC_RGBInitTypeDef rgb_init;

    LCDC_Cmd(LCDC, DISABLE);
    LCDC_RGBStructInit(&rgb_init);

    // panel timing
    rgb_init.Panel_RgbTiming.RgbVsw = panel_timing->vsync_pulse_width;
    rgb_init.Panel_RgbTiming.RgbVbp = panel_timing->vsync_back_porch;
    rgb_init.Panel_RgbTiming.RgbVfp = panel_timing->vsync_front_porch;
    rgb_init.Panel_RgbTiming.RgbHsw = panel_timing->hsync_pulse_width;
    rgb_init.Panel_RgbTiming.RgbHbp = panel_timing->hsync_back_porch;
    rgb_init.Panel_RgbTiming.RgbHfp = panel_timing->hsync_front_porch;

    // display param
    rgb_init.Panel_Init.IfWidth = LCDC_RGB_IF_24_BIT;
    rgb_init.Panel_Init.ImgWidth = panel_timing->width;
    rgb_init.Panel_Init.ImgHeight = panel_timing->height;

    // polar settings
    rgb_init.Panel_RgbTiming.Flags.RgbEnPolar = panel_timing->de_active_high ?
                                                LCDC_RGB_EN_PUL_HIGH_LEV_ACTIVE : LCDC_RGB_EN_PUL_LOW_LEV_ACTIVE;
    rgb_init.Panel_RgbTiming.Flags.RgbDclkActvEdge = panel_timing->dclk_falling_edge ?
                                                LCDC_RGB_DCLK_FALLING_EDGE_FETCH : LCDC_RGB_DCLK_RISING_EDGE_FETCH;
    rgb_init.Panel_RgbTiming.Flags.RgbHsPolar = panel_timing->hsync_active_low ?
                                                LCDC_RGB_HS_PUL_LOW_LEV_SYNC : LCDC_RGB_HS_PUL_HIGH_LEV_SYNC;
    rgb_init.Panel_RgbTiming.Flags.RgbVsPolar = panel_timing->vsync_active_low ?
                                                LCDC_RGB_VS_PUL_LOW_LEV_SYNC : LCDC_RGB_VS_PUL_HIGH_LEV_SYNC;

    // color formats
    switch (lcdc_context.in_format) {
        case LCD_FORMAT_RGB565:
            rgb_init.Panel_Init.InputFormat = LCDC_INPUT_FORMAT_RGB565;
            break;
        case LCD_FORMAT_ARGB8888:
            rgb_init.Panel_Init.InputFormat = LCDC_INPUT_FORMAT_ARGB8888;
            break;
        case LCD_FORMAT_RGB888:
        default:
            rgb_init.Panel_Init.InputFormat = LCDC_INPUT_FORMAT_RGB888;
            break;
    }

    switch (lcdc_context.out_format) {
        case LCD_FORMAT_RGB565:
            rgb_init.Panel_Init.OutputFormat = LCDC_OUTPUT_FORMAT_RGB565;
            rgb_init.Panel_Init.IfWidth = LCDC_RGB_IF_16_BIT;
            break;
        case LCD_FORMAT_RGB888:
        default:
            rgb_init.Panel_Init.OutputFormat = LCDC_OUTPUT_FORMAT_RGB888;
            rgb_init.Panel_Init.IfWidth = LCDC_RGB_IF_24_BIT;
            break;
    }

    rgb_init.Panel_Init.RGBRefreshFreq = panel_timing->clock_frequency;

    LCDC_RGBInit(LCDC, &rgb_init);
    LCDC_DMABurstSizeConfig(LCDC, 2);

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "lcdc_rgb_controller_init init done\n");

    return true;
}

bool lcdc_rgb_controller_init(int32_t color_depth, panel_dev_t *panel) {
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

    lcdc_enable_clk();

    if (!rgb_controller_init(panel_timing)) {
        return false;
    }

    lcdc_init_irq(&lcdc_context.timing);
    lcdc_context.initialized = true;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "LCDC initialized with panel: %s\n",
             panel->desc->name);

    return true;
}

void lcdc_rgb_do_page_flip(uint8_t *buffer) {
    if (!lcdc_context.initialized || !lcdc_context.panel) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "lcdc not initialized or no panel\n");
        return;
    }

    DCache_CleanInvalidate(0xFFFFFFFF, 0xFFFFFFFF);
    LCDC_DMAImgCfg(LCDC, (uint32_t)buffer);
    LCDC_ShadowReloadConfig(LCDC);

    if (!lcdc_context.lcdc_enabled) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "lcdc enable\n");
        LCDC_Cmd(LCDC, ENABLE);
        lcdc_context.lcdc_enabled = true;
    }

}

void lcdc_rgb_register_vblank_callback(display_driver_callback_t *event) {
    lcdc_context.callback = event;
}