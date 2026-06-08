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

#include <stdlib.h>
#include "i2c_api.h"
#include "i2c_ex_api.h"
#include "gpio_api.h"
#include "gpio_irq_api.h"
#include "input.h"

#define LOG_TAG "CST328"

/* Pin Definitions */
#define RST_PIN _PA_10
#define INT_PIN _PA_9
#define SDA_PIN _PB_10
#define SCL_PIN _PB_11

/* Register Definitions */
#define CST_TP1_REG 0xD000
#define CST_TP2_REG 0xD007
#define CST_TP3_REG 0xD00C
#define CST_TP4_REG 0xD011
#define CST_TP5_REG 0xD017
#define CST_DEBUG_INFO_MODE 0xD101
#define CST_FW_INFO 0xD208
#define CST_DEVIDE_MODE 0xD109

/* Configuration */
#define TPD_MAX_FINGERS 5
#define I2C_ADDR 0x1A
#define I2C_BUS_CLK 400000
#define CST328_DEBUG 1

#define XSIZE 480
#define YSIZE 800

#define CHECK_AND_RETURN(p) \
    do { \
        if (!(p)) { \
            return; \
        } \
    } while(0)

#define MSG_Q_SIZE 10

struct cst328_data {
    i2c_t client;
    gpio_irq_t gpio_irq;
    rtos_mutex_t lock;
    rtos_queue_t work_queue;
    bool initialized;
    bool enabled;
    u16 x;
    u16 y;
    bool is_pressed;
};

static input_device_t cst328_device;
static void (*s_user_cb)(input_event_t *) = NULL;

static int cst328_i2c_write(i2c_t *client, u16 reg, u8 *buf, u8 len)
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

    ret = i2c_write(client, I2C_ADDR, (char *)temp, len + 2, 2);
    free(temp);

    if (ret != (len + 2)) {
        RTK_LOGE(LOG_TAG, "%s: Write to slave error\n", __func__);
        return -1;
    }

    return 1;
}

static int cst328_i2c_read(i2c_t *client, u16 reg, u8 *buf, u8 len)
{
    u16 r;
    int ret = 0;

    r = reg & 0xFF;
    reg = (reg >> 8) | (r << 8);

    ret = i2c_write(client, I2C_ADDR, (char *)&reg, 2, 1);
    if (ret != 2) {
        RTK_LOGE(LOG_TAG, "%s: Slave no ACK before read\n", __func__);
        return -1;
    }

    return i2c_read(client, I2C_ADDR, (char *)buf, len, 1);
}

static int cst328_read_reg(i2c_t *client, u16 reg, u8 *value)
{
    return cst328_i2c_read(client, reg, value, 1);
}

static int cst328_write_reg(i2c_t *client, u16 reg, u8 value)
{
    return cst328_i2c_write(client, reg, &value, 1);
}

static void cst328_reset(struct cst328_data *ts)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Initialize reset pin */
    GPIO_InitStructure.GPIO_Pin = RST_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(&GPIO_InitStructure);

    GPIO_WriteBit(RST_PIN, 0);
    rtos_time_delay_ms(20);
    GPIO_WriteBit(RST_PIN, 1);
    rtos_time_delay_ms(100);

    /* Initialize interrupt pin */
    GPIO_InitStructure.GPIO_Pin = INT_PIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_Init(&GPIO_InitStructure);

    rtos_time_delay_ms(50);
}

