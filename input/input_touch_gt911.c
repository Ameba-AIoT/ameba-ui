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

#include <stdio.h>

#include "ameba_soc.h"
#include "os_wrapper.h"

#include "i2c_api.h"
#include "i2c_ex_api.h"
#include "gpio_api.h"
#include "gpio_irq_api.h"
#include "input.h"

#define LOG_TAG "GT911"

#define RST_PIN                 _PB_0
#define INT_PIN                 _PA_31
#define SDA_PIN                 _PA_29
#define SCL_PIN                 _PA_30
#define XSIZE                   480
#define YSIZE                   480
#define TRANSFORM_EXCHANGE_X_Y  0
#define TRANSFORM_INVERSE_X     1
#define TRANSFORM_INVERSE_Y     1

#define GT_CTRL_REG             0X8040
#define GT_CFGS_REG             0X8047
#define GT_CHECK_REG            0X80FF
#define GT_PID_REG              0X8140
#define GT_GSTID_REG            0X814E
#define GT_TP1_REG              0X8150
#define GT_TP2_REG              0X8158
#define GT_TP3_REG              0X8160
#define GT_TP4_REG              0X8168
#define GT_TP5_REG              0X8170
#define GT_POINT_REG            0x814F

#define TPD_MAX_FINGERS         5
#define I2C_ADDR                0x14
#define I2C_BUS_CLK             400000

#define PATCH_FOR_LCD_NOISE     1

#define CHECK_AND_RETURN(p) \
    do { \
        if (!(p)) { \
            return; \
        } \
    } while(0)

static const u8 GT911_CFG_TBL[] =
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

#define MSG_Q_SIZE 20

/* Device structure */
struct gt911_data {
    u16 x;
    u16 y;

    bool initialized;
    bool enabled;
    gpio_irq_t gpio_irq;
    i2c_t client;
    rtos_queue_t work_queue;
    rtos_mutex_t lock;
};

static input_device_t gt911_device;
static input_event_callback_t s_user_cb = NULL;

/* I2C read/write helper functions */
static int gt911_i2c_read(i2c_t *client, u16 reg, u8 *buf, int len)
{
    u16 r;
    int ret = 0;

    r = reg & 0xff;
    reg = (reg >> 8) | (r << 8);
    ret = i2c_write(client, I2C_ADDR, (char *)&reg, 2, 1);

    if (ret != 2) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Slave no ACK before read. \r\n", __func__);
        return -1;
    }

    return i2c_read(client, I2C_ADDR, (char *)buf, len, 1);
}

static int gt911_i2c_write(i2c_t *client, u16 reg, u8 *buf, int len)
{
    u8 *temp;
    int ret = 0;
    temp =  rtos_mem_zmalloc(len + 2);
    temp[1] = reg & 0xff;
    temp[0] = (reg >> 8);
    memcpy(temp + 2, buf, len);
    ret = i2c_write(client, I2C_ADDR, (char *)temp, len + 2, 2);
    rtos_mem_free(temp);

    if (ret != (len + 2)) {
        RTK_LOGS(NOTAG, RTK_LOG_ALWAYS, "%s: Write to slave error. \r\n", __func__);
        return -1;
    }

    return 1;
}

/* Read single register */
static int gt911_read_reg(i2c_t *client, u16 reg, u8 *value)
{
    return gt911_i2c_read(client, reg, value, 1);
}

/* Write single register */
static int gt911_write_reg(i2c_t *client, u16 reg, u8 value)
{
    return gt911_i2c_write(client, reg, &value, 1);
}

/* Send command */
static int gt911_send_command(i2c_t *client, u8 cmd)
{
    return gt911_write_reg(client, GT_CTRL_REG, cmd);
}

static void gt911_raw_callback(u16 x, u16 y, u8 state) {
    if (s_user_cb) {
        input_event_t event;
        event.type = INPUT_EVENT_TOUCH;
        event.timestamp = 0;
        event.data.touch.x = x;
        event.data.touch.y = y;
        event.data.touch.pressed = state;
        event.data.touch.touch_id = 0;
        s_user_cb(&event);
    }
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

/* Reset chip */
static void gt911_reset(struct gt911_data *ts)
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
    ts->client.i2c_idx = 0;
    i2c_init(&ts->client, SDA_PIN, SCL_PIN);
    i2c_frequency(&ts->client, I2C_BUS_CLK);
    i2c_restart_disable(&ts->client);

#if PATCH_FOR_LCD_NOISE
    /* -- Fix -- */
    /* i2c_api does not apply interface for IC_FILTER parameter. */
    /* Set IC_FILTER manually to ignore LCD noise. */

    extern I2C_InitTypeDef I2CInitDat[2];
    u32 val;

    I2C_Cmd(ts->client.I2Cx, DISABLE);
    I2CInitDat[ts->client.i2c_idx].I2CFilter = 0x108;
    val = HAL_READ32(0x41008000, 0xEC);
    val &= (~0x1FF);
    val |= 0x108;
    HAL_WRITE32(0x41008000, 0xEC, val);
    I2C_Cmd(ts->client.I2Cx, ENABLE);
#endif
}

