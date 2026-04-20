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
#include <string.h>

#include "ameba_soc.h"
#include "os_wrapper.h"

#include "lvgl.h"
#include "lv_ameba_hal.h"
#include "demos/lv_demos.h"
#include "display_mode_setting.h"
#include "input.h"

#define TOUCH_TAG "TOUCH"
#define DEMO_TOUCH "cst328"

static lv_indev_data_t g_lvgl_touch_data = {
    .state = LV_INDEV_STATE_RELEASED,
    .point = {0, 0}
};

static void touch_event_callback(input_event_t *event)
{
    if (event->type != INPUT_EVENT_TOUCH) {
        return;
    }

    g_lvgl_touch_data.point.x = event->data.touch.x;
    g_lvgl_touch_data.point.y = event->data.touch.y;
    g_lvgl_touch_data.state = event->data.touch.pressed ?
                              LV_INDEV_STATE_PRESSED :
                              LV_INDEV_STATE_RELEASED;

    if (event->data.touch.pressed) {
        RTK_LOGD(TOUCH_TAG, "PRESS   (%3d, %3d)\n",
                 event->data.touch.x, event->data.touch.y);
    } else {
        RTK_LOGD(TOUCH_TAG, "RELEASE (%3d, %3d)\n",
                 event->data.touch.x, event->data.touch.y);
    }
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    data->point.x = g_lvgl_touch_data.point.x;
    data->point.y = g_lvgl_touch_data.point.y;
    data->state = g_lvgl_touch_data.state;
}

void touch_init(void)
{
    RTK_LOGI(TOUCH_TAG, "===== Touch Initialization =====\n");

    input_manager_init();
    input_set_global_callback(touch_event_callback);

    input_device_t *ts = input_get_device(DEMO_TOUCH);
    if (!ts) {
        RTK_LOGE(TOUCH_TAG, "ts(%s) device not found!\n", DEMO_TOUCH);
        return;
    }

    RTK_LOGI(TOUCH_TAG, "Found ts(%s) device\n", DEMO_TOUCH);

    if (ts->info.state == INPUT_DEV_DISABLED) {
        if (ts->ops.init) {
            ts->ops.init();
            RTK_LOGI(TOUCH_TAG, "ts(%s) hardware initialized\n", DEMO_TOUCH);
        }
    }

    if (ts->info.state != INPUT_DEV_ENABLED) {
        if (ts->ops.enable) {
            ts->ops.enable();
            ts->info.state = INPUT_DEV_ENABLED;
            RTK_LOGI(TOUCH_TAG, "ts(%s) enabled\n", DEMO_TOUCH);
        }
    }

    RTK_LOGI(TOUCH_TAG, "ts(%s) ready (state: %d)\n", DEMO_TOUCH, ts->info.state);
    RTK_LOGI(TOUCH_TAG, "===== Touch Ready =====\n");
}

int touch_register_to_lvgl(void)
{
    RTK_LOGI(TOUCH_TAG, "Registering touch to LVGL...\n");

    lv_indev_t *indev_touch = lv_indev_create();
    if (!indev_touch) {
        RTK_LOGE(TOUCH_TAG, "Failed to create input device!\n");
        return -1;
    }

    lv_indev_set_type(indev_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touch, touch_read_cb);

    RTK_LOGI(TOUCH_TAG, "Touch registered to LVGL\n");
    return 0;
}
