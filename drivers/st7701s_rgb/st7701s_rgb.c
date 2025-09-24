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

#include "st7701s_rgb.h"
#include "os_wrapper.h"

static ST7701S_RGBVBlankCallback *g_callback = NULL;
static void *g_data = NULL;

#define ST7701S_DRV_PIXEL_BITS      16

#define WIDTH                       480
#define HEIGHT                      480
#define MEM_SIZE                    (WIDTH * HEIGHT * 2) // RGB565
#define LCDC_LINE_NUM_INTR_DEF      (WIDTH / 2)

#define LCDC_RESET                  _PA_4
#define LCD_BLEN                    _PC_0 // Back Light EN

#define SPI1_MOSI                   _PB_29
#define SPI1_SCLK                   _PB_28
#define SPI1_CS                     _PB_27
// #define SPI1_MISO

typedef struct {
    IRQn_Type IrqNum;
    SPI_TypeDef *spi_dev;

    void *RxData;
    void *TxData;
    u32 RxLength;
    u32 TxLength;

    u32 Index;
    u32 Role;
} SPI_OBJ;

static SPI_OBJ spi_master;

#define COLOR_DEBUG 0

static u8 *g_buffer = NULL;
#if COLOR_DEBUG
static u8 *debug_buffer = NULL;
#endif
static int g_image_format = 0;

static struct LCDC_IRQInfoDef {
    u32 IrqNum;
    u32 IrqData;
    u32 IrqPriority;
} gLcdcIrqInfo;

/* config pinmux and control blen pad */
static void lcdc_pinmux_config(void)
{
    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s \r\n", __func__);

    Pinmux_Config(_PA_22, PINMUX_FUNCTION_LCD_D0);          /* D0 - B0 */
    Pinmux_Config(_PA_21, PINMUX_FUNCTION_LCD_D1);          /* D1 - B1 */
    Pinmux_Config(_PA_20, PINMUX_FUNCTION_LCD_D2);          /* D2 - B2 */
    Pinmux_Config(_PA_19, PINMUX_FUNCTION_LCD_D3);          /* D3 - B3 */
    Pinmux_Config(_PA_16, PINMUX_FUNCTION_LCD_D4);          /* D4 - B4 */
    Pinmux_Config(_PA_15, PINMUX_FUNCTION_LCD_D5);          /* D5 - G0 */
    Pinmux_Config(_PA_14, PINMUX_FUNCTION_LCD_D6);          /* D6 - G1 */
    Pinmux_Config(_PA_13, PINMUX_FUNCTION_LCD_D7);          /* D7 - G2 */
    Pinmux_Config(_PA_12, PINMUX_FUNCTION_LCD_D8);          /* D8 - G3 */
    Pinmux_Config(_PA_11, PINMUX_FUNCTION_LCD_D9);          /* D9 - G4 */
    Pinmux_Config(_PA_10, PINMUX_FUNCTION_LCD_D10);         /* D10 - G5 */
    Pinmux_Config(_PA_9, PINMUX_FUNCTION_LCD_D11);          /* D11 - R0 */
    Pinmux_Config(_PA_8, PINMUX_FUNCTION_LCD_D12);          /* D12 - R1 */
    Pinmux_Config(_PA_7, PINMUX_FUNCTION_LCD_D13);          /* D13 - R2 */
    Pinmux_Config(_PA_6, PINMUX_FUNCTION_LCD_D14);          /* D14 - R3 */
    Pinmux_Config(_PA_5, PINMUX_FUNCTION_LCD_D15);          /* D15 - R4 */

    Pinmux_Config(_PA_25, PINMUX_FUNCTION_LCD_RGB_HSYNC);   /* HSYNC */
    Pinmux_Config(_PA_26, PINMUX_FUNCTION_LCD_RGB_VSYNC);   /* VSYNC */
    Pinmux_Config(_PA_24, PINMUX_FUNCTION_LCD_RGB_DCLK);    /* DCLK */

    Pinmux_Config(_PA_23, PINMUX_FUNCTION_LCD_RGB_DE);      /* DE */
}

static void gpio_lcd_reset(void)
{
    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s \r\n", __func__);

    GPIO_InitTypeDef GPIO_InitStruct_LCD_RST;

    GPIO_InitStruct_LCD_RST.GPIO_Pin = LCDC_RESET;
    GPIO_InitStruct_LCD_RST.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStruct_LCD_RST);

    GPIO_WriteBit(LCDC_RESET, 1);
    DelayMs(4);
    GPIO_WriteBit(LCDC_RESET, 0);
    DelayMs(30);
    GPIO_WriteBit(LCDC_RESET, 1);
    DelayMs(120);
}

static void gpio_back_light_enable(void)
{
    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s \r\n", __func__);

    GPIO_InitTypeDef GPIO_InitStruct_BLEN;

    GPIO_InitStruct_BLEN.GPIO_Pin = LCD_BLEN;
    GPIO_InitStruct_BLEN.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStruct_BLEN);

    GPIO_WriteBit(LCD_BLEN, 1);  // LCD_BLEN is back-light, set 1 to enbale.
}

