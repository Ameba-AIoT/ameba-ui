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

void touch_init(void);
int touch_register_to_lvgl(void);

#endif // AMEBA_UI_LVGL_TOUCH_H