u8 gt911_set_config(struct gt911_data *ts, u8 mode)
{
    u8 buf[2];
    u8 i = 0;
    buf[0] = 0;
    buf[1] = mode;

    for (i = 0; i < sizeof(GT911_CFG_TBL); i++) {
        buf[0] += GT911_CFG_TBL[i];
    }

    buf[0] = (~buf[0]) + 1;
    gt911_i2c_write(&ts->client, GT_CFGS_REG, (u8 *)GT911_CFG_TBL, sizeof(GT911_CFG_TBL));
    gt911_i2c_write(&ts->client, GT_CHECK_REG, buf, 2);
    return 0;
}

/* Read product information */
static int gt911_read_product_info(struct gt911_data *ts)
{
    const char iic_write_buf[2] = {0x81, 0x40};
    char iic_read_buf[5] = {0};

    i2c_write(&ts->client, I2C_ADDR, iic_write_buf, 2, 1);
    i2c_read(&ts->client, I2C_ADDR, iic_read_buf, 4, 1);
    RTK_LOGI(LOG_TAG, "%s CTP ID:%x %x %x %x \r\n", __func__,
        iic_read_buf[0], iic_read_buf[1], iic_read_buf[2], iic_read_buf[3]);

    return 0;
}

/* Process touch data */
static void gt911_process_touch_data(struct gt911_data *ts)
{
    CHECK_AND_RETURN(ts);
    u8 read[10] = { 0 };
    u8 mode = 0;
    u8 point_num = 0;
    u8 reset = 0;
    int ret = 0;
    int len = 0;

    rtos_mutex_take(ts->lock, MUTEX_WAIT_TIMEOUT);
    ret = gt911_read_reg(&ts->client, GT_GSTID_REG, &mode);

    if (ret < 0) {
        RTK_LOGW(LOG_TAG, "%s: Read mode fail. \r\n", __func__);
        goto err_finish;
    }

    RTK_LOGD(LOG_TAG, "%s: mode: %x \r\n", __func__, mode);
    read[0] = mode;

    if (mode & 0x80) {
        ret = gt911_write_reg(&ts->client, GT_GSTID_REG, reset); // clear flags

        if (ret < 0) {
            RTK_LOGW(LOG_TAG, "%s: Slave no ACK. \r\n", __func__);
            goto err_finish;
        }
    } else {
        goto err_finish;
    }

    point_num = mode & 0x0F;

    if (point_num == 0) {
        goto err_finish;
    }

    len = gt911_i2c_read(&ts->client, GT_POINT_REG, read + 1, 7);

    if (len < 0) {
        RTK_LOGW(LOG_TAG, "%s: Slave no response. \r\n", __func__);
        goto err_finish;
    }

    ret = gt911_write_reg(&ts->client, GT_GSTID_REG, reset);
    if (ret < 0) {
        RTK_LOGW(LOG_TAG, "%s: Write reset fail. \r\n", __func__);
        goto err_finish;
    }

err_finish:
    if (read[0] > 0) {
        u8 state = len > 0 ? 1 : 0;
        u16 x = (read[2] | (read[3] << 8));
        u16 y = (read[4] | (read[5] << 8));
        if (state) {
#if TRANSFORM_INVERSE_X
            x = XSIZE - x;
#endif
#if TRANSFORM_INVERSE_Y
            y = YSIZE - y;
#endif
#if TRANSFORM_EXCHANGE_X_Y
            ts->x = x;
            x = y;
            y = ts->x;
#endif
            ts->x = x;
            ts->y = y;
        } else {
            x = ts->x;
            y = ts->y;
        }

        gt911_raw_callback(x, y, state);
        RTK_LOGD(LOG_TAG, "x:%d y:%d pressure:%d\n", x, y, state);
    }

    rtos_mutex_give(ts->lock);
}

/* Work queue handler function */
static int gt911_work_handler(struct gt911_data *ts)
{
    void *p_msg = NULL;

    if (ts->enabled) {
        if (RTK_SUCCESS == rtos_queue_receive(ts->work_queue, &p_msg, 20)) {
            gt911_process_touch_data(ts);
            return 0;
        }
    }

    return -1;
}

