/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
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

#include "lcdc.h"

#define LOG_TAG "lcdc"

typedef struct {
    lcdc_event_t callback;
    void *user_data;
    uint8_t *current_buffer;
    lcd_format_t format;
    lcd_timing_t timing;
    bool refresh_pending;
    bool initialized;
    bool lcdc_enabled;
} lcdc_context_t;

static lcdc_context_t lcdc_context = {0};

static struct {
    uint32_t irq_num;
    uint32_t irq_priority;
} lcdc_irq_info = {
    .irq_num = LCDC_IRQ,
    .irq_priority = INT_PRI_MIDDLE
};

static const lcd_timing_t default_timing_800x480 = {
    .width = 800,
    .height = 480,
    .hsync_front_porch = 40,
    .hsync_back_porch = 40,
    .hsync_pulse_width = 4,
    .vsync_front_porch = 6,
    .vsync_back_porch = 4,
    .vsync_pulse_width = 1,
    .clock_frequency = 60
};

static const lcd_timing_t default_timing_480x272 = {
    .width = 480,
    .height = 272,
    .hsync_front_porch = 2,
    .hsync_back_porch = 2,
    .hsync_pulse_width = 41,
    .vsync_front_porch = 2,
    .vsync_back_porch = 2,
    .vsync_pulse_width = 10,
    .clock_frequency = 60
};

static void lcdc_pinmux_config(void) {
    RTK_LOGS(LOG_TAG, RTK_LOG_ALWAYS, "Configuring LCDC pins\n");

    GPIO_InitTypeDef gpio_display;
    gpio_display.GPIO_Pin = _PA_17;
    gpio_display.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&gpio_display);
    GPIO_WriteBit(_PA_17, 1);

    // backlight
    GPIO_InitTypeDef gpio_init;
    gpio_init.GPIO_Pin = _PB_3;
    gpio_init.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&gpio_init);
    GPIO_WriteBit(_PB_3, 1);

    // RGB pins
    Pinmux_Config(_PB_15, PINMUX_FUNCTION_LCD_D0);
    Pinmux_Config(_PB_17, PINMUX_FUNCTION_LCD_D1);
    Pinmux_Config(_PB_21, PINMUX_FUNCTION_LCD_D2);
    Pinmux_Config(_PB_18, PINMUX_FUNCTION_LCD_D3);
    Pinmux_Config(_PA_6, PINMUX_FUNCTION_LCD_D4);
    Pinmux_Config(_PA_8, PINMUX_FUNCTION_LCD_D5);
    Pinmux_Config(_PA_7, PINMUX_FUNCTION_LCD_D6);
    Pinmux_Config(_PA_10, PINMUX_FUNCTION_LCD_D7);

    Pinmux_Config(_PB_9, PINMUX_FUNCTION_LCD_D8);
    Pinmux_Config(_PB_11, PINMUX_FUNCTION_LCD_D9);
    Pinmux_Config(_PB_10, PINMUX_FUNCTION_LCD_D10);
    Pinmux_Config(_PB_16, PINMUX_FUNCTION_LCD_D11);
    Pinmux_Config(_PB_22, PINMUX_FUNCTION_LCD_D12);
    Pinmux_Config(_PB_23, PINMUX_FUNCTION_LCD_D13);
    Pinmux_Config(_PB_14, PINMUX_FUNCTION_LCD_D14);
    Pinmux_Config(_PB_12, PINMUX_FUNCTION_LCD_D15);

    Pinmux_Config(_PA_22, PINMUX_FUNCTION_LCD_D16);
    Pinmux_Config(_PA_25, PINMUX_FUNCTION_LCD_D17);
    Pinmux_Config(_PA_29, PINMUX_FUNCTION_LCD_D18);
    Pinmux_Config(_PB_4, PINMUX_FUNCTION_LCD_D19);
    Pinmux_Config(_PB_5, PINMUX_FUNCTION_LCD_D20);
    Pinmux_Config(_PB_6, PINMUX_FUNCTION_LCD_D21);
    Pinmux_Config(_PB_7, PINMUX_FUNCTION_LCD_D22);
    Pinmux_Config(_PB_8, PINMUX_FUNCTION_LCD_D23);

    // Control pins
    Pinmux_Config(_PA_16, PINMUX_FUNCTION_LCD_RGB_HSYNC);
    Pinmux_Config(_PA_13, PINMUX_FUNCTION_LCD_RGB_VSYNC);
    Pinmux_Config(_PA_9, PINMUX_FUNCTION_LCD_RGB_DCLK);
    Pinmux_Config(_PA_14, PINMUX_FUNCTION_LCD_RGB_DE);
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

        if (lcdc_context.callback.vblank_handler) {
            lcdc_context.callback.vblank_handler(lcdc_context.user_data);
        }
    }

    if (int_status & LCDC_BIT_DMA_UN_INTS) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "irq: dma underflow\n");
    }
}

