/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ameba_soc.h"
#include "os_wrapper.h"

#include "lvgl.h"
#include "input.h"
#include "lv_port_touch.h"

#define LOG_TAG     "LV-Touch"
#define TOUCH_DEV   "cst328"

static lv_indev_data_t s_touch_data = {
    .state = LV_INDEV_STATE_RELEASED,
    .point = {0, 0}
};

static void touch_event_cb(input_event_t *event)
{
    if (event->type != INPUT_EVENT_TOUCH) {
        return;
    }

    s_touch_data.point.x = event->data.touch.x;
    s_touch_data.point.y = event->data.touch.y;
    s_touch_data.state   = event->data.touch.pressed
                           ? LV_INDEV_STATE_PRESSED
                           : LV_INDEV_STATE_RELEASED;
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = s_touch_data.point.x;
    data->point.y = s_touch_data.point.y;
    data->state   = s_touch_data.state;
}

void lv_port_touch_init(void)
{
    RTK_LOGI(LOG_TAG, "Touch init (%s)\n", TOUCH_DEV);

    input_manager_init();
    input_set_global_callback(touch_event_cb);

    input_device_t *ts = input_get_device(TOUCH_DEV);
    RTK_LOGI(LOG_TAG, "ts=%p\n", ts);
    if (!ts) {
        RTK_LOGE(LOG_TAG, "Device '%s' not found\n", TOUCH_DEV);
        return;
    }

    RTK_LOGI(LOG_TAG, "ts->ops.init=%p enable=%p state=%d\n",
             ts->ops.init, ts->ops.enable, ts->info.state);

    if (ts->info.state == INPUT_DEV_DISABLED && ts->ops.init) {
        RTK_LOGI(LOG_TAG, "calling ts->ops.init\n");
        ts->ops.init();
        RTK_LOGI(LOG_TAG, "ts->ops.init done\n");
    }

    if (ts->info.state != INPUT_DEV_ENABLED && ts->ops.enable) {
        RTK_LOGI(LOG_TAG, "calling ts->ops.enable\n");
        ts->ops.enable();
        ts->info.state = INPUT_DEV_ENABLED;
        RTK_LOGI(LOG_TAG, "ts->ops.enable done\n");
    }

    RTK_LOGI(LOG_TAG, "Touch ready\n");
}

int lv_port_touch_register(void)
{
    lv_indev_t *indev = lv_indev_create();
    if (!indev) {
        RTK_LOGE(LOG_TAG, "Failed to create LVGL input device\n");
        return -1;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    RTK_LOGI(LOG_TAG, "Touch registered to LVGL\n");
    return 0;
}
