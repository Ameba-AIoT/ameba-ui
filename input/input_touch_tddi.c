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

#include "i2c_api.h"
#include "i2c_ex_api.h"
#include "gpio_api.h"
#include "gpio_irq_api.h"
#include "ameba_soc.h"
#include "input.h"

#define LOG_TAG "TDDI"

/* Pin Definitions */
#define RST_PIN _PA_10
#define INT_PIN _PA_9
#define SDA_PIN _PB_10
#define SCL_PIN _PB_11

/* I2C slave address */
/* From spec: addr i2c = 0x28, cmd i2c = 0x56 (0x28 << 1 for write) */
#define I2C_ADDR 0x56
#define I2C_BUS_CLK 1000000

/* Detected I2C address (updated after probe) */
static u8 s_i2c_addr = I2C_ADDR;

/* Register Definitions (from sitronix_ts.h) */
#define ST_REG_TOUCH_INFO       0x10
#define ST_REG_FIRMWARE_VERSION 0x00
#define ST_REG_CHIP_ID          0xF4
#define ST_REG_X_RESOLUTION_H   0x05
#define ST_REG_X_RESOLUTION_L   0x06
#define ST_REG_Y_RESOLUTION_H   0x07
#define ST_REG_Y_RESOLUTION_L   0x08
#define ST_REG_MAX_TOUCHES      0x09

/* Configuration */
#define TPD_MAX_FINGERS 5
#define XSIZE 480
#define YSIZE 480

#define CHECK_AND_RETURN(p) \
    do { \
        if (!(p)) { \
            return; \
        } \
    } while(0)

#define MSG_Q_SIZE 10

struct tddi_data {
    i2c_t client;
    gpio_irq_t gpio_irq;
    rtos_mutex_t lock;
    rtos_queue_t work_queue;
    bool initialized;
    bool enabled;
    u16 x;
    u16 y;
};

static input_device_t tddi_device;
static void (*s_user_cb)(input_event_t *) = NULL;

/*
 * I2C write: send [addr_hi, addr_lo, data...]
 * 16-bit register address, big-endian (same as CST328)
 */
static int tddi_i2c_write(i2c_t *client, u16 reg, u8 *buf, u8 len)
{
    u8 *temp;
    int ret = 0;

    temp = malloc(len + 2);
    if (!temp) {
        RTK_LOGE(LOG_TAG, "Failed to allocate memory\n");
        return -1;
    }

    temp[0] = (reg >> 8) & 0xFF;
    temp[1] = reg & 0xFF;
    memcpy(temp + 2, buf, len);

    ret = i2c_write(client, s_i2c_addr, (char *)temp, len + 2, 2);
    free(temp);

    if (ret != (len + 2)) {
        RTK_LOGE(LOG_TAG, "%s: Write to slave error\n", __func__);
        return -1;
    }

    return 1;
}

/*
 * I2C read: send 2-byte register address (big-endian), then read data.
 * Uses separate write+read transactions (with STOP between them), same
 * pattern as both CST328 driver and Sitronix Linux driver. The 150µs
 * delay is required for NonStretch FW (from sitronix_ts_i2c.c).
 */
static int tddi_i2c_read(i2c_t *client, u16 reg, u8 *buf, u8 len)
{
    u8 addr_buf[2];
    int ret = 0;

    /* Register address in big-endian (MSB first) */
    addr_buf[0] = (reg >> 8) & 0xFF;
    addr_buf[1] = reg & 0xFF;

    ret = i2c_write(client, s_i2c_addr, (char *)addr_buf, 2, 1);
    if (ret != 2) {
        RTK_LOGE(LOG_TAG, "%s: Slave no ACK before read (addr=0x%02X)\n", __func__, s_i2c_addr);
        return -1;
    }

    /* Delay for NonStretch FW (150µs from sitronix_ts_i2c.c Linux driver) */
    DelayUs(150);

    ret = i2c_read(client, s_i2c_addr, (char *)buf, len, 1);
    if (ret < 0) {
        RTK_LOGE(LOG_TAG, "%s: Read failed\n", __func__);
        return -1;
    }

    return ret;
}

static int tddi_read_reg(i2c_t *client, u16 reg, u8 *value)
{
    return tddi_i2c_read(client, reg, value, 1);
}

static int tddi_write_reg(i2c_t *client, u16 reg, u8 value)
{
    return tddi_i2c_write(client, reg, &value, 1);
}

