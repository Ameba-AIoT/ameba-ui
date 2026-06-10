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

#ifndef AMEBA_UI_LVGL_PLATFORM_LV_PORT_TOUCH_H
#define AMEBA_UI_LVGL_PLATFORM_LV_PORT_TOUCH_H

/** Initialize the touch hardware and input manager. */
void lv_port_touch_init(void);

/**
 * Register touch as an LVGL pointer input device.
 * @return 0 on success, -1 on failure.
 */
int lv_port_touch_register(void);

#endif /* AMEBA_UI_LVGL_PLATFORM_LV_PORT_TOUCH_H */
