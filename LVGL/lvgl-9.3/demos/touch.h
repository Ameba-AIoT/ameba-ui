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

#ifndef AMEBA_UI_LVGL_TOUCH_H
#define AMEBA_UI_LVGL_TOUCH_H

#include <stdint.h>
#include <stdbool.h>

#include "lvgl.h"

bool touch_init(void);
void touch_read(lv_indev_t * indev, lv_indev_data_t * data);

#endif // AMEBA_UI_LVGL_TOUCH_H