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

 #include <string.h>

#include "touch.h"
#include "input.h"

static input_touch_data_t g_touch = { 0 };
static input_key_data_t g_key = { 0 };
static input_mouse_data_t g_mouse = { 0 };

void input_event_callback(input_event_t *event)
{
    switch (event->type) {
        case INPUT_EVENT_TOUCH:
            memcpy(&g_touch, &event->data.touch, sizeof(input_touch_data_t));
            break;
        case INPUT_EVENT_KEY:
            memcpy(&g_key, &event->data.key, sizeof(input_key_data_t));
            break;
        case INPUT_EVENT_MOUSE:
            memcpy(&g_mouse, &event->data.mouse, sizeof(input_mouse_data_t));
            break;
        default:
            break;
    }
}

bool touch_init(void)
{
    input_device_t *dev = NULL;
    input_manager_init();
    input_set_global_callback(input_event_callback);
    dev = input_get_device("gt911");

    if (dev) {
        dev->ops.init();
        dev->ops.enable();
    }

    return dev != NULL;
}

void touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = g_touch.x;
    data->point.y = g_touch.y;

    if (g_touch.pressed) {
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}