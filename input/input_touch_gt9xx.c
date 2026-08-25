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

#define LOG_TAG "GT9XX"

/*
 * Ported from Linux driver drivers/rtkdrivers/touchscreen/gt9xx.c (Goodix GT9xx).
 * Only the bring-up path (reset, i2c bus test, firmware id read, touch point
 * report) is kept; ESD watchdog, config-table download, firmware update,
 * pinctrl and sysfs plumbing are Linux platform infra with no RTOS equivalent
 * here and are dropped, matching the existing input_touch_gt911.c/cst328.c
 * porting convention in this component.
 *
 * Pin mapping cross-checked against the Linux DTS board files
 * (rtl8730elm-va7-full.dts / rtl8730elm-va7-tests-display.dts):
 *   gt9xx@5d on &i2c2, reset-gpios = gpioa 10, irq-gpios = gpioa 9
 *   i2c2_pins (rtl8730e-pinctrl.dtsi): SDA = PB_10, SCL = PB_11
 * i.e. GT9xx shares the same I2C2/PA_10/PA_9 wiring as CST328 on this board,
 * NOT the PB_0/PA_31/PA_29/PA_30 set used by input_touch_gt911.c.
 */

#define RST_PIN                 _PA_10
#define INT_PIN                 _PA_9
#define SDA_PIN                 _PB_10
#define SCL_PIN                 _PB_11
#define XSIZE                   1024
#define YSIZE                   600
#define TRANSFORM_EXCHANGE_X_Y  0
#define TRANSFORM_INVERSE_X     0
#define TRANSFORM_INVERSE_Y     0

/* Register map, from gt9xx.h GTP_REG_* (GTP_ADDR_LENGTH = 2 bytes). */
#define GTP_REG_COMMAND         0x8040
#define GTP_REG_COMMAND_CHECK   0x8046
#define GTP_REG_CONFIG_DATA     0x8047
#define GTP_REG_VERSION         0x8140
#define GTP_REG_SENSOR_ID       0x814A
#define GTP_READ_COOR_ADDR      0x814E

#define GTP_MAX_TOUCH_ID        16
#define MASK_BIT_8              0x80

#define TPD_MAX_FINGERS         5
/*
 * GT9xx address selection happens during reset: the INT pin level sampled
 * while RST is released selects 0x5d (INT low) or 0x14 (INT high).
 * Linux dts board file uses "gt9xx@5d" (reg = <0x5d>), so the reset sequence
 * below must hold INT low through the release edge to land on 0x5d.
 */
#define I2C_ADDR                0x5d
#define I2C_BUS_CLK             400000

#define RETRY_MAX_TIMES         5

#define CHECK_AND_RETURN(p) \
    do { \
        if (!(p)) { \
            return; \
        } \
    } while(0)

#define MSG_Q_SIZE 20

/* Device structure */
struct gt9xx_data {
    u16 x;
    u16 y;

    bool initialized;
    bool enabled;
    gpio_irq_t gpio_irq;
    i2c_t client;
    rtos_queue_t work_queue;
    rtos_mutex_t lock;
};

static input_device_t gt9xx_device;
static input_event_callback_t s_user_cb = NULL;

/* I2C read/write helper functions. Mirrors gtp_i2c_read/gtp_i2c_write
 * (16-bit register address, MSB first). */
static int gt9xx_i2c_read(i2c_t *client, u16 reg, u8 *buf, int len)
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

static int gt9xx_i2c_write(i2c_t *client, u16 reg, u8 *buf, int len)
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

static int gt9xx_read_reg(i2c_t *client, u16 reg, u8 *value)
{
    return gt9xx_i2c_read(client, reg, value, 1);
}

static int gt9xx_write_reg(i2c_t *client, u16 reg, u8 value)
{
    return gt9xx_i2c_write(client, reg, &value, 1);
}

static void gt9xx_raw_callback(u16 x, u16 y, u8 state)
{
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

/*
 * Address-select reset sequence, per gtp_reset_guitar()/gtp_int_output():
 * the INT pin level sampled at the RST release edge selects the I2C address
 * (INT high -> 0x14, INT low -> 0x5d). Target address here is 0x5d, so INT
 * must be held LOW across the RST release edge (opposite of the GT911 file
 * this was ported from, which targets 0x14 and drives INT high beforehand).
 */
static void board_i2c_init(void)
{
    GPIO_WriteBit(INT_PIN, 0);
    GPIO_WriteBit(RST_PIN, 0);
    DelayMs(10);
    DelayUs(1000);          /* T3 > 100us, INT stays low (select 0x5d) */
    GPIO_WriteBit(RST_PIN, 1);  /* release reset: address latched here */
    DelayMs(6);              /* T4: 5ms <= T4 < 10ms */
    GPIO_WriteBit(INT_PIN, 0);
    DelayMs(50);
}

/* Reset chip. Ported from gtp_reset_guitar()'s timing (T2/T3/T4/T5). */
static void gt9xx_reset(struct gt9xx_data *ts)
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
}

