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

#ifndef AMEBA_UI_INPUT_INPUT_H
#define AMEBA_UI_INPUT_INPUT_H

#include <stdint.h>

#define INPUT_CAP_TOUCH         (1 << 0)
#define INPUT_CAP_KEYBOARD      (1 << 1)
#define INPUT_CAP_MOUSE         (1 << 2)
#define INPUT_CAP_MULTI_TOUCH   (1 << 3)

typedef enum {
    INPUT_EVENT_TOUCH,
    INPUT_EVENT_KEY,
    INPUT_EVENT_MOUSE,
} input_event_type_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t pressed;        // 1: touch down, 0: touch up
    uint8_t touch_id;       // Touch point ID (for multi-touch)
} input_touch_data_t;

typedef struct {
    uint16_t key_code;
    uint8_t pressed;        // 1: key down, 0: key up
} input_key_data_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    int8_t wheel_diff;
    uint8_t left_pressed;
    uint8_t right_pressed;
    uint8_t middle_pressed;
} input_mouse_data_t;

typedef struct {
    input_event_type_t type;
    uint32_t timestamp;
    union {
        input_touch_data_t touch;
        input_key_data_t key;
        input_mouse_data_t mouse;
    } data;
} input_event_t;

typedef enum {
    INPUT_DEV_TOUCH,
    INPUT_DEV_KEYPAD,
    INPUT_DEV_MOUSE,
} input_dev_type_t;

typedef enum {
    INPUT_DEV_DISABLED,
    INPUT_DEV_ENABLED,
} input_dev_state_t;

typedef struct {
    char name[32];
    input_dev_type_t type;
    input_dev_state_t state;
    uint32_t capabilities;  // device cap
} input_dev_info_t;

typedef struct input_device_ops {
    void (*init)(void);
    void (*deinit)(void);
    void (*enable)(void);
    void (*disable)(void);
    int (*ioctl)(uint32_t cmd, void *arg);
} input_device_ops_t;

typedef struct input_device {
    input_dev_info_t info;
    input_device_ops_t ops;
    void (*register_callback)(void (*cb)(input_event_t *));
    void *priv;
} input_device_t;

typedef void (*input_event_callback_t)(input_event_t *);

void input_manager_init(void);
int input_device_register(input_device_t *dev);
int input_device_unregister(const char *name);
input_device_t *input_get_device(const char *name);
input_device_t *input_get_device_by_type(input_dev_type_t type);
void input_set_global_callback(input_event_callback_t cb);

#endif // AMEBA_UI_INPUT_INPUT_H