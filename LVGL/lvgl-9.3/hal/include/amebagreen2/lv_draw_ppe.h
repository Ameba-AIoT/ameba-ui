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

#ifndef UI_LVGL_LV_DRIVERS_LV_DRAW_PPE_H
#define UI_LVGL_LV_DRIVERS_LV_DRAW_PPE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/

/* Enable PPE draw unit */
#ifndef LV_USE_DRAW_PPE
    #define LV_USE_DRAW_PPE     1
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

typedef struct {
    uint32_t cf : 8;            /**< Color format: See `lv_color_format_t`*/
    uint32_t w: 16;
    uint32_t h: 16;
    uint32_t min_x: 16;
    uint32_t min_y: 16;
    uint32_t stride: 16;        /**< Number of bytes in a row*/
    uint32_t color: 32;         /**< Color*/
} lv_draw_ppe_header_t;

typedef struct {
    void* src_buf;
    void* dest_buf;
    lv_draw_ppe_header_t *src_header;
    lv_draw_ppe_header_t *dest_header;
    float scale_x;              /**< can be 16/1, 16/2, 16/3, ..., 16/65535*/
    float scale_y;              /**< can be 16/1, 16/2, 16/3, ..., 16/65535*/
    uint32_t angle;             /**< can be 90/180/270*/
    uint32_t opa;               /**< Color format: See `lv_opa_t`*/
} lv_draw_ppe_configuration_t;

/**
 * @brief Initialize the PPE draw unit
 */
void lv_draw_ppe_init(void);

/**
 * @brief Process the buffer by PPE
 */
void lv_draw_ppe_configure_and_start_transfer(lv_draw_ppe_configuration_t *ppe_draw_conf);

/**
 * @brief Deinitialize the PPE draw unit
 */
void lv_draw_ppe_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* UI_LVGL_LV_DRIVERS_LV_DRAW_PPE_H */