/* i2c bus sanity check. Ported from gtp_i2c_test(): read back
 * GTP_REG_CONFIG_DATA a few times, expect the 2-byte address ack. */
static int gt9xx_i2c_test(i2c_t *client)
{
    u8 test[3] = {0};
    u8 retry = 0;
    int ret = -1;

    while (retry++ < RETRY_MAX_TIMES) {
        ret = gt9xx_i2c_read(client, GTP_REG_CONFIG_DATA, test, 1);
        if (ret >= 0) {
            return 0;
        }

        RTK_LOGW(LOG_TAG, "i2c test failed, retry %d\n", retry);
        rtos_time_delay_ms(10);
    }

    return -1;
}

/* Read product id / firmware version / sensor id. Ported from
 * gtp_get_fw_info(): buf layout is [addr_hi, addr_lo, pid0..3, ver_lo, ver_hi]. */
static int gt9xx_read_product_info(struct gt9xx_data *ts)
{
    u8 buf[8] = {0};
    u16 version;

    if (gt9xx_i2c_read(&ts->client, GTP_REG_VERSION, buf, 6) < 0) {
        RTK_LOGE(LOG_TAG, "Failed to read fw_info\n");
        return -1;
    }

    version = (u16)((buf[5] << 8) | buf[4]);

    RTK_LOGI(LOG_TAG, "%s CTP PID:%c%c%c%c version:%04x\n", __func__,
             buf[0], buf[1], buf[2], buf[3], version);

    return 0;
}

/* Process touch data. Ported from gtp_get_points()/gtp_work_func(),
 * trimmed to single-point reporting (matches input_touch_gt911.c behaviour;
 * TPD_MAX_FINGERS kept as a sanity bound only). */
static void gt9xx_process_touch_data(struct gt9xx_data *ts)
{
    CHECK_AND_RETURN(ts);
    u8 read[10] = { 0 };
    u8 mode = 0;
    u8 point_num = 0;
    u8 reset = 0;
    int ret = 0;
    int len = 0;

    rtos_mutex_take(ts->lock, MUTEX_WAIT_TIMEOUT);
    ret = gt9xx_read_reg(&ts->client, GTP_READ_COOR_ADDR, &mode);

    if (ret < 0) {
        RTK_LOGW(LOG_TAG, "%s: Read mode fail. \r\n", __func__);
        goto err_finish;
    }

    RTK_LOGD(LOG_TAG, "%s: mode: %x \r\n", __func__, mode);
    read[0] = mode;

    if ((mode & MASK_BIT_8) == 0) {
        goto err_finish;
    }

    point_num = mode & 0x0F;

    if (point_num == 0 || point_num > TPD_MAX_FINGERS) {
        ret = gt9xx_write_reg(&ts->client, GTP_READ_COOR_ADDR, reset); // clear flags
        goto err_finish;
    }

    len = gt9xx_i2c_read(&ts->client, GTP_READ_COOR_ADDR + 1, read + 1, 7);

    if (len < 0) {
        RTK_LOGW(LOG_TAG, "%s: Slave no response. \r\n", __func__);
        goto err_finish;
    }

    ret = gt9xx_write_reg(&ts->client, GTP_READ_COOR_ADDR, reset);
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

        gt9xx_raw_callback(x, y, state);
        RTK_LOGD(LOG_TAG, "x:%d y:%d pressure:%d\n", x, y, state);
    }

    rtos_mutex_give(ts->lock);
}

/* Work queue handler function */
static int gt9xx_work_handler(struct gt9xx_data *ts)
{
    void *p_msg = NULL;

    if (ts->enabled) {
        if (RTK_SUCCESS == rtos_queue_receive(ts->work_queue, &p_msg, 20)) {
            gt9xx_process_touch_data(ts);
            return 0;
        }
    }

    return -1;
}

/* Interrupt handler function */
static void gt9xx_irq_handler(u32 dev_id, u32 event)
{
    (void) event;
    struct gt9xx_data *ts = (struct gt9xx_data *)dev_id;
    CHECK_AND_RETURN(ts);

    /* Add work to work queue */
    if (RTK_SUCCESS != rtos_queue_send(ts->work_queue, ts, 0)) {
        RTK_LOGW(LOG_TAG, "%s, send queue failed\n", __func__);
    }
}

/* Initialize chip. Ported from gtp_probe()'s bring-up sequence:
 * reset -> i2c bus test -> read product info -> arm irq. */
