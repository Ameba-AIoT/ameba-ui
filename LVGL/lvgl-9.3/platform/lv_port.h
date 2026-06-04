/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

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