/* Interrupt handler function */
static void gt911_irq_handler(u32 dev_id, u32 event)
{
    (void) event;
    struct gt911_data *ts = (struct gt911_data *)dev_id;
    CHECK_AND_RETURN(ts);

    /* Add work to work queue */
    if (RTK_SUCCESS != rtos_queue_send(ts->work_queue, ts, 0)) {
        RTK_LOGW(LOG_TAG, "%s, send queue failed\n", __func__);
    }
}

/* Initialize chip */
static void gt911_init_chip(struct gt911_data *ts)
{
    /* Reset chip */
    gt911_reset(ts);
    /* Wait for chip initialization */
    rtos_time_delay_ms(100);
    /* Read product information */
    gt911_read_product_info(ts);
    gpio_irq_init(&ts->gpio_irq, INT_PIN, gt911_irq_handler, (u32)ts);
    gpio_irq_set(&ts->gpio_irq, IRQ_RISE, 1); // IRQ_FALL
}

static void gt911_register_callback(void (*cb)(input_event_t *)) {
    s_user_cb = cb;
}

static void gt911_work(void *param) {
    struct gt911_data *ts = (struct gt911_data *) param;
    CHECK_AND_RETURN(ts);

    while (ts->initialized) {
        if (ts->enabled) {
            if (gt911_work_handler(ts)) {
                rtos_time_delay_ms(20);
            }
        } else {
            rtos_time_delay_ms(20);
        }
    }

    rtos_task_delete(NULL);
}

static void gt911_init_ops(void) {
    RTK_LOGI(LOG_TAG, "Touch device initialized\n");
    CHECK_AND_RETURN(!gt911_device.priv);
    struct gt911_data *gt911 = (struct gt911_data *) rtos_mem_zmalloc(sizeof(struct gt911_data));
    rtos_mutex_create_static(&gt911->lock);
    rtos_mutex_give(gt911->lock);
    rtos_queue_create(&gt911->work_queue, MSG_Q_SIZE, sizeof(uint32_t));
    gt911_init_chip(gt911);
    gt911->initialized = true;
    gt911_device.priv = gt911;

    if (rtos_task_create(NULL, ((const char *)"gt911_work"), gt911_work, gt911, 1024 * 4, 3) != RTK_SUCCESS) {
        RTK_LOGE(LOG_TAG, "Failed to create gt911_work\n\r");
    }
}

static void gt911_deinit_ops(void) {
    struct gt911_data *gt911 = (struct gt911_data*)gt911_device.priv;
    CHECK_AND_RETURN(gt911);

    gt911->initialized = false;
    rtos_mutex_delete_static(gt911->lock);
    rtos_queue_delete(gt911->work_queue);
    gpio_irq_deinit(&gt911->gpio_irq);
    rtos_mem_free(gt911);
    gt911_device.priv = NULL;
}

static void gt911_enable_ops(void) {
    struct gt911_data *gt911 = (struct gt911_data*)gt911_device.priv;
    CHECK_AND_RETURN(gt911);

    rtos_mutex_take(gt911->lock, MUTEX_WAIT_TIMEOUT);
    gt911->enabled = true;
    gpio_irq_enable(&gt911->gpio_irq);
    rtos_mutex_give(gt911->lock);
}

static void gt911_disable_ops(void) {
    struct gt911_data *gt911 = (struct gt911_data*)gt911_device.priv;
    CHECK_AND_RETURN(gt911);

    rtos_mutex_take(gt911->lock, MUTEX_WAIT_TIMEOUT);
    gt911->enabled = false;
    gpio_irq_disable(&gt911->gpio_irq);
    rtos_mutex_give(gt911->lock);
}

static int gt911_ioctl_ops(u32 cmd, void *arg) {
    return -1;
}

static input_device_ops_t gt911_ops = {
    .init = gt911_init_ops,
    .deinit = gt911_deinit_ops,
    .enable = gt911_enable_ops,
    .disable = gt911_disable_ops,
    .ioctl = gt911_ioctl_ops,
};

input_device_t *input_touch_gt911_init(void) {
    snprintf(gt911_device.info.name, sizeof(gt911_device.info.name), "gt911");
    gt911_device.info.type = INPUT_DEV_TOUCH;
    gt911_device.info.state = INPUT_DEV_DISABLED;
    gt911_device.info.capabilities = INPUT_CAP_TOUCH;

    gt911_device.ops = gt911_ops;
    gt911_device.register_callback = gt911_register_callback;
    gt911_device.priv = NULL;
    return &gt911_device;
}