static void gt9xx_init_chip(struct gt9xx_data *ts)
{
    gt9xx_reset(ts);
    rtos_time_delay_ms(100);

    if (gt9xx_i2c_test(&ts->client)) {
        RTK_LOGE(LOG_TAG, "Failed communicate with IC use I2C\n");
    }

    gt9xx_read_product_info(ts);
    gpio_irq_init(&ts->gpio_irq, INT_PIN, gt9xx_irq_handler, (u32)ts);
    /* Linux dts: irq-flags = <2> (IRQ_TYPE_EDGE_FALLING), matches CST328 wiring. */
    gpio_irq_set(&ts->gpio_irq, IRQ_FALL, 1);
}

static void gt9xx_register_callback(void (*cb)(input_event_t *))
{
    s_user_cb = cb;
}

static void gt9xx_work(void *param)
{
    struct gt9xx_data *ts = (struct gt9xx_data *) param;
    CHECK_AND_RETURN(ts);

    while (ts->initialized) {
        if (ts->enabled) {
            if (gt9xx_work_handler(ts)) {
                rtos_time_delay_ms(20);
            }
        } else {
            rtos_time_delay_ms(20);
        }
    }

    rtos_task_delete(NULL);
}

static void gt9xx_init_ops(void)
{
    RTK_LOGI(LOG_TAG, "Touch device initialized\n");
    CHECK_AND_RETURN(!gt9xx_device.priv);
    struct gt9xx_data *gt9xx = (struct gt9xx_data *) rtos_mem_zmalloc(sizeof(struct gt9xx_data));
    rtos_mutex_create_static(&gt9xx->lock);
    rtos_mutex_give(gt9xx->lock);
    rtos_queue_create(&gt9xx->work_queue, MSG_Q_SIZE, sizeof(uint32_t));
    gt9xx_init_chip(gt9xx);
    gt9xx->initialized = true;
    gt9xx_device.priv = gt9xx;

    if (rtos_task_create(NULL, ((const char *)"gt9xx_work"), gt9xx_work, gt9xx, 1024 * 4, 3) != RTK_SUCCESS) {
        RTK_LOGE(LOG_TAG, "Failed to create gt9xx_work\n\r");
    }
}

static void gt9xx_deinit_ops(void)
{
    struct gt9xx_data *gt9xx = (struct gt9xx_data *)gt9xx_device.priv;
    CHECK_AND_RETURN(gt9xx);

    gt9xx->initialized = false;
    rtos_mutex_delete_static(gt9xx->lock);
    rtos_queue_delete(gt9xx->work_queue);
    gpio_irq_deinit(&gt9xx->gpio_irq);
    rtos_mem_free(gt9xx);
    gt9xx_device.priv = NULL;
}

static void gt9xx_enable_ops(void)
{
    struct gt9xx_data *gt9xx = (struct gt9xx_data *)gt9xx_device.priv;
    CHECK_AND_RETURN(gt9xx);

    rtos_mutex_take(gt9xx->lock, MUTEX_WAIT_TIMEOUT);
    gt9xx->enabled = true;
    gpio_irq_enable(&gt9xx->gpio_irq);
    rtos_mutex_give(gt9xx->lock);
}

static void gt9xx_disable_ops(void)
{
    struct gt9xx_data *gt9xx = (struct gt9xx_data *)gt9xx_device.priv;
    CHECK_AND_RETURN(gt9xx);

    rtos_mutex_take(gt9xx->lock, MUTEX_WAIT_TIMEOUT);
    gt9xx->enabled = false;
    gpio_irq_disable(&gt9xx->gpio_irq);
    rtos_mutex_give(gt9xx->lock);
}

static int gt9xx_ioctl_ops(u32 cmd, void *arg)
{
    return -1;
}

static input_device_ops_t gt9xx_ops = {
    .init = gt9xx_init_ops,
    .deinit = gt9xx_deinit_ops,
    .enable = gt9xx_enable_ops,
    .disable = gt9xx_disable_ops,
    .ioctl = gt9xx_ioctl_ops,
};

input_device_t *input_touch_gt9xx_init(void)
{
    snprintf(gt9xx_device.info.name, sizeof(gt9xx_device.info.name), "gt9xx");
    gt9xx_device.info.type = INPUT_DEV_TOUCH;
    gt9xx_device.info.state = INPUT_DEV_DISABLED;
    gt9xx_device.info.capabilities = INPUT_CAP_TOUCH;

    gt9xx_device.ops = gt9xx_ops;
    gt9xx_device.register_callback = gt9xx_register_callback;
    gt9xx_device.priv = NULL;
    return &gt9xx_device;
}
