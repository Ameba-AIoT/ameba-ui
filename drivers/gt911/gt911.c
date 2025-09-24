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

#include "i2c_api.h"
#include "i2c_ex_api.h"
#include "gt911.h"
#include "gpio_api.h"
#include "gpio_irq_api.h"

#define RST_PIN             _PA_18
#define INT_PIN             _PA_17
#define SDA_PIN             _PA_27
#define SCL_PIN             _PA_28

#define GT_CTRL_REG         0X8040
#define GT_CFGS_REG         0X8047
#define GT_CHECK_REG        0X80FF
#define GT_PID_REG          0X8140

#define GT_GSTID_REG        0X814E
#define GT_TP1_REG          0X8150
#define GT_TP2_REG          0X8158
#define GT_TP3_REG          0X8160
#define GT_TP4_REG          0X8168
#define GT_TP5_REG          0X8170

#define TPD_MAX_FINGERS     5
#define XSIZE               480
#define YSIZE               480
#define I2C_ADDR            0x14
#define I2C_BUS_CLK         400000

#define GT911_PATCH_FOR_LCD_NOISE 1

const uint8_t GT911_CFG_TBL[] =
{
    0X60, 0XE0, 0X01, 0XE0, 0X01, 0X05, 0X35, 0X00, 0X02, 0X08,
    0X1E, 0X08, 0X05, 0X3C, 0X0F, 0X05, 0X00, 0X00, 0XFF, 0X67,
    0X50, 0X00, 0X00, 0X18, 0X1A, 0X1E, 0X14, 0X89, 0X28, 0X0A,
    0X30, 0X2E, 0XBB, 0X0A, 0X03, 0X00, 0X00, 0X02, 0X33, 0X1D,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X32, 0X00, 0X00,
    0X2A, 0X1C, 0X5A, 0X94, 0XC5, 0X02, 0X07, 0X00, 0X00, 0X00,
    0XB5, 0X1F, 0X00, 0X90, 0X28, 0X00, 0X77, 0X32, 0X00, 0X62,
    0X3F, 0X00, 0X52, 0X50, 0X00, 0X52, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X0F,
    0X0F, 0X03, 0X06, 0X10, 0X42, 0XF8, 0X0F, 0X14, 0X00, 0X00,
    0X00, 0X00, 0X1A, 0X18, 0X16, 0X14, 0X12, 0X10, 0X0E, 0X0C,
    0X0A, 0X08, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X29, 0X28, 0X24, 0X22, 0X20, 0X1F, 0X1E, 0X1D,
    0X0E, 0X0C, 0X0A, 0X08, 0X06, 0X05, 0X04, 0X02, 0X00, 0XFF,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
    0XFF, 0XFF, 0XFF, 0XFF,
};

static i2c_t g_obj;
static gt911_touch_data_t g_gt911_touch_data;
gt911_touch_data_callback g_cb = NULL;

static int GT911_read_reg(u16 reg, u8 *buf, u8 len)
{
    u16 r;
    int ret = 0;

    r = reg & 0xff;

    reg = (reg >> 8) | (r << 8);

    ret = i2c_write(&g_obj, I2C_ADDR, (char *)&reg, 2, 1);

    if (ret != 2) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Slave no ACK before read. \r\n", __func__);
        return -1;
    }
    return i2c_read(&g_obj, I2C_ADDR, (char *)buf, len, 1);
}

static int GT911_write_reg(u16 reg, u8 *buf, u8 len)
{
    u8 *temp;
    int ret = 0;
    temp =  malloc(len + 2);
    temp[1] = reg & 0xff;
    temp[0] = (reg >> 8);

    memcpy(temp + 2, buf, len);
    ret = i2c_write(&g_obj, I2C_ADDR, (char *)temp, len + 2, 2);
    free(temp);

    if (ret != (len + 2)) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Write to slave error. \r\n", __func__);
        return -1;
    }

    return 1;
}

uint32_t GT911_read(uint8_t *buf, int len)
{
    uint8_t *read = buf;

    uint8_t mode = 0;
    uint8_t point_num = 0;
    uint8_t reset = 0;
    int ret = 0;

    ret = GT911_read_reg(GT_GSTID_REG, &mode, 1);
    if (ret < 0) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Read mode fail. \r\n", __func__);
        return 0;
    }

    RTK_LOGS(NOTAG, RTK_LOG_DEBUG, "%s: mode: %x \r\n", __func__, mode);
    read[0] = mode;
    if (mode & 0x80) {
        ret = GT911_write_reg(GT_GSTID_REG, &reset, 1); // clear flags
        if (ret < 0) {
            RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Slave no ACK. \r\n", __func__);
            return 0;
        }
    } else {
        return 0;
    }
    point_num = mode & 0x0F;
    if (point_num == 0)
    {
        return 0;
    }

    len = GT911_read_reg(0x814F, read + 1, 7);
    if (len < 0) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Slave no response. \r\n", __func__);
        return 0;
    }

    ret = GT911_write_reg(GT_GSTID_REG, &reset, 1);
    if (ret < 0) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Write reset fail. \r\n", __func__);
        return 0;
    }

    return len;
}

