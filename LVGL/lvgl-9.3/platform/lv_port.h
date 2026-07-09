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

#pragma once

#ifndef AMEBA_UI_LVGL_PLATFORM_LV_PORT_H
#define AMEBA_UI_LVGL_PLATFORM_LV_PORT_H

#include <stdint.h>

typedef void (*lv_port_demo_fn_t)(void);

/**
 * Initialize LVGL, display, hardware acceleration, and filesystem.
 * @param rotation Screen rotation in degrees (0 / 90 / 180 / 270).
 * @return 0 on success, negative error code on failure.
 */
int lv_port_init(uint16_t rotation);

/** Run the LVGL event loop. Blocks until lv_port_deinit() is called. */
void lv_port_run(void);

/** Stop the event loop and release all LVGL resources. */
void lv_port_deinit(void);

/**
 * Create an RTOS task that calls lv_port_init(rotation) -> demo_fn() ->
 * lv_port_run() -> lv_port_deinit() in sequence.
 * Call from app_example() before rtos_sched_start().
 */
void lv_port_run_demo(lv_port_demo_fn_t demo_fn, uint16_t rotation);

#endif /* AMEBA_UI_LVGL_PLATFORM_LV_PORT_H */
