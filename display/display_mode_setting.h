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

static inline int32_t display_mode_get_width(void) {
#if defined(CONFIG_ST7701S_MIPI) && CONFIG_ST7701S_MIPI
    return 480;
#elif defined(CONFIG_ST7701S) && CONFIG_ST7701S
    return 800;
#elif defined(CONFIG_ST7701S_RGB565) && CONFIG_ST7701S_RGB565
    return 480;
#elif defined(CONFIG_ST7701P_RGB) && CONFIG_ST7701P_RGB
    return 480;
#elif defined(CONFIG_HJ3508_12) && CONFIG_HJ3508_12
    return 320;
#elif defined(CONFIG_B1620A) && CONFIG_B1620A
    return 720;
#elif defined(CONFIG_T1720A) && CONFIG_T1720A
    return 800;
#elif defined(CONFIG_JD9165BA) && CONFIG_JD9165BA
    return 1024;
#else
    RTK_LOGE(LOG_TAG, "Unsupported panel:%ld\n");
    return 0;
#endif
}

static inline int32_t display_mode_get_height(void) {
#if defined(CONFIG_ST7701S_MIPI) && CONFIG_ST7701S_MIPI
    return 800;
#elif defined(CONFIG_ST7701S) && CONFIG_ST7701S
    return 480;
#elif defined(CONFIG_ST7701S_RGB565) && CONFIG_ST7701S_RGB565
    return 480;
#elif defined(CONFIG_ST7701P_RGB) && CONFIG_ST7701P_RGB
    return 480;
#elif defined(CONFIG_HJ3508_12) && CONFIG_HJ3508_12
    return 480;
#elif defined(CONFIG_B1620A) && CONFIG_B1620A
    return 720;
#elif defined(CONFIG_T1720A) && CONFIG_T1720A
    return 480;
#elif defined(CONFIG_JD9165BA) && CONFIG_JD9165BA
    return 600;
#else
    RTK_LOGE(LOG_TAG, "Unsupported panel:%ld\n");
    return 0;
#endif
}

#endif // AMEBA_UI_DISPLAY_DISPLAY_MODE_SETTING_H