static int GT911_firmware_info(void)
{
    const char iic_write_buf[2] = {0x81, 0x40};
    char iic_read_buf[5] = {0};

    i2c_write(&g_obj, I2C_ADDR, iic_write_buf, 2, 1);
    i2c_read(&g_obj, I2C_ADDR, iic_read_buf, 4, 1);

    RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s CTP ID:%x %x %x %x \r\n", __func__,
        iic_read_buf[0], iic_read_buf[1], iic_read_buf[2], iic_read_buf[3]);

    return 0;
}

void transform_point(void)
{
    int x = g_gt911_touch_data.x;
    int y = g_gt911_touch_data.y;

    g_gt911_touch_data.x = YSIZE - y;
    g_gt911_touch_data.y = x;
}

static void GT911_touch_report(void)
{
    g_gt911_touch_data.state = TOUCH_RELEASE;
    uint8_t buf[10] = {0};
    g_gt911_touch_data.x = 480;
    g_gt911_touch_data.y = 480;
    static uint16_t x_old = 0;
    static uint16_t y_old = 0;

    if (GT911_read(buf, 0)) {
        g_gt911_touch_data.state = TOUCH_PRESS;
        g_gt911_touch_data.x = (buf[2] | (buf[3] << 8));
        g_gt911_touch_data.y = (buf[4] | (buf[5] << 8));

        transform_point();
        x_old = g_gt911_touch_data.x;
        y_old = g_gt911_touch_data.y;
    } else {
        g_gt911_touch_data.x = x_old;
        g_gt911_touch_data.y = y_old;
    }
    RTK_LOGS(NOTAG, RTK_LOG_DEBUG, "%s: (x, y) = (%d, %d) \r\n", __func__, x_old, y_old);
}

void gpio_touch_irq_handler(uint32_t id, uint32_t event)
{
    UNUSED(id);
    UNUSED(event);

    GT911_touch_report();
    if (g_cb) {
        g_cb(g_gt911_touch_data);
    }
}

uint8_t GT911_Send_Cfg(uint8_t mode)
{
    uint8_t buf[2];
    uint8_t i = 0;
    buf[0] = 0;
    buf[1] = mode;
    for (i = 0; i < sizeof(GT911_CFG_TBL); i++) {
        buf[0] += GT911_CFG_TBL[i];
    }
    buf[0] = (~buf[0]) + 1;
    GT911_write_reg(GT_CFGS_REG, (uint8_t *)GT911_CFG_TBL, sizeof(GT911_CFG_TBL));
    GT911_write_reg(GT_CHECK_REG, buf, 2);
    return 0;
}

static void board_i2c_init(void)
{
    GPIO_WriteBit(INT_PIN, 0);
    GPIO_WriteBit(RST_PIN, 0);
    DelayMs(10);
    GPIO_WriteBit(INT_PIN, 1);
    DelayUs(100);
    GPIO_WriteBit(RST_PIN, 1);
    DelayMs(5);
    GPIO_WriteBit(INT_PIN, 0);
    DelayMs(50);
}

void gt911_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /*init RST pin*/
    GPIO_InitStructure.GPIO_Pin = RST_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStructure);

    /*init INT pin*/
    GPIO_InitStructure.GPIO_Pin = INT_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStructure);

    board_i2c_init();

    /*init INT pin*/
    GPIO_InitStructure.GPIO_Pin = INT_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_Init(&GPIO_InitStructure);

    /*init I2C*/
    g_obj.i2c_idx = 0;
    i2c_init(&g_obj, SDA_PIN, SCL_PIN);
    i2c_frequency(&g_obj, I2C_BUS_CLK);
    i2c_restart_disable(&g_obj);

#if GT911_PATCH_FOR_LCD_NOISE
    /* -- Fix -- */
    /* i2c_api does not apply interface for IC_FILTER parameter. */
    /* Set IC_FILTER manually to ignore LCD noise. */

    extern I2C_InitTypeDef I2CInitDat[2];
    u32 val;

    I2C_Cmd(g_obj.I2Cx, DISABLE);
    I2CInitDat[g_obj.i2c_idx].I2CFilter = 0x108;
    val = HAL_READ32(0x41008000, 0xEC);
    val &= (~0x1FF);
    val |= 0x108;
    HAL_WRITE32(0x41008000, 0xEC, val);
    I2C_Cmd(g_obj.I2Cx, ENABLE);
#endif

    GT911_firmware_info();

    gpio_irq_t gpio_btn;
    gpio_irq_init(&gpio_btn, INT_PIN, gpio_touch_irq_handler, (uint32_t)(&gpio_btn));
    gpio_irq_set(&gpio_btn, IRQ_RISE, 1); // IRQ_FALL
    gpio_irq_enable(&gpio_btn);
}

void gt911_register_touch_data_callback(gt911_touch_data_callback cb)
{
    g_cb = cb;
}
