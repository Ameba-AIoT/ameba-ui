/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_ameba_jpeg.h"
#include "lv_draw_ppe.h"
#include "lv_ameba_hal.h"

void lv_ameba_hal_init(void)
{
    lv_ameba_jpeg_init();
    lv_draw_ppe_init();
}
