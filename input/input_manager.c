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
#include <string.h>

#include "ameba_soc.h"

#include "input.h"

#define LOG_TAG "INPUT"

#define MAX_INPUT_DEVICES   10

static input_device_t *s_devices[MAX_INPUT_DEVICES];
static int s_device_count = 0;
static input_event_callback_t s_global_callback = NULL;

#ifdef CONFIG_TOUCH_GT911
extern input_device_t *input_touch_gt911_init(void);
#endif
#ifdef CONFIG_TOUCH_CST328
extern input_device_t *input_touch_cst328_init(void);
#endif
#ifdef CONFIG_TOUCH_TDDI
extern input_device_t *input_touch_tddi_init(void);
#endif

static void input_dispatch_event(input_event_t *event) {
    if (s_global_callback) {
        s_global_callback(event);
    }
}

void input_manager_init(void) {
    RTK_LOGI(LOG_TAG, "Initialize input manager\n");
    memset(s_devices, 0, sizeof(s_devices));
    s_device_count = 0;

#ifdef CONFIG_TOUCH_GT911
    input_device_t *gt911_dev = input_touch_gt911_init();
    if (gt911_dev) {
        input_device_register(gt911_dev);
        gt911_dev->register_callback(input_dispatch_event);
    }
#endif

#ifdef CONFIG_TOUCH_CST328
    input_device_t *cst328_dev = input_touch_cst328_init();
    if (cst328_dev) {
        input_device_register(cst328_dev);
        cst328_dev->register_callback(input_dispatch_event);
    }
#endif

#ifdef CONFIG_TOUCH_TDDI
    input_device_t *tddi_dev = input_touch_tddi_init();
    if (tddi_dev) {
        input_device_register(tddi_dev);
        tddi_dev->register_callback(input_dispatch_event);
    }
#endif
}

int input_device_register(input_device_t *dev) {
    if (!dev || s_device_count >= MAX_INPUT_DEVICES) {
        return -1;
    }

    for (int i = 0; i < s_device_count; i++) {
        if (strcmp(s_devices[i]->info.name, dev->info.name) == 0) {
            return -2;  // device already exist
        }
    }

    s_devices[s_device_count++] = dev;

    RTK_LOGI(LOG_TAG, "Device registered: %s (type: %d)\n",
             dev->info.name, dev->info.type);

    return 0;
}

int input_device_unregister(const char *name) {
    for (int i = 0; i < s_device_count; i++) {
        if (strcmp(s_devices[i]->info.name, name) == 0) {
            for (int j = i; j < s_device_count - 1; j++) {
                s_devices[j] = s_devices[j + 1];
            }
            s_device_count--;
            RTK_LOGI(LOG_TAG, "Device unregistered: %s\n", name);
            return 0;
        }
    }
    return -1;
}

input_device_t *input_get_device(const char *name) {
    for (int i = 0; i < s_device_count; i++) {
        if (strcmp(s_devices[i]->info.name, name) == 0) {
            return s_devices[i];
        }
    }
    return NULL;
}

input_device_t *input_get_device_by_type(input_dev_type_t type) {
    for (int i = 0; i < s_device_count; i++) {
        if (s_devices[i]->info.type == type) {
            return s_devices[i];
        }
    }
    return NULL;
}

void input_set_global_callback(input_event_callback_t cb) {
    s_global_callback = cb;
}