static bool lcdc_controller_init(const lcd_timing_t *timing) {
    LCDC_RGBInitTypeDef rgb_init;

    LCDC_Cmd(LCDC, DISABLE);
    LCDC_RGBStructInit(&rgb_init);

    // panel timing
    rgb_init.Panel_RgbTiming.RgbVsw = timing->vsync_pulse_width;
    rgb_init.Panel_RgbTiming.RgbVbp = timing->vsync_back_porch;
    rgb_init.Panel_RgbTiming.RgbVfp = timing->vsync_front_porch;
    rgb_init.Panel_RgbTiming.RgbHsw = timing->hsync_pulse_width;
    rgb_init.Panel_RgbTiming.RgbHbp = timing->hsync_back_porch;
    rgb_init.Panel_RgbTiming.RgbHfp = timing->hsync_front_porch;

    // display param
    rgb_init.Panel_Init.IfWidth = LCDC_RGB_IF_24_BIT;
    rgb_init.Panel_Init.ImgWidth = timing->width;
    rgb_init.Panel_Init.ImgHeight = timing->height;

    // polar settings
    rgb_init.Panel_RgbTiming.Flags.RgbEnPolar = LCDC_RGB_EN_PUL_HIGH_LEV_ACTIVE;
    rgb_init.Panel_RgbTiming.Flags.RgbDclkActvEdge = LCDC_RGB_DCLK_FALLING_EDGE_FETCH;
    rgb_init.Panel_RgbTiming.Flags.RgbHsPolar = LCDC_RGB_HS_PUL_LOW_LEV_SYNC;
    rgb_init.Panel_RgbTiming.Flags.RgbVsPolar = LCDC_RGB_VS_PUL_LOW_LEV_SYNC;

    // color formats
    switch (lcdc_context.format) {
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

    // output settings
    rgb_init.Panel_Init.OutputFormat = LCDC_OUTPUT_FORMAT_RGB888;
    rgb_init.Panel_Init.RGBRefreshFreq = timing->clock_frequency;

    LCDC_RGBInit(LCDC, &rgb_init);
    LCDC_DMABurstSizeConfig(LCDC, 2);

    return true;
}

bool lcdc_init(lcd_format_t format, const lcd_timing_t *timing) {
    if (lcdc_context.initialized) {
        RTK_LOGS(LOG_TAG, RTK_LOG_WARN, "LCDC already initialized\n");
        return true;
    }

    if (!timing) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Timing configuration is required\n");
        return false;
    }

    // save info
    lcdc_context.format = format;
    lcdc_context.timing = *timing;
    lcdc_context.initialized = false;

    // pinmux config
    lcdc_pinmux_config();

    // enable clock
    LCDC_RccEnable();

    // init lcdc
    if (!lcdc_controller_init(timing)) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Failed to initialize LCD controller\n");
        return false;
    }

    // register interrupt
    InterruptRegister((IRQ_FUN)lcdc_irq_handler, lcdc_irq_info.irq_num, 
                      (uint32_t)LCDC, lcdc_irq_info.irq_priority);
    InterruptEn(lcdc_irq_info.irq_num, lcdc_irq_info.irq_priority);

    // enable interrupt
    int line_intr_position = timing->height * 5 / 6;  // line interrupt position
    LCDC_LineINTPosConfig(LCDC, line_intr_position);
    LCDC_INTConfig(LCDC, LCDC_BIT_LCD_FRD_INTEN | LCDC_BIT_DMA_UN_INTEN | 
                   LCDC_BIT_LCD_LIN_INTEN, ENABLE);

    // enable lcdc
    //LCDC_Cmd(LCDC, ENABLE);

    lcdc_context.lcdc_enabled = false;
    lcdc_context.initialized = true;

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "LCDC initialized: %dx%d, format: %d\n", 
             timing->width, timing->height, format);

    return true;
}

bool lcdc_init_default(lcd_format_t format, uint32_t width, uint32_t height) {
    const lcd_timing_t *default_timing = NULL;
    
    if (width == 800 && height == 480) {
        default_timing = &default_timing_800x480;
    } else if (width == 480 && height == 272) {
        default_timing = &default_timing_480x272;
    } else {
        lcd_timing_t timing = {
            .width = width,
            .height = height,
            .hsync_front_porch = 40,
            .hsync_back_porch = 40,
            .hsync_pulse_width = 4,
            .vsync_front_porch = 6,
            .vsync_back_porch = 4,
            .vsync_pulse_width = 1,
            .clock_frequency = 60
        };
        return lcdc_init(format, &timing);
    }
    
    return lcdc_init(format, default_timing);
}

void lcdc_deinit(void) {
    if (!lcdc_context.initialized) {
        return;
    }

    LCDC_Cmd(LCDC, DISABLE);
    InterruptDis(lcdc_irq_info.irq_num);
    InterruptUnRegister(lcdc_irq_info.irq_num);
    
    memset(&lcdc_context, 0, sizeof(lcdc_context));

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "LCDC deinitialized\n");
}

void lcdc_get_resolution(uint32_t *width, uint32_t *height) {
    if (width) *width = lcdc_context.timing.width;
    if (height) *height = lcdc_context.timing.height;
}

bool lcdc_set_timing(const lcd_timing_t *timing) {
    if (!lcdc_context.initialized || !timing) {
        return false;
    }

    // save new timing
    lcdc_context.timing = *timing;
    
    // re-init lcdc
    LCDC_Cmd(LCDC, DISABLE);
    bool success = lcdc_controller_init(timing);
    
    // reset line interrupt position
    int line_intr_position = timing->height * 5 / 6;
    LCDC_LineINTPosConfig(LCDC, line_intr_position);
    
    LCDC_Cmd(LCDC, ENABLE);
    
    if (success) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "LCDC timing updated: %dx%d\n", 
                 timing->width, timing->height);
    }

    return success;
}

void lcdc_page_flip(uint8_t *buffer) {
    if (!lcdc_context.initialized) {
        return;
    }

    DCache_CleanInvalidate(0xFFFFFFFF, 0xFFFFFFFF);
    LCDC_DMAImgCfg(LCDC, (uint32_t)buffer);
    LCDC_ShadowReloadConfig(LCDC);

    if (!lcdc_context.lcdc_enabled) {
        LCDC_Cmd(LCDC, ENABLE);
        lcdc_context.lcdc_enabled = true;
    }
}

void lcdc_register_event_callback(lcdc_event_t *callback, void *user_data) {
    lcdc_context.callback = *callback;
    lcdc_context.user_data = user_data;
}