static int cst328_read_firmware_info(struct cst328_data *ts)
{
    u8 buf[8];
    u8 temp = 0;
    u32 cst328_ic_version = 0;
    u16 cst328_ic_checksum = 0;

    cst328_write_reg(&ts->client, CST_DEBUG_INFO_MODE, temp);
    rtos_time_delay_ms(10);

    if (cst328_i2c_read(&ts->client, CST_FW_INFO, buf, 8) < 0) {
        RTK_LOGE(LOG_TAG, "Failed to read firmware info\n");
        return -1;
    }

    cst328_ic_version = buf[3];
    cst328_ic_version <<= 8;
    cst328_ic_version |= buf[2];
    cst328_ic_version <<= 8;
    cst328_ic_version |= buf[1];
    cst328_ic_version <<= 8;
    cst328_ic_version |= buf[0];

    cst328_ic_checksum = buf[7];
    cst328_ic_checksum <<= 8;
    cst328_ic_checksum |= buf[6];
    cst328_ic_checksum <<= 8;
    cst328_ic_checksum |= buf[5];
    cst328_ic_checksum <<= 8;
    cst328_ic_checksum |= buf[4];

    if (CST328_DEBUG) {
        RTK_LOGI(LOG_TAG, "Chip IC version: %lu, checksum: %u\n",
                 cst328_ic_version, cst328_ic_checksum);
    }

    if (cst328_ic_version == 0xA5A5A5A5) {
        RTK_LOGE(LOG_TAG, "Chip IC doesn't have firmware\n");
        return -1;
    }

    return 0;
}

static void cst328_process_touch_data(struct cst328_data *ts)
{
    CHECK_AND_RETURN(ts);

    u8 i2c_buf[8];
    u8 buf[30] = {0};
    int ret;
    int cnt;
    int idx;
    u8 sw;
    u16 input_x = 0;
    u16 input_y = 0;
    u16 reg;
    int i2c_len, len_1, len_2;

    rtos_mutex_take(ts->lock, MUTEX_WAIT_TIMEOUT);

    ret = cst328_i2c_read(&ts->client, CST_TP1_REG, buf, 7);
    if (ret < 0) {
        RTK_LOGW(LOG_TAG, "%s: Read CST_TP1_REG fail\n", __func__);
        goto err_finish;
    }

    if (buf[6] != 0xAB) {
        RTK_LOGW(LOG_TAG, "Scan data is not valid\n");
        i2c_buf[0] = 0xD0;
        i2c_buf[1] = 0x00;
        i2c_buf[2] = 0xAB;
        ret = i2c_write(&ts->client, I2C_ADDR, (char *)i2c_buf, 3, 1);
        if (ret < 0) {
            RTK_LOGE(LOG_TAG, "Send read touch info ending failed\n");
        }
        goto err_finish;
    }

    cnt = buf[5] & 0x7F;

    if (cnt > TPD_MAX_FINGERS) {
        RTK_LOGE(LOG_TAG, "Scan touch exceed max fingers\n");
        i2c_buf[0] = 0xD0;
        i2c_buf[1] = 0x00;
        i2c_buf[2] = 0xAB;
        ret = i2c_write(&ts->client, I2C_ADDR, (char *)i2c_buf, 3, 1);
        if (ret < 0) {
            RTK_LOGE(LOG_TAG, "Send read touch info ending failed\n");
        }
        goto err_finish;
    } else if (cnt == 0) {
        goto err_finish;
    }

    if (cnt > 1) {
        if (CST328_DEBUG) {
            RTK_LOGD(LOG_TAG, "Scan cnt: %d\n", cnt);
        }

        i2c_len = (cnt - 1) * 5 + 1;
        len_1 = i2c_len;

        for (idx = 0; idx < i2c_len; idx += 6) {
            i2c_buf[0] = 0xD0;
            i2c_buf[1] = 0x07 + idx;
            reg = (u16)((i2c_buf[0] << 8) | i2c_buf[1]);

            if (CST328_DEBUG) {
                RTK_LOGD(LOG_TAG, "Scan read touch reg: 0x%x\n", reg);
            }

            if (len_1 >= 6) {
                len_2 = 6;
                len_1 -= 6;
            } else {
                len_2 = len_1;
                len_1 = 0;
            }

            cst328_i2c_read(&ts->client, reg, i2c_buf, len_2);
            for (int i = 0; i < len_2; i++) {
                buf[5 + idx + i] = i2c_buf[i];
            }
        }
        i2c_len += 5;

        if (buf[i2c_len - 1] != 0xAB) {
            RTK_LOGE(LOG_TAG, "Scan data error!\n");
            buf[0] = 0xD0;
            buf[1] = 0x00;
            buf[2] = 0xAB;
            ret = i2c_write(&ts->client, I2C_ADDR, (char *)buf, 3, 1);
            if (ret < 0) {
                RTK_LOGE(LOG_TAG, "Send read touch info ending failed\n");
            }
            goto err_finish;
        }
    }

    i2c_buf[0] = 0xD0;
    i2c_buf[1] = 0x00;
    i2c_buf[2] = 0xAB;
    ret = i2c_write(&ts->client, I2C_ADDR, (char *)i2c_buf, 3, 1);
    if (ret < 0) {
        RTK_LOGE(LOG_TAG, "Scan send read touch info ending failed\n");
    }

    idx = 0;
    input_x = (u16)((buf[idx + 1] << 4) | ((buf[idx + 3] >> 4) & 0x0F));
    input_y = (u16)((buf[idx + 2] << 4) | (buf[idx + 3] & 0x0F));
    sw = (buf[idx] & 0x0F) >> 1;

    if (CST328_DEBUG) {
        RTK_LOGD(LOG_TAG, "CST328 Point x:%d, y:%d, sw:%d\n", input_x, input_y, sw);
    }

    if (sw == 0x03) {
        bool state_changed = false;
        if (!ts->is_pressed) {
            ts->is_pressed = true;
            state_changed = true;
        }
        ts->x = XSIZE - input_x;
        ts->y = YSIZE - input_y;

        if (s_user_cb) {
            input_event_t event;
            event.type = INPUT_EVENT_TOUCH;
            event.timestamp = 0;
            event.data.touch.x = ts->x;
            event.data.touch.y = ts->y;
            event.data.touch.touch_id = 0;

            if (state_changed) {
                event.data.touch.pressed = 1;
                RTK_LOGD(LOG_TAG, "Touch PRESS: (%d, %d)\n", ts->x, ts->y);
            } else {
                event.data.touch.pressed = 1;
                RTK_LOGD(LOG_TAG, "Touch MOVE: (%d, %d)\n", ts->x, ts->y);
            }
            s_user_cb(&event);
        }
    } else {
        if (ts->is_pressed) {
            ts->is_pressed = false;

            if (s_user_cb) {
                input_event_t event;
                event.type = INPUT_EVENT_TOUCH;
                event.timestamp = 0;
                event.data.touch.x = ts->x;
                event.data.touch.y = ts->y;
                event.data.touch.pressed = 0;
                event.data.touch.touch_id = 0;

                s_user_cb(&event);
                RTK_LOGD(LOG_TAG, "Touch RELEASE: (%d, %d)\n", ts->x, ts->y);
            }
        }
    }

err_finish:
    rtos_mutex_give(ts->lock);
}