static void lcdc_irq_handler(void)
{
    volatile u32 IntId;

    IntId = LCDC_GetINTStatus(LCDC);
    LCDC_ClearINT(LCDC, IntId);

    RTK_LOGS(NOTAG, RTK_LOG_DEBUG, "irq 0x%x \r\n", IntId);

    if (IntId & LCDC_BIT_LCD_FRD_INTS) {
        RTK_LOGS(NOTAG, RTK_LOG_DEBUG, "intr: frame done \r\n");
    }

    if (IntId & LCDC_BIT_LCD_LIN_INTS) {
        RTK_LOGS(NOTAG, RTK_LOG_DEBUG, "intr: line hit \r\n");
        LCDC_DMAImgCfg(LCDC, (u32)g_buffer);
        LCDC_ShadowReloadConfig(LCDC);
    }

    if (IntId & LCDC_BIT_LCD_LIN_INTEN) {
        if (g_callback) {
            g_callback->VBlank(g_data);
        }
    }

    if (IntId & LCDC_BIT_DMA_UN_INTS) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "intr: dma udf !!! \r\n");
    }
}

static void spi_write_command(uint32_t cmd)
{
    DelayUs(200);

    while (!SSI_Writeable(SPI1_DEV));
    SSI_WriteData(SPI1_DEV, cmd);
}

static void spi_write_data(uint32_t data)
{
    uint32_t send = (data | 0x100);

    while (!SSI_Writeable(SPI1_DEV));
    SSI_WriteData(SPI1_DEV, send);
}

#ifdef BIST_DEBUG
static void spi_debug_bist_mode(void)
{
    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s \r\n", __func__);
    #include "st7701_rgb_spi_bist.inc"
}
#else
static void spi_send_init_sequence(void)
{
    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s \r\n", __func__);
    #include "st7701_rgb_spi_init.inc"
}
#endif

static void spi_driver_init(void)
{
    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s \r\n", __func__);

    u32 SclkPhase = SCPH_TOGGLES_IN_MIDDLE; // SCPH_TOGGLES_IN_MIDDLE or SCPH_TOGGLES_AT_START
    u32 SclkPolarity = SCPOL_INACTIVE_IS_LOW; // SCPOL_INACTIVE_IS_LOW or SCPOL_INACTIVE_IS_HIGH
    u32 ClockDivider = 500; // Fssi_clk is 100MHz

    /* SPI1 as Master */
    spi_master.Index = 0x1;
    spi_master.Role = SSI_MASTER;
    spi_master.spi_dev = SPI_DEV_TABLE[spi_master.Index].SPIx;
    spi_master.IrqNum = SPI_DEV_TABLE[spi_master.Index].IrqNum;

    /* init SPI1 */
    SSI_InitTypeDef SSI_InitStructM;
    SSI_StructInit(&SSI_InitStructM);
    RCC_PeriphClockCmd(APBPeriph_SPI1, APBPeriph_SPI1_CLOCK, ENABLE);
    Pinmux_Config(SPI1_MOSI, PINMUX_FUNCTION_SPI1);
    Pinmux_Config(SPI1_SCLK, PINMUX_FUNCTION_SPI1);
    Pinmux_Config(SPI1_CS, PINMUX_FUNCTION_SPI1);
    // Pinmux_Config(SPI1_MISO, PINMUX_FUNCTION_SPIM);

    Pinmux_Config(_PB_27, PINMUX_FUNCTION_SPI1_CS);          /* SPI_CS */
    Pinmux_Config(_PB_28, PINMUX_FUNCTION_SPI1_CLK);         /* SPI_SCK */
    Pinmux_Config(_PB_29, PINMUX_FUNCTION_SPI1_MOSI);        /* SPI_SDA */

    PAD_PullCtrl((u32)SPI1_CS, GPIO_PuPd_UP);

    SSI_SetRole(spi_master.spi_dev, SSI_MASTER);
    SSI_InitStructM.SPI_Role = SSI_MASTER;
    SSI_Init(spi_master.spi_dev, &SSI_InitStructM);

    /* set format */
    SSI_SetSclkPhase(spi_master.spi_dev, SclkPhase);
    SSI_SetSclkPolarity(spi_master.spi_dev, SclkPolarity);
    SSI_SetDataFrameSize(spi_master.spi_dev, DFS_9_BITS);

    /* set frequency */
    SSI_SetBaudDiv(spi_master.spi_dev, ClockDivider); // Fspi_clk is 100MHz
}

