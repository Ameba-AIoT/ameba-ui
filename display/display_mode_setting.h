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

#ifndef AMEBA_UI_DISPLAY_DISPLAY_MODE_SETTING_H
#define AMEBA_UI_DISPLAY_DISPLAY_MODE_SETTING_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void (*vblank_handler)(void);
} display_mode_callback_t;

bool display_mode_init(int32_t color_depth);
void display_mode_set_callback(display_mode_callback_t *callback);
void display_mode_flip_buffer(uint8_t *buffer);

#endif // AMEBA_UI_DISPLAY_DISPLAY_MODE_SETTING_H