/* Hardware reset */
static void tddi_reset(struct tddi_data *ts)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RTK_LOGI(LOG_TAG, "Reset: init RST pin PA10 as output\n");

    /* Initialize reset pin */
    GPIO_InitStructure.GPIO_Pin = RST_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStructure);

    RTK_LOGI(LOG_TAG, "Reset: RST low (20ms)\n");
    GPIO_WriteBit(RST_PIN, 0);
    rtos_time_delay_ms(20);
    RTK_LOGI(LOG_TAG, "Reset: RST high (100ms)\n");
    GPIO_WriteBit(RST_PIN, 1);
    rtos_time_delay_ms(100);

    RTK_LOGI(LOG_TAG, "Reset: init INT pin PA9 as input pull-up\n");

    /* Initialize interrupt pin as input with pull-up */
    GPIO_InitStructure.GPIO_Pin = INT_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_Init(&GPIO_InitStructure);

    rtos_time_delay_ms(50);
    RTK_LOGI(LOG_TAG, "Reset: complete\n");
}

/* Verify I2C communication by reading chip info */
static int tddi_verify_communication(struct tddi_data *ts)
{
    u8 buf[8] = {0};

    RTK_LOGI(LOG_TAG, "Verifying I2C communication (addr=0x%02X)...\n", I2C_ADDR);

    /* Read FIRMWARE_VERSION (0x00) - 8 bytes */
    if (tddi_i2c_read(&ts->client, ST_REG_FIRMWARE_VERSION, buf, 8) < 0) {
        RTK_LOGE(LOG_TAG, "I2C read FAILED - addr 0x%02X not responding\n", I2C_ADDR);
        return -1;
    }

    RTK_LOGI(LOG_TAG, "I2C OK - reg 0x00 raw: %02X %02X %02X %02X %02X %02X %02X %02X\n",
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    RTK_LOGI(LOG_TAG, "FW version=0x%02X, status=0x%02X, sensing=%02X%02X\n",
             buf[0], buf[1], buf[2], buf[3]);

    /* Try reading CHIP_ID (0xF4) */
    if (tddi_read_reg(&ts->client, ST_REG_CHIP_ID, &buf[0]) < 0) {
        RTK_LOGW(LOG_TAG, "Read CHIP_ID failed\n");
    } else {
        RTK_LOGI(LOG_TAG, "CHIP_ID = 0x%02X\n", buf[0]);
    }

    return 0;
}

/*
 * Process touch data from TOUCH_INFO register (0x10).
 * Data format (from sitronix_ts.c:IRQ handler):
 *   coord_buf[0] = header
 *   coord_buf[4] onwards = 7 bytes per touch point
 *   Per point: byte0[7]=active, X=(byte0[5:0]<<8)|byte1, Y=(byte2[5:0]<<8)|byte3
 */
static void tddi_process_touch_data(struct tddi_data *ts)
{
    CHECK_AND_RETURN(ts);

    u8 buf[40] = {0};
    u8 read_len = TPD_MAX_FINGERS * 7 + 5;
    int ret;
    int i;
    bool touch_found = false;
    u16 input_x = 0;
    u16 input_y = 0;

    rtos_mutex_take(ts->lock, MUTEX_WAIT_TIMEOUT);

    {
        u8 val;
        if (tddi_read_reg(&ts->client, 0x04, &val) >= 0) {
            RTK_LOGD(LOG_TAG, "Process: reg 0x04 = 0x%02X\n", val);
        } else {
            RTK_LOGD(LOG_TAG, "Process: reg 0x04 read FAILED\n");
        }
    }

    RTK_LOGD(LOG_TAG, "Process: reading TOUCH_INFO(0x10) len=%d\n", read_len);
    ret = tddi_i2c_read(&ts->client, ST_REG_TOUCH_INFO, buf, read_len);
    if (ret < 0) {
        RTK_LOGW(LOG_TAG, "%s: Read TOUCH_INFO fail\n", __func__);
        goto err_finish;
    }

    RTK_LOGD(LOG_TAG, "Process: RAW buf: %02X %02X %02X %02X %02X %02X %02X %02X ...\n",
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    RTK_LOGD(LOG_TAG, "Process: header=0x%02X (ESD=%d, prox=%d)\n",
             buf[0], (buf[0] & 0x80) ? 1 : 0, (buf[0] >> 4) & 0x07);

    /* Parse touch points, 7 bytes each starting at offset 4.
     * Coordinate sanity check: reject points outside the valid display area
     * to filter out garbage data that may have bit7 accidentally set. */
    for (i = 0; i < TPD_MAX_FINGERS; i++) {
        u8 *p = &buf[4 + i * 7];

        RTK_LOGD(LOG_TAG, "Process: point[%d]: %02X %02X %02X %02X %02X %02X %02X (active=%d)\n",
                 i, p[0], p[1], p[2], p[3], p[4], p[5], p[6], (*p & 0x80) ? 1 : 0);

        if (*p & 0x80) {
            /* Touch point is active */
            input_x = (u16)(((u16)(*p & 0x3F) << 8) | (u16)*(p + 1));
            input_y = (u16)(((u16)(*(p + 2) & 0x3F) << 8) | (u16)*(p + 3));

            /* Reject coordinates outside valid range (garbage filtering) */
            if (input_x >= XSIZE || input_y >= YSIZE) {
                RTK_LOGD(LOG_TAG, "Process: point[%d] OUT OF RANGE (%d,%d), skipping\n",
                         i, input_x, input_y);
                continue;
            }

            RTK_LOGD(LOG_TAG, "Process: TOUCH point[%d] x=%d y=%d\n", i, input_x, input_y);

            ts->x = input_x;
            ts->y = input_y;
            touch_found = true;
            break; /* Report first valid touch only (single-touch mode) */
        }
    }

    if (touch_found) {
        RTK_LOGD(LOG_TAG, "Process: reporting TOUCH PRESS (%d, %d)\n", ts->x, ts->y);
        if (s_user_cb) {
            input_event_t event;
            event.type = INPUT_EVENT_TOUCH;
            event.timestamp = 0;
            event.data.touch.x = ts->x;
            event.data.touch.y = ts->y;
            event.data.touch.pressed = 1;
            event.data.touch.touch_id = 0;
            s_user_cb(&event);
        }
    } else {
        RTK_LOGD(LOG_TAG, "Process: no touch points, reporting RELEASE\n");
        /* No touch points active - send release if we were pressed */
        if (s_user_cb) {
            input_event_t event;
            event.type = INPUT_EVENT_TOUCH;
            event.timestamp = 0;
            event.data.touch.x = ts->x;
            event.data.touch.y = ts->y;
            event.data.touch.pressed = 0;
            event.data.touch.touch_id = 0;
            s_user_cb(&event);
        }
    }

err_finish:
    rtos_mutex_give(ts->lock);
}

static int tddi_work_handler(struct tddi_data *ts)
{
    void *p_msg = NULL;

    if (ts->enabled) {
        if (RTK_SUCCESS == rtos_queue_receive(ts->work_queue, &p_msg, 20)) {
            RTK_LOGD(LOG_TAG, "Work: processing touch data\n");
            tddi_process_touch_data(ts);
            return 0;
        }
    }

    return -1;
}

static void tddi_irq_handler(u32 dev_id, u32 event)
{
    (void) event;
    struct tddi_data *ts = (struct tddi_data *)dev_id;
    CHECK_AND_RETURN(ts);

    RTK_LOGD(LOG_TAG, "IRQ: triggered (event=0x%x), sending to work queue\n", event);

    if (RTK_SUCCESS != rtos_queue_send(ts->work_queue, ts, 20)) {
        RTK_LOGW(LOG_TAG, "%s, send queue failed\n", __func__);
    }
}

static void tddi_work(void *param)
{
    struct tddi_data *ts = (struct tddi_data *)param;
    CHECK_AND_RETURN(ts);

    while (ts->initialized) {
        if (ts->enabled) {
            if (tddi_work_handler(ts)) {
                rtos_time_delay_ms(20);
            }
        } else {
            rtos_time_delay_ms(20);
        }
    }

    rtos_task_delete(NULL);
}

static int tddi_init_gpio_irq(struct tddi_data *ts)
{
    RTK_LOGI(LOG_TAG, "IRQ: init INT on PA9, falling edge\n");
    gpio_irq_init(&ts->gpio_irq, INT_PIN, tddi_irq_handler, (u32)ts);
    gpio_irq_set(&ts->gpio_irq, IRQ_FALL, 1);
    gpio_irq_enable(&ts->gpio_irq);
    RTK_LOGI(LOG_TAG, "IRQ: enabled\n");

    return 0;
}

static int tddi_init_i2c(struct tddi_data *ts)
{
    RTK_LOGI(LOG_TAG, "I2C: init SDA=PB10 SCL=PB11 freq=%d\n", I2C_BUS_CLK);
    i2c_init(&ts->client, SDA_PIN, SCL_PIN);
    i2c_frequency(&ts->client, I2C_BUS_CLK);
    RTK_LOGI(LOG_TAG, "I2C: init complete\n");

    return 0;
}

static void tddi_init_chip(struct tddi_data *ts)
{
    RTK_LOGI(LOG_TAG, "Init chip: starting HW reset\n");
    tddi_reset(ts);
    rtos_time_delay_ms(100);

    RTK_LOGI(LOG_TAG, "Init chip: verifying I2C\n");
    tddi_verify_communication(ts);

    RTK_LOGI(LOG_TAG, "Init chip: setting up IRQ\n");
    tddi_init_gpio_irq(ts);

    RTK_LOGI(LOG_TAG, "Init chip: complete\n");
}

static void tddi_init_ops(void)
{
    RTK_LOGI(LOG_TAG, "Init ops: entering\n");
    CHECK_AND_RETURN(!tddi_device.priv);

    RTK_LOGI(LOG_TAG, "Init ops: allocating device data\n");
    struct tddi_data *tddi = (struct tddi_data *)rtos_mem_zmalloc(sizeof(struct tddi_data));
    if (!tddi) {
        RTK_LOGE(LOG_TAG, "Init ops: malloc failed!\n");
        return;
    }

    RTK_LOGI(LOG_TAG, "Init ops: creating mutex and queue\n");
    rtos_mutex_create_static(&tddi->lock);
    rtos_mutex_give(tddi->lock);
    rtos_queue_create(&tddi->work_queue, MSG_Q_SIZE, sizeof(struct tddi_data *));

    RTK_LOGI(LOG_TAG, "Init ops: initializing I2C\n");
    tddi_init_i2c(tddi);
    RTK_LOGI(LOG_TAG, "Init ops: initializing chip\n");
    tddi_init_chip(tddi);

    tddi->initialized = true;
    tddi_device.priv = tddi;
    RTK_LOGI(LOG_TAG, "Init ops: device data stored, initialzied=true\n");

    RTK_LOGI(LOG_TAG, "Init: creating work task\n");
    if (rtos_task_create(NULL, ((const char *)"tddi_work"), tddi_work, tddi, 1024 * 4, 3) != RTK_SUCCESS) {
        RTK_LOGE(LOG_TAG, "Failed to create tddi_work\n\r");
    } else {
        RTK_LOGI(LOG_TAG, "Init: work task created\n");
    }
}

static void tddi_deinit_ops(void)
{
    struct tddi_data *tddi = (struct tddi_data *)tddi_device.priv;
    CHECK_AND_RETURN(tddi);

    tddi->initialized = false;
    rtos_mutex_delete_static(tddi->lock);
    rtos_queue_delete(tddi->work_queue);
    gpio_irq_deinit(&tddi->gpio_irq);
    rtos_mem_free(tddi);
    tddi_device.priv = NULL;
}

static void tddi_enable_ops(void)
{
    struct tddi_data *tddi = (struct tddi_data *)tddi_device.priv;
    CHECK_AND_RETURN(tddi);

    RTK_LOGI(LOG_TAG, "Enable: enabling touch\n");
    rtos_mutex_take(tddi->lock, MUTEX_WAIT_TIMEOUT);
    tddi->enabled = true;
    gpio_irq_enable(&tddi->gpio_irq);
    rtos_mutex_give(tddi->lock);
    RTK_LOGI(LOG_TAG, "Enable: touch enabled\n");
}

static void tddi_disable_ops(void)
{
    struct tddi_data *tddi = (struct tddi_data *)tddi_device.priv;
    CHECK_AND_RETURN(tddi);

    rtos_mutex_take(tddi->lock, MUTEX_WAIT_TIMEOUT);
    tddi->enabled = false;
    gpio_irq_disable(&tddi->gpio_irq);
    rtos_mutex_give(tddi->lock);
}

static int tddi_ioctl_ops(u32 cmd, void *arg)
{
    return -1;
}

static input_device_ops_t tddi_ops = {
    .init = tddi_init_ops,
    .deinit = tddi_deinit_ops,
    .enable = tddi_enable_ops,
    .disable = tddi_disable_ops,
    .ioctl = tddi_ioctl_ops,
};

static void tddi_register_callback(void (*cb)(input_event_t *))
{
    s_user_cb = cb;
}

input_device_t *input_touch_tddi_init(void)
{
    RTK_LOGI(LOG_TAG, "Entry: creating touch device\n");
    snprintf(tddi_device.info.name, sizeof(tddi_device.info.name), "tddi");
    tddi_device.info.type = INPUT_DEV_TOUCH;
    tddi_device.info.state = INPUT_DEV_DISABLED;
    tddi_device.info.capabilities = INPUT_CAP_TOUCH;

    tddi_device.ops = tddi_ops;
    tddi_device.register_callback = tddi_register_callback;
    tddi_device.priv = NULL;

    return &tddi_device;
}