static void lcdc_driver_init(void)
{
    LCDC_RGBInitTypeDef LCDC_RGBInitStruct;

    LCDC_Cmd(LCDC, DISABLE);
    LCDC_RGBStructInit(&LCDC_RGBInitStruct);

    /* set HV para according to lcd spec */
    LCDC_RGBInitStruct.Panel_RgbTiming.RgbVsw = 4; // 10;
    LCDC_RGBInitStruct.Panel_RgbTiming.RgbVbp = 4; // 20;
    LCDC_RGBInitStruct.Panel_RgbTiming.RgbVfp = 8; // 20;

    LCDC_RGBInitStruct.Panel_RgbTiming.RgbHsw = 10;
    LCDC_RGBInitStruct.Panel_RgbTiming.RgbHbp = 10; // 50;
    LCDC_RGBInitStruct.Panel_RgbTiming.RgbHfp = 38; // 50;

    LCDC_RGBInitStruct.Panel_Init.IfWidth = LCDC_RGB_IF_16_BIT;
    LCDC_RGBInitStruct.Panel_Init.ImgWidth = WIDTH;
    LCDC_RGBInitStruct.Panel_Init.ImgHeight = HEIGHT;

    LCDC_RGBInitStruct.Panel_RgbTiming.Flags.RgbEnPolar = LCDC_RGB_EN_PUL_HIGH_LEV_ACTIVE;
    LCDC_RGBInitStruct.Panel_RgbTiming.Flags.RgbDclkActvEdge = LCDC_RGB_DCLK_FALLING_EDGE_FETCH;
    LCDC_RGBInitStruct.Panel_RgbTiming.Flags.RgbHsPolar = LCDC_RGB_HS_PUL_LOW_LEV_SYNC;
    LCDC_RGBInitStruct.Panel_RgbTiming.Flags.RgbVsPolar = LCDC_RGB_VS_PUL_LOW_LEV_SYNC;

    LCDC_RGBInitStruct.Panel_Init.InputFormat = LCDC_INPUT_FORMAT_RGB565;
    LCDC_RGBInitStruct.Panel_Init.OutputFormat = LCDC_OUTPUT_FORMAT_RGB565;
    LCDC_RGBInitStruct.Panel_Init.RGBRefreshFreq = 60;

    LCDC_RGBInit(LCDC, &LCDC_RGBInitStruct);

    /* configure DMA burst size */
    LCDC_DMABurstSizeConfig(LCDC, 2);
}

void st7701s_rgb_init_prepare(void)
{
    gpio_lcd_reset();

    spi_driver_init();

#ifdef BIST_DEBUG
    spi_debug_bist_mode();
    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s Testing Bist Mode. \r\n", __func__);
#else
    spi_send_init_sequence();
#endif
}

void st7701s_rgb_init(int image_format)
{
    g_image_format = image_format;
    if (g_image_format != RGB565) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s not supported. \r\n", __func__);
        return;
    }

#if COLOR_DEBUG
    debug_buffer = (uint8_t *)malloc(MEM_SIZE);
    memset(debug_buffer, 0xFF, 480 * 480 * 2); // white
#endif

    /* init lcdc irq info */
    gLcdcIrqInfo.IrqNum = LCDC_IRQ; //49
    gLcdcIrqInfo.IrqPriority = INT_PRI_MIDDLE;
    gLcdcIrqInfo.IrqData = (u32)LCDC;

    lcdc_pinmux_config();
    gpio_back_light_enable();

    /* Enable function and clock */
    LCDC_RccEnable();

    /* register irq handler */
    InterruptRegister((IRQ_FUN)lcdc_irq_handler, gLcdcIrqInfo.IrqNum, NULL, gLcdcIrqInfo.IrqPriority);
    InterruptEn(gLcdcIrqInfo.IrqNum, gLcdcIrqInfo.IrqPriority);

    /* init lcdc driver */
    lcdc_driver_init();

    /* config irq event */
    LCDC_LineINTPosConfig(LCDC, LCDC_LINE_NUM_INTR_DEF);
    LCDC_INTConfig(LCDC, LCDC_BIT_LCD_FRD_INTEN | LCDC_BIT_DMA_UN_INTEN | LCDC_BIT_LCD_LIN_INTEN, ENABLE);

    st7701s_rgb_init_prepare();

    /* enable lcdc */
    LCDC_Cmd(LCDC, ENABLE);
}

void st7701s_rgb_clean_invalidate_buffer(u8 *buffer)
{
#if !COLOR_DEBUG
    if (g_image_format == RGB565) {
        g_buffer = buffer;
    } else {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s not support. \r\n", __func__);
    }
#else
    /* Debug only. */
    UNUSED(buffer);
    g_buffer = debug_buffer;
#endif

    DCache_Clean((u32)g_buffer, MEM_SIZE);
}

void st7701s_rgb_get_info(int *width, int *height)
{
    *width = WIDTH;
    *height = HEIGHT;
}

void st7701s_rgb_register_callback(ST7701S_RGBVBlankCallback *callback, void *data)
{
    g_callback = callback;
    g_data = data;
}
