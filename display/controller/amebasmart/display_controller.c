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

#include "os_wrapper.h"
#include "ameba_soc.h"

#include "panel_manager.h"

#include "lcdc_mipi.h"
#include "lcdc_rgb.h"
#include "spi_only.h"

#include "display_controller.h"

#define LOG_TAG "DisplayController"

typedef struct {
    // panel device
    panel_dev_t *panel;
    bool initialized;
} controller_context_t;

static controller_context_t controller_context = {0};

bool controller_init_with_panel(int32_t color_depth, panel_dev_t *panel) {
    if (!panel || !panel->desc) {
        RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Invalid panel device\n");
        return false;
    }

    switch (panel->desc->interface) {
        case PANEL_IF_MIPI_DSI:
            if (!lcdc_mipi_controller_init(color_depth, panel)) {
                return false;
            }
            break;

        case PANEL_IF_SPI:
            if (!spi_only_controller_init(color_depth, panel)) {
                return false;
            }
            break;

        default:
            RTK_LOGS(LOG_TAG, RTK_LOG_ERROR, "Unsupported panel interface: %d\n",
                     panel->desc->interface);
            return false;
    }

    RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "controller initialized with panel: %s\n",
             panel->desc->name);

    controller_context.panel = panel;
    controller_context.initialized = true;

    return true;
}

void controller_do_page_flip(uint8_t *buffer) {
    if (!controller_context.initialized || !controller_context.panel) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "controller not initialized or no panel\n");
        return;
    }

    switch (controller_context.panel->desc->interface) {
        case PANEL_IF_MIPI_DSI:
            lcdc_mipi_do_page_flip(buffer);
            break;

        case PANEL_IF_SPI:
            spi_only_do_page_flip(buffer);
            break;

        default:
            break;
    }
}

void controller_register_vblank_callback(display_driver_callback_t *event) {
    if (!controller_context.initialized || !controller_context.panel) {
        RTK_LOGS(LOG_TAG, RTK_LOG_INFO, "controller not initialized or no panel\n");
        return;
    }

    switch (controller_context.panel->desc->interface) {
        case PANEL_IF_MIPI_DSI:
            lcdc_mipi_register_vblank_callback(event);
            break;

        case PANEL_IF_SPI:
            spi_only_register_vblank_callback(event);
            break;

        default:
            break;
    }
}