static int cst328_work_handler(struct cst328_data *ts)
{
    void *p_msg = NULL;

    if (ts->enabled) {
        if (RTK_SUCCESS == rtos_queue_receive(ts->work_queue, &p_msg, 20)) {
            cst328_process_touch_data(ts);
            return 0;
        }
    }

    return -1;
}

static void cst328_irq_handler(u32 dev_id, u32 event)
{
    (void) event;
    struct cst328_data *ts = (struct cst328_data *)dev_id;
    CHECK_AND_RETURN(ts);

    if (RTK_SUCCESS != rtos_queue_send(ts->work_queue, ts, 0)) {
        RTK_LOGW(LOG_TAG, "%s, send queue failed\n", __func__);
    }
}

static void cst328_work(void *param)
{
    struct cst328_data *ts = (struct cst328_data *)param;
    CHECK_AND_RETURN(ts);

    while (ts->initialized) {
        if (ts->enabled) {
            if (cst328_work_handler(ts)) {
                rtos_time_delay_ms(20);
            }
        } else {
            rtos_time_delay_ms(20);
        }
    }

    rtos_task_delete(NULL);
}

static int cst328_init_gpio_irq(struct cst328_data *ts)
{
    gpio_irq_init(&ts->gpio_irq, INT_PIN, cst328_irq_handler, (u32)ts);
    gpio_irq_set(&ts->gpio_irq, IRQ_FALL, 1);
    gpio_irq_enable(&ts->gpio_irq);

    return 0;
}

