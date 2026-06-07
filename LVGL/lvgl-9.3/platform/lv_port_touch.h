/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/** Initialize the touch hardware and input manager. */
void lv_port_touch_init(void);

/**
 * Register touch as an LVGL pointer input device.
 * @return 0 on success, -1 on failure.
 */
int lv_port_touch_register(void);