static int cst328_init_i2c(struct cst328_data *ts)
{
    i2c_init(&ts->client, SDA_PIN, SCL_PIN);
    i2c_frequency(&ts->client, I2C_BUS_CLK);

    return 0;
}

static void cst328_init_chip(struct cst328_data *ts)
{
    u8 temp = 0;

    cst328_reset(ts);
    rtos_time_delay_ms(100);
    cst328_read_firmware_info(ts);

    cst328_write_reg(&ts->client, CST_DEVIDE_MODE, temp);

    cst328_init_gpio_irq(ts);
}

static void cst328_init_ops(void)
{
    RTK_LOGI(LOG_TAG, "Touch device initializing\n");
    CHECK_AND_RETURN(!cst328_device.priv);

    struct cst328_data *cst328 = (struct cst328_data *) rtos_mem_zmalloc(sizeof(struct cst328_data));
    rtos_mutex_create_static(&cst328->lock);
    rtos_mutex_give(cst328->lock);
    rtos_queue_create(&cst328->work_queue, MSG_Q_SIZE, sizeof(uint32_t));

    cst328_init_i2c(cst328);
    cst328_init_chip(cst328);

    cst328->initialized = true;
    cst328->is_pressed = false;
    cst328_device.priv = cst328;

    if (rtos_task_create(NULL, ((const char *)"cst328_work"), cst328_work, cst328, 1024 * 4, 3) != RTK_SUCCESS) {
        RTK_LOGE(LOG_TAG, "Failed to create cst328_work\n\r");
    }
}

static void cst328_deinit_ops(void)
{
    struct cst328_data *cst328 = (struct cst328_data*)cst328_device.priv;
    CHECK_AND_RETURN(cst328);

    cst328->initialized = false;
    rtos_mutex_delete_static(cst328->lock);
    rtos_queue_delete(cst328->work_queue);
    gpio_irq_deinit(&cst328->gpio_irq);
    rtos_mem_free(cst328);
    cst328_device.priv = NULL;
}

static void cst328_enable_ops(void)
{
    struct cst328_data *cst328 = (struct cst328_data*)cst328_device.priv;
    CHECK_AND_RETURN(cst328);

    rtos_mutex_take(cst328->lock, MUTEX_WAIT_TIMEOUT);
    cst328->enabled = true;
    gpio_irq_enable(&cst328->gpio_irq);
    rtos_mutex_give(cst328->lock);
}

static void cst328_disable_ops(void)
{
    struct cst328_data *cst328 = (struct cst328_data*)cst328_device.priv;
    CHECK_AND_RETURN(cst328);

    rtos_mutex_take(cst328->lock, MUTEX_WAIT_TIMEOUT);
    cst328->enabled = false;
    gpio_irq_disable(&cst328->gpio_irq);
    rtos_mutex_give(cst328->lock);
}

static int cst328_ioctl_ops(u32 cmd, void *arg)
{
    return -1;
}

static input_device_ops_t cst328_ops = {
    .init = cst328_init_ops,
    .deinit = cst328_deinit_ops,
    .enable = cst328_enable_ops,
    .disable = cst328_disable_ops,
    .ioctl = cst328_ioctl_ops,
};

static void cst328_register_callback(void (*cb)(input_event_t *))
{
    s_user_cb = cb;
}

input_device_t *input_touch_cst328_init(void)
{
    snprintf(cst328_device.info.name, sizeof(cst328_device.info.name), "cst328");
    cst328_device.info.type = INPUT_DEV_TOUCH;
    cst328_device.info.state = INPUT_DEV_DISABLED;
    cst328_device.info.capabilities = INPUT_CAP_TOUCH;

    cst328_device.ops = cst328_ops;
    cst328_device.register_callback = cst328_register_callback;
    cst328_device.priv = NULL;

    return &cst328_device;